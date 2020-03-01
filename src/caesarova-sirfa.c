#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>


#define initial_capacity 64
#define growth_ratio 141/100

int minimum(int a, int b, int c) {
	if (a <= b)
		return a < c ? a : c;
	else
		return b < c ? b : c;
}

/*Reads one line from stdin into dynamically allocated buffer. Caller is responsible for memory freeing. */
char* readline() {

	int size = initial_capacity;
	char* line = (char*)malloc(size);

	for (int index = 0; ; ++index) {

		if (index + 1 == size) {
			size = size * growth_ratio;
			/*We're not checking the returned value here, because the assignment DOES NOT state, 
			what shall be done if we run out of memeory. */
			line = realloc(line, size); 
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
	int similarities = 0;
	for (; *a; ++a, ++b)
		if (*a == *b)
			++similarities;
	return similarities;
}

/*						ATTENTION! 
For the purpose of making encoding and decoding functionality easily legible, 
I've defined a term "canonical form" of char. Since we only assume the letters 
of english alphabet with lowercase preceding uppercase, shifing characters would reuire
nasty ammount of ifs. When normal char is canonicalized, it is mapped to interval [0, 2* alphabet_size)
with lowercase letters preceding uppercase. (E.g. letter 'B' should have value 27 in canonical form).

Canonical form is used to perform caesar's cipher calculations and final value is converted back to normal char. */

char char_to_canonical(char c) {
	char const result = islower(c) ? c - 'a' : c - 'A' + alphabet_size;
	//printf("Canonicalize char %c => %d\n", c, result);
	return result;
}

char char_from_canonical(char value) {
	char const result = value < alphabet_size ?  value + 'a' : value - alphabet_size + 'A';
	//printf("Decode value %d => char %c\n", value, result);
	return result;
}

void encode_string_Caesar(char* string, int offset) {
	for (; *string; ++string) {
		int const new = char_to_canonical(*string);
		int const new_shifted = new + offset;
		char const result = char_from_canonical(new_shifted % (2 * alphabet_size));
		//printf("Character %c, value %d, after %d, result %c\n", *string, new, new_shifted, result);
		*string = result;
	}
}

void solve_simple(char* encoded, char const* const intercepted) {

	char* const tmp = (char*)malloc(strlen(encoded) + 1);
	strcpy(tmp, encoded);

	int best = 0, max_similarities = count_similarities(tmp, intercepted);

	for (int i = 1; i < 2 * alphabet_size; ++i) {

		encode_string_Caesar(tmp, 1);
		int const similarities = count_similarities(tmp, intercepted);
		//printf("Strings %s and %s have %d similarities\n", tmp, intercpeted, similarities);
		if (similarities > max_similarities) {
			max_similarities = similarities;
			best = i;
		}
	}
	free(tmp);

	encode_string_Caesar(encoded, best);
	printf("%s\n", encoded);
}

bool has_valid_alphabet(char const* string) {

	for (; *string; ++string)
		if (!isalpha(*string))
			return false;
	return true;
}

int levenstein_distance(char const* a, char const* b) {

	int const a_len = strlen(a), b_len = strlen(b);
	
	int** const matrix = (int**)malloc(sizeof(int*) * (a_len + 1));
	for (int row = 0; row <= a_len; ++row) 
		matrix[row] = (int*)malloc(sizeof(int) * (b_len + 1));

	for (int row = 0; row <= a_len; ++row)
		matrix[row][0] = row;
	for (int col = 0; col <= b_len; ++col)
		matrix[0][col] = col;

	for (int col = 1; col <= b_len; ++col)
		for (int row = 1; row <= a_len; ++row) {
			int const cost = a[row - 1] == b[col - 1] ? 0 : 1;
			matrix[row][col] = minimum(matrix[row - 1][col] + 1, matrix[row][col - 1] + 1, matrix[row - 1][col - 1] + cost);
		}

	int const result = matrix[a_len][b_len];

	for (int row = 0; row <= a_len; ++row)
		free(matrix[row]);
	free(matrix);

	return result;
}

void solve_optional(char* encoded, char const* const intercpeted) {

	char* const tmp = (char*)malloc(strlen(encoded) + 1);
	strcpy(tmp, encoded);

	int best = 0, min_distance = levenstein_distance(tmp, intercpeted);

	for (int i = 1; i < 2 * alphabet_size; ++i) {

		encode_string_Caesar(tmp, 1);
		int const distance = levenstein_distance(tmp, intercpeted);
		//printf("Strings %s and %s have distance %d\n", tmp, intercpeted, distance);

		if (distance < min_distance) {
			min_distance = distance;
			best = i;
		}
	}
	free(tmp);

	encode_string_Caesar(encoded, best);
	printf("%s\n", encoded);

}

int main(int argc, char ** argv) {

	char* encoded = readline();
	char* intercepted = readline();

	if (!has_valid_alphabet(encoded) || !has_valid_alphabet(intercepted)) {
		fprintf(stderr, "Error: Chybny vstup!\n");
		free(encoded);
		free(intercepted);
		return 100;
	}

	if (argc == 2 && strcmp(argv[1], "-prg-optional") == 0)
		solve_optional(encoded, intercepted);
	else {
		//simple case - we have to check that lines have the same length
		if (strlen(encoded) != strlen(intercepted)) {
			fprintf(stderr, "Error: Chybna delka vstupu!\n");
			free(encoded);
			free(intercepted);
			return 101;
		}

		solve_simple(encoded, intercepted);
	}

	free(intercepted);
	free(encoded);
	return 0;
}
