#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#define initial_capacity 64
#define growth_ratio 141/100

enum exit_code {
	exit_ok = EXIT_SUCCESS,
	//length of intercepted and encoded strings are not equal in simple assignment
	exit_length_mismatch = 101,
	//some of the input strings contain non alphabetical characters
	exit_invalid_alphabet = 100,
	//memory allocation error happened somewhere
	exit_bad_alloc = 666

};


int minimum(int a, int b, int c) {
	if (a <= b) {
		return a < c ? a : c;
	}
	else {
		return b < c ? b : c;
	}
}

/*Reads one line from stdin into dynamically allocated buffer. Caller is responsible for freeing the allocated memory. */
char* readline() {

	int size = initial_capacity;
	char* line = (char*)malloc(size);
	if (!line) {
		return NULL;
	}

	for (int index = 0; ; ++index) {

		if (index + 1 == size) {
			size = size * growth_ratio;
			/* The assignment DOES NOT state, what shall be done if we run out of memory. */
			char* const new_line = (char*)realloc(line, size);
			if (!new_line) {
				free(line);
				return NULL;
			}
			line = new_line;
		}

		line[index] = getchar();
		if (line[index] == '\n') {
			line[index] = '\0';
			return line;
		}
	}
}

int const alphabet_size = 'z' - 'a' + 1;

/* Counts number of characters that appear in both a and b on the same offset.*/
int count_similarities(char const* a, char const* b) {
	assert(strlen(a) == strlen(b));
	int similarities = 0;
	for (; *a; ++a, ++b) {
		if (*a == *b) {
			++similarities;
		}
	}
	return similarities;
}

/******************************************************
				IMPORTANT
*******************************************************
!
For the purpose of making encoding and decoding functionality easily legible,
I've defined a term "canonical form" of char. Since we only assume the letters
of english alphabet with lowercase preceding uppercase, shifing characters would reuire
nasty ammount of ifs. When normal char is canonicalized, it is mapped to interval [0, 2* alphabet_size)
with lowercase letters preceding uppercase. (E.g. letter 'B' should have value 27 in canonical form).

Canonical form is used to perform caesar's cipher calculations and final value is converted back to normal char. */

int char_to_canonical(char c) {
	assert(islower(c) || isupper(c));

	return islower(c) ? c - 'a' : c - 'A' + alphabet_size;
}

char canonical_to_char(int value) {
	assert(value >= 0 && value < alphabet_size * 2);

	return value < alphabet_size ? value + 'a' : value - alphabet_size + 'A';
}

/* Perform Caesar's cipher shiftign by given 'shift' on given string INPLACE,
	i.e. the pointed to string is modified directly. */
void encode_string_Caesar(char* string, int shift) {
	for (; *string; ++string) {
		int const new = char_to_canonical(*string);
		int const new_shifted = (new + shift) % (2 * alphabet_size);
		char const result = canonical_to_char(new_shifted);
		*string = result;
	}
}

/* Solve simple assignment - given both encoded string and partially incorrectly intercepted
	original message, find an integer best_shift, for which, when used as argument of Caesar's cipher,
	decoded string shares the most letters with the intercepted one.*/
void solve_simple(char* const encoded, char const* const intercepted) {

	/* Copy of encoded string, on which all modifications will take place. */
	char* const modifiable_copy = (char*)malloc(strlen(encoded) + 1);
	if (!modifiable_copy) {
		fprintf(stderr, "Error: Could not allocate memory in %s.\n",  __FUNCTION__ );
		return;
	}

	strcpy(modifiable_copy, encoded);

	//We assume that no shift is the best option so far
	int best_shift = 0, max_similarities = count_similarities(modifiable_copy, intercepted);

	//And cycle through all other possible shifts searching for the one, which produces the most similarities
	for (int i = 1; i < 2 * alphabet_size; ++i) {

		encode_string_Caesar(modifiable_copy, 1);
		int const similarities = count_similarities(modifiable_copy, intercepted);

		if (similarities > max_similarities) {
			max_similarities = similarities;
			best_shift = i;
		}
	}
	free(modifiable_copy);

	//finally the best found solution is applied and printed
	encode_string_Caesar(encoded, best_shift);
	printf("%s\n", encoded);
}

/* Checks all characters in a string and returns true iff none of them falls outside valid alphabet.
	Reminder: valid alphabet consists of lowercase and uppercase letters of latin alphabet with lowercase preceding upper.*/
bool has_valid_alphabet(char const* string) {

	for (; *string; ++string) {
		if (!isalpha(*string)) {
			return false;
		}
	}
	return true;
}

void swap(int** a, int** b) {
	int* tmp = *a;
	*a = *b;
	*b = tmp;
}

/* Compute levenstein distance of two sttrings.

	Algorithm adapted from dr. Genyk-Berezovskyj's seminar at FEE CTU on dynamic programming,
	it replaces full matrix of Wagner–Fischer by only two columns that are required at each time.

	Return value -1 indicates memory allocation error.*/
int compute_levenstein_distance(char const* a, char const* b) {

	int const a_len = strlen(a), b_len = strlen(b);
	int const height = a_len + 1;

	/* The algorithm traverses virtual matrix from top to bottom by columns, it is therefore sufficient
	to keep two columns in memory at any point in time. One column is used as source of data, the other
	is being filled by new data. At the end of each iteration of the algorithm, they are swapped. */
	int* previous_column = (int*)malloc(sizeof(int) * height),
		* this_column = (int*)malloc(sizeof(int) * height);

	if (!previous_column || !this_column) {
		free(previous_column);
		free(this_column);
		fprintf(stderr, "Error: Could not allocate memory in %s.\n", __FUNCTION__);
		return -1;
	}

	/* First column contains growing sequence of integers, because there are k differences
	between an empty string and a string of length k. */
	for (int row = 0; row < height; ++row) {
		this_column[row] = row;
	}

	/* Construct remaining columns iteratively. */
	for (int column = 1; column <= b_len; ++column) {
		swap(&this_column, &previous_column);

		//First row is similar to first column. k distances between "" and string of length k
		this_column[0] = column;
		for (int row = 1; row < height; ++row) {
			int const cost = a[row - 1] == b[column - 1] ? 0 : 1;
			this_column[row] = minimum(this_column[row - 1] + 1,
				previous_column[row] + 1,
				previous_column[row - 1] + cost);
		}
	}

	int const result = this_column[a_len];

	free(previous_column);
	free(this_column);

	return result;
}

/* Solves optional form of asignment: Missheard and/or missing letters may occur in the input.
	The optimal solution is now that one, which has the lowest levenstein distance of
	intercepted (possibly missheard) original message and decoded string. */
void solve_optional(char* encoded, char const* const intercpeted) {

	char* const modifiable_copy = (char*)malloc(strlen(encoded) + 1);
	if (!modifiable_copy) {
		fprintf(stderr, "Error: Could not allocate memory in %s.\n", __FUNCTION__);
		return;
	}

	strcpy(modifiable_copy, encoded);

	//We assume that no shift is the best solution and then cycle though all others
	int best = 0, min_distance = compute_levenstein_distance(modifiable_copy, intercpeted);
	if (min_distance == -1) {
		free(modifiable_copy);
		return;
	}

	for (int i = 1; i < 2 * alphabet_size; ++i) {

		encode_string_Caesar(modifiable_copy, 1);
		int const distance = compute_levenstein_distance(modifiable_copy, intercpeted);
		if (distance == -1) {
			free(modifiable_copy);
			return;
		}

		if (distance < min_distance) {
			min_distance = distance;
			best = i;
		}
	}
	free(modifiable_copy);

	encode_string_Caesar(encoded, best);
	printf("%s\n", encoded);
}

int main(int argc, char** argv) {

	//Assignment guarantees that two lines of input will be available
	char* encoded = readline();
	char* intercepted = readline();
	if (!encoded || !intercepted) {
		free(encoded);
		free(intercepted);
		fprintf(stderr, "Error: Could not allocate memory.\n");
		return exit_bad_alloc;
	}

	//Check whether both strings only contain valid characters. Otherwise print error and exit
	if (!has_valid_alphabet(encoded) || !has_valid_alphabet(intercepted)) {
		fprintf(stderr, "Error: Chybny vstup!\n");
		free(encoded);
		free(intercepted);
		return exit_invalid_alphabet;
	}

	if (argc == 2 && strcmp(argv[1], "-prg-optional") == 0) {
		solve_optional(encoded, intercepted);
	}
	else {
		//simple case - we have to check that lines have the same length
		if (strlen(encoded) != strlen(intercepted)) {
			fprintf(stderr, "Error: Chybna delka vstupu!\n");
			free(encoded);
			free(intercepted);
			return exit_length_mismatch;
		}

		solve_simple(encoded, intercepted);
	}

	free(intercepted);
	free(encoded);
	return exit_ok;
}
