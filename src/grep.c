#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>


/* Global variables storing the program's state.*/
#define INITIAL_LINE_CAPACITY 1024
#define GROWTH_RATIO 141 / 100
#define color_begin "\x1b[01;31m\x1b[K"
#define color_end "\x1b[m\x1b[K"
int const REGEX_REJECT = -1;


FILE* input_stream;
bool colorize_output = false;
bool use_regex = false;
const char* pattern = NULL;
bool matched_some = false;

const char* input_stream_name = "stdin";
bool print_line_numbers = false;



/* Structure holding information about the size and location of dynamically growing string. */
typedef struct dynamic_string {
	int capacity;
	char* ptr;
} dynamic_string;

void dynamic_string_grow(dynamic_string* const line) {
	line->capacity = line->capacity * GROWTH_RATIO;
	char* const reallocated = realloc(line->ptr, line->capacity);
	if (!reallocated) {
		fprintf(stderr, "%sError:%s Unable to reallocate %d bytes of memory. Halting.\n", color_begin, color_end, line->capacity);
		free(line->ptr);
		exit(EXIT_FAILURE);
	}
	line->ptr = reallocated;
}

dynamic_string input_line;

/* Dumb function testing identity of strings, because strcmp is prohibited for no reason.
 Returns true iff len(a) == len(b) and both strings consist of the same sequence of chars. */
bool strings_same(char const* a, char const* b) {

	//While both strings have a letter and they are the same, continue looping
	for (; *a && *b; ++a, ++b)
		if (*a != *b)
			return false;

	//if one of the strings has not ended yet, their lenghts are not equal
	return !*a && !*b;
}

/* Dumb function counting the lenght of string, because length is prohibited... */
int length(char const* a) {
	int len = 0;
	for (; *a; ++a, ++len);
	return len;
}

/* Parse command line arguments and set global state accordingly. If file is supplied, try to open it.
 Prints error message and exits the program if any error occures while trying to open the given file. */
void set_global_state(char** argument) {

	input_stream = stdin;

	for (; *argument && **argument == '-'; ++argument)
		if (strings_same(*argument, "-E"))
			use_regex = true;
		else if (strings_same(*argument, "--color=always"))
			colorize_output = true;
		else if (strings_same(*argument, "-n"))
			print_line_numbers = true;
		else { //unknown parameter name
			fprintf(stderr, "%sError:%s Unknown parameter \"%s\".\n", color_begin, color_end, *argument);
			exit(EXIT_FAILURE);
		}

	if (!*argument) {
		fprintf(stderr, "%sError:%s Missing parameter PATTERN.\n", color_begin, color_end);
		exit(EXIT_FAILURE);
	}

	pattern = *argument;
	++argument; //move to the next OPTIONAL parameter specifying the input file
	if (*argument) {
		input_stream_name = *argument;
		input_stream = fopen(input_stream_name, "r");
		if (!input_stream) {
			fprintf(stderr, "%sError:%s Unable to open file \"%s\".\n", color_begin, color_end, input_stream_name);
			exit(EXIT_FAILURE);
		}
	}
	//Allocate internal buffer only iff no error occured 
	input_line = (dynamic_string){ .capacity = INITIAL_LINE_CAPACITY, .ptr = malloc(INITIAL_LINE_CAPACITY) };
}

void read_input(dynamic_string *const line) {

	int index = 0;
	for (; (line->ptr[index] = fgetc(input_stream)) && line->ptr[index] != '\n' && !feof(input_stream); ++index)
		if (index + 1 == line->capacity) 
			dynamic_string_grow(line);
	line->ptr[index] = '\0';
}

typedef struct regex_state {

	int id;
	char accepted_letter;
	int state_on_match, state_otherwise;

} regex_state;



typedef struct regex_state_machine {

	regex_state* states;
	int capacity, size;
	int state_start, state_current, state_finish;
	dynamic_string matched_string;

} regex_state_machine;

void regex_init(regex_state_machine* const rsm, int capacity) {
	rsm->capacity = capacity;
	rsm->states = malloc(rsm->capacity * sizeof(regex_state));
	rsm->matched_string = (dynamic_string){ .capacity = rsm->capacity + 10, .ptr = malloc(rsm->capacity + 10) };
	if (!rsm->states || !rsm->matched_string.ptr) {
		fprintf(stderr, "%sError:%s Unable to allocate %ld bytes of memory. Halting.\n", 
			color_begin, color_end, rsm->capacity * sizeof(regex_state));
		if (rsm->states)
			free(rsm->states);
		if (rsm->matched_string.ptr)
			free(rsm->matched_string.ptr);
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < rsm->capacity; ++i) {
		regex_state* const state = &rsm->states[i];
		state->id = i;
		state->accepted_letter = '\0';
		state->state_on_match = REGEX_REJECT;
		state->state_otherwise = REGEX_REJECT;
	}
}

/*Construct a state machine for regex parsing. Some memory storage is preallocated based
on an educated guess. Linked list of states is then constructed, taking the global settings in mind. */
regex_state_machine regex_compile(const char* regex) {

	regex_state_machine rsm;
	regex_init(&rsm, length(regex) + 10);


	rsm.size = 1;

	//Set up the starting state
	rsm.state_start = 0;
	int current_state = 0;

	for (; *regex; ++regex, ++rsm.size, ++current_state) {
		regex_state* const current = &rsm.states[current_state];

		current->accepted_letter = regex[0];
		current->state_on_match = current_state + 1;
	
		//If regular expresions are disabled, all characters in the pattern have to be interpreted literally
		if (!use_regex) 
			continue;
		if (regex[1] == '?') { 
			current->state_otherwise = current_state + 1;
			regex++;
		}
		else if (regex[1] == '+') {

			regex_state* const repetitive = &rsm.states[current_state + 1];
			repetitive->accepted_letter = regex[0];
			repetitive->state_on_match = repetitive->id;

			repetitive->state_otherwise = current_state + 2;
			++current_state;
			++rsm.size;
			++regex;
		}
		else if (regex[1] == '*') {
			current->state_on_match = current_state;
			current->state_otherwise = current_state + 1;

			regex++;
		}
	}

	rsm.state_finish = rsm.size - 1;
	return rsm;

}

/* Returns index of the next state when matching a string against the regex. */
int regex_state_get_next(regex_state* const state, char next_char) {
	return state->accepted_letter == next_char ? state->state_on_match : state->state_otherwise;
}

/* Returns true iff given regex state machine just successfully matched a string. */
bool regex_match_success(regex_state_machine const* rsm) {
	return rsm->state_current == rsm->state_finish;
}

/* Returns true iff given regex state machine just failed to match a string. */
bool regex_match_fail(regex_state_machine const* rsm) {
	return rsm->state_current == REGEX_REJECT;
}

/* Returns true iff given regex state machine finished matching - it either succeeded or failed. */
bool regex_match_ended(regex_state_machine const* rsm) {
	return regex_match_fail(rsm) || regex_match_success(rsm);
}

/* Prepare regex state machine for matching. */
void regex_match_start(regex_state_machine* rsm) {
	rsm->state_current = rsm->state_start;
}

/* State machine transfer function. Chooses the new state for given state machine 
   based on current state and supplied character. */
void regex_transfer_function(regex_state_machine* rsm, char next_char) {
	regex_state * const state = &rsm->states[rsm->state_current];
	rsm->state_current = regex_state_get_next(state, next_char);

	/*Here we have to compensate a tradeoff for simple implementation of regex_compile.
	  Because of the way optional matching (c?) is now implemented, it is sometimes 
	  desired to apply transfer function twice. 
	  For the regex 'a?b' and string 'b' the first transfer recognizes, that a was not supplied.
	  A second transfer is then required to consume the letter b.
	  
	  This must be performed every time the first transfer doesn't match a letter whilst not setting the regex to fail.*/
	if (!regex_match_fail(rsm) && state->accepted_letter != next_char)
		rsm->state_current = regex_state_get_next(&rsm->states[rsm->state_current], next_char);
}

/* Tries to match a string against given regex from the beginning. Returns true iff the match was successful. 
   All matched characters are copied to internal buffer in regex state machine. */
bool regex_match_exact(regex_state_machine* const rsm, char const* string) {

	int write_index = 0;
	for (regex_match_start(rsm); *string && !regex_match_ended(rsm); ++string, ++write_index) {
		regex_transfer_function(rsm, *string);
		rsm->matched_string.ptr[write_index] = *string;
		if (write_index + 1 == rsm->matched_string.capacity)
			dynamic_string_grow(&rsm->matched_string);
	}
	/*write_index now points one past the last character, that was copied. 
	  If the matching did not succeed, it it secessary to overwrite the last char by NT.
	  If it succeeded, the NT is appended. */
	if (!regex_match_success(rsm))
		--write_index;
	rsm->matched_string.ptr[write_index] = '\0';
	return regex_match_success(rsm);
}

/* Returns true iff 'line' contains any substring matched by regex. */
bool regex_matches_some(regex_state_machine* const rsm, char const* line) {
	for (; *line; ++line)
		if (regex_match_exact(rsm, line))
			return true;
	return false;
}

/* Prints given line whilst colorizing substrings matching the regex. */
void print_colorized(regex_state_machine * const rsm, char const * line) {

	for (; *line; ) 
		if (regex_match_exact(rsm, line)) {
			printf("%s%s%s", color_begin, rsm->matched_string.ptr, color_end);
			line += length(rsm->matched_string.ptr);
		}
		else 
			printf("%c", *line++);

	printf("\n");
}


/* Cleanup resources allocated by the program. */
void cleanup(regex_state_machine *const rsm) {

	free(rsm->states);
	free(rsm->matched_string.ptr);
	free(input_line.ptr);

	if (input_stream != stdin)
		fclose(input_stream);
}

int main(int argc, char** argv) {

	set_global_state(argv + 1);

	regex_state_machine rsm = regex_compile(pattern);

	for (int line_number = 1; !feof(input_stream); ++line_number) {
	
		read_input(&input_line);
		if (regex_matches_some(&rsm, input_line.ptr)) {

			if (print_line_numbers) 
				printf("%s:%d:", input_stream_name, line_number);

			if (colorize_output)
				print_colorized(&rsm, input_line.ptr);
			else
				printf("%s\n", input_line.ptr);
			matched_some = true;
		}
	}

	cleanup(&rsm);

	return matched_some ? EXIT_SUCCESS : 1;
}
