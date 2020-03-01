#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>


#define BONUS

typedef struct matrix {

	int height, width;
	int ** data;
	bool dead;

#ifdef BONUS
	char name;
#endif
} matrix;

bool input_error = false;

int** allocate(int height, int width) {
	int ** const result = malloc(sizeof(int*) * height);
	for (int y = 0;y < height; ++y)
		result[y] = malloc(sizeof(int) * width);
	return result;
}

#ifndef BONUS
matrix read_matrix() {
	matrix m = (matrix) {.dead = false };
	if (scanf("%d %d", &m.height, &m.width) != 2) {
		input_error = true;
		return m;
	}
	m.data = allocate(m.height, m.width);
	for (int row = 0; row < m.height; ++row) 
		for (int col = 0; col < m.width; ++col)
			if (scanf("%d", &m.data[row][col]) != 1) {
				input_error = true;
				return m;
			}
	scanf(" ");
	return m;
}
#else
matrix read_matrix_bonus() {
	matrix m = (matrix) {.dead = false};

	assert(getchar() == '=' && getchar() == '[');

	int max_width = 100;
	int* row = malloc(sizeof(int) * max_width);

	int width = 0;
	for (int value; scanf("%d", &value) == 1; ++width) { //Loops until the ';' is hit
		if (width == max_width) {
			max_width *= 2;
			row = realloc(row, sizeof(int) * max_width);
		}
		row[width] = value;
	}
	m.width = width;

	int max_height = 100;
	m.data = malloc(sizeof(int*) * max_height);
	m.data[0] = row;

	int height = 1;
	if (getchar() != ']') {
		for (int value; ; ++height) {
			if (height == max_height) {
				max_height *= 2;
				m.data = realloc(m.data, sizeof(int*) * max_height);
			}
			m.data[height] = malloc(sizeof(int) * width);
			for (int i = 0; i < width; ++i) {
				scanf("%d", &value);
				m.data[height][i] = value;
			}
			if (getchar() == ']')
				break;
		}
		++height;
	}
	m.height = height;
	assert(getchar() == '\n'); // consumes newline
	return m;
}

#endif

matrix multiply(matrix left, matrix right) {
	matrix result = (matrix){.height = left.height, .width = right.width, .dead = false };
	result.data = allocate(result.height, result.width);

	for (int row = 0;row< result.height; ++row)
		for (int col = 0; col < result.width; ++col) {
			int sum = 0;

			for (int index = 0; index<left.width; ++index)
				sum += left.data[row][index] * right.data[index][col];

			result.data[row][col] = sum;
		}

	return result;
}

void matrix_add(matrix* const left, matrix* const right) {
	assert(left->height == right->height);
	assert(left->width == right->width);
	for (int row = 0; row < left->height; ++row)
		for (int col = 0; col < left->width; ++col)
			left->data[row][col] += right->data[row][col];
}

void matrix_destroy(matrix m) {
	for (int y =0;y < m.height; ++y)
		free(m.data[y]);
	free(m.data);
}

void matrix_negate(matrix* const m) {
	for (int row = 0; row <m->height; ++row)
		for (int col = 0; col < m->width; ++col)
			m->data[row][col] = -m->data[row][col];
}

matrix matrix_copy(matrix * const m) {
	matrix result = (matrix){.height = m->height, .width = m->width, .dead = m->dead, .name = m->name};
	result.data = allocate(result.height, result.width);
	for (int row = 0; row < result.height; ++row)
		for (int col = 0; col < result.width; ++col)
			result.data[row][col] = m->data[row][col];
	return result;
}

void fatal_error(int matrix_count, matrix* matrices) {
		for (int i = 0;i < matrix_count;++i)
			matrix_destroy(matrices[i]);
		fprintf(stderr, "Error: Chybny vstup!\n");
		exit(100);
}

#ifndef BONUS
void print_result(matrix * const result) {
	printf("%d %d\n", result->height, result->width);
	for (int row = 0; row < result->height; ++row) {
		for (int col = 0; col < result->width - 1; ++col)
			printf("%d ", result->data[row][col]);
		printf("%d\n", result->data[row][result->width - 1]);
	}
}
#else
void print_result(matrix * const result) {
	putchar('[');
	for (int row = 0; row < result->height - 1; ++row) {
		for (int col = 0; col < result->width - 1; ++col)
			printf("%d ", result->data[row][col]);
		printf("%d; ", result->data[row][result->width - 1]);
	}
	for (int col = 0; col < result->width - 1; ++col)
		printf("%d ", result->data[result->height - 1][col]);
	printf("%d]\n", result->data[result->height - 1][result->width - 1]);
}
#endif

int main() {
#ifndef BONUS

	matrix matrices[100];
	char operators[100] = { 0 };

	matrices[0] = read_matrix();

	int matrix_count = 1;
	for (; !feof(stdin) && !input_error; ++matrix_count) {
		int read = getchar();
		if (read == EOF)
			break;
		operators[matrix_count - 1] = read;
		if (read != '-' && read != '*' && read != '+') {
			input_error = true;
			break;
		}
		matrices[matrix_count] = read_matrix();
	}

	if (input_error) 
		fatal_error(matrix_count, matrices);

	for (int i = 0; i < matrix_count - 1; ++i) 
		if (operators[i] == '*') {
			operators[i] = ' ';
			if (matrices[i + 1].height != matrices[i].width) {
				input_error = true;
				break;
			}
			matrix result = multiply(matrices[i], matrices[i + 1]);
			matrix_destroy(matrices[i]);
			matrix_destroy(matrices[i + 1]);
			matrices[i + 1] = result;
			matrices[i].dead = true;
		}

	if (input_error) 
		fatal_error(matrix_count, matrices);

	int write = 0;
	for (int read = 0; read < matrix_count; ++read)
		if (!matrices[read].dead) {
			operators[write] = operators[read];
			matrices[write] = matrices[read];
			++write;
		}

	matrix_count = write;
	for (int i = 0; i < matrix_count - 1; ++i)
		if (operators[i] == '-')
			matrix_negate(&matrices[i + 1]);

	matrix *const result = &matrices[0];

	for (int i = 1; i < matrix_count; ++i)
		if (matrices[i].height != result->height || matrices[i].width != result->width) {
			input_error  = true;
			break;
		}
		else
			for (int y = 0; y < result->height; ++y)
				for (int x = 0; x < result->width; ++x)
					result->data[y][x] += matrices[i].data[y][x];

	if (input_error) 
		fatal_error(matrix_count, matrices);

	print_result(result);
	

	for (int i = 0; i <matrix_count; ++i)
		matrix_destroy(matrices[i]);
#else

	matrix matrices[26];
	memset(matrices, 0, sizeof(matrices));

	for (char name; (name = getchar()) != '\n'; ) {
		matrix * const m = &matrices[name - 'A'];
		*m = read_matrix_bonus();
		m->name = name;
	}


	matrix result = matrix_copy(&matrices[getchar() - 'A']);
	for (int op = getchar(); op == '*' || op == '+' || op == '-' ; op = getchar()) {
		if (op == '*') {
			matrix new_result = multiply(result, matrices[getchar() - 'A']);
			matrix_destroy(result);
			result = new_result;
			continue;
		}
		else {
			matrix right_operand = matrix_copy(&matrices[getchar() - 'A']);
			for (int second_op = getchar(); ;second_op = getchar()) {
				if (second_op != '*') {
					ungetc(second_op, stdin);
					break;
				}
				matrix new_right_operand = multiply(right_operand, matrices[getchar() - 'A']);
				matrix_destroy(right_operand);
				right_operand = new_right_operand;
			}
			if (op == '-')
				matrix_negate(&right_operand);
			matrix_add(&result, &right_operand);
			matrix_destroy(right_operand);
		}
	}

	print_result(&result);
	matrix_destroy(result);
	   	 
	for (int i = 0; i < 26; ++i)
		matrix_destroy(matrices[i]);


#endif
	return EXIT_SUCCESS;
}
