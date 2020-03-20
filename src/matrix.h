
/* Matrices - HW04 of the course B3B36PRG at FEE CTU Prague
 *
 * Written by Vojtech Michal
*/
#ifndef MATRICES_H
#define MATRICES_H

/*Define the underlying type for arithmetic operations. 
  In this assignment, all numbers shall fit in 32bit signed int. */
typedef int underlying;

typedef struct matrix {

	int height, width;
	underlying** data;
	bool dead;

#ifdef MATRIX_BONUS
	char name;
#endif
} matrix;

void matrix_init(matrix* const m) {
	m->height = m->width = 0;
	m->data = NULL;
	m->dead = false;

#ifdef MATRIX_BONUS
	m->name = 'x';
#endif
}

/* Release allocated memory for the given matrix*/
void matrix_destroy(matrix* const m) {
	if (!m->data) //If this matrix never allocated memory, there is nothing to free
		return;

	for (int row = 0; row < m->height; ++row)
		free(m->data[row]);
	free(m->data);
	m->data = NULL;
}

bool matrix_same_size(matrix const* const a, matrix const* const b) {
	return a->height == b->height && a->width == b->width;
}

bool matrix_can_multiply(matrix const* const a, matrix const* const b) {
	return a->width == b->height;
}

/* Allocate height * width 2D array of underlying. */
underlying** allocate_2Darray(int const height, int const width) {
	underlying** const result = malloc(sizeof(underlying*) * height);

	if (!result) 
		return NULL;

	for (int row = 0; row < height; ++row) {
		result[row] = malloc(sizeof(underlying) * width);

		if (!result[row]) { //If allocation failed, delete all allocated blocks and return
			for (int i = 0; i < row; ++i)
				free(result[i]);
			free(result);
			return NULL;
		}
	}

	return result;
}

matrix matrix_multiply(matrix const * const left, matrix const * const right) {
	assert(matrix_can_multiply(left, right));
	matrix result = (matrix){ .height = left->height, .width = right->width, .dead = false };
	result.data = allocate_2Darray(result.height, result.width);
	assert(result.data && "Matrix needs to allocate memory to perform multiplication!");

	for (int row = 0; row < result.height; ++row)
		for (int col = 0; col < result.width; ++col) {
			underlying sum = 0;

			for (int index = 0; index < left->width; ++index)
				sum += left->data[row][index] * right->data[index][col];

			result.data[row][col] = sum;
		}

	return result;
}

/* Add second matrix to the first. Modifies the left operand in place. */
void matrix_add(matrix* const left, matrix* const right) {
	assert(matrix_same_size(left, right));
	assert(left->height == right->height);
	assert(left->width == right->width);
	for (int row = 0; row < left->height; ++row)
		for (int col = 0; col < left->width; ++col)
			left->data[row][col] += right->data[row][col];
}

/* Multiply the matrix by negative one. */
void matrix_negate(matrix* const m) {
	for (int row = 0; row < m->height; ++row)
		for (int col = 0; col < m->width; ++col)
			m->data[row][col] = -m->data[row][col];
}

/* Deepcopy given matrix. */
matrix matrix_copy(matrix const* const m) {
	matrix result = (matrix){ .height = m->height, .width = m->width, .dead = m->dead };
	result.data = allocate_2Darray(result.height, result.width);
	assert(result.data && "Matrix needs to allocate memory to perform copy!");
	for (int row = 0; row < result.height; ++row)
		for (int col = 0; col < result.width; ++col)
			result.data[row][col] = m->data[row][col];
	return result;
}



#endif
