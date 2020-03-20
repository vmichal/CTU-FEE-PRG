#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define MATRIX_BONUS
#include "matrix.h"

/* Parses matrices from standard input. Given format:
B=[5 2 4; 0 2 -1; 3 -5 -4]
E=[-6 -5 -8; -1 -1 -10; 10 0 -7]
R=[-1 -7 6; -2 9 -4; 6 -10 2]
*/
matrix read_matrix_bonus() {
	matrix m = (matrix){ .dead = false };

	assert(getchar() == '=' && getchar() == '[');

	//Read first row of the matrix and determine the width
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

	//Continue reading rows one by one with known width searching for closing square bracket denoting end of matrix
	int max_height = 100;
	m.data = malloc(sizeof(int*) * max_height);
	m.data[0] = row;

	int height = 1;
	for (char delimiter; (delimiter = getchar()) == ';' ;++height) {
		if (height == max_height) {
			max_height *= 2;
			m.data = realloc(m.data, sizeof(int*) * max_height);
		}
		m.data[height] = malloc(sizeof(int) * width);
		for (int i = 0; i < width; ++i)
			scanf("%d", &m.data[height][i]);
	}

	m.height = height;
	assert(getchar() == '\n');// consumes newline
	return m;
}

void print_result(matrix* const result) {
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


typedef enum node_type {
	NODE_LITERAL,
	NODE_ADD,
	NODE_SUB,
	NODE_MUL
} node_type;


typedef struct dimensions {
	int height;
	int width;
} dimensions;


typedef struct expression_node {
	node_type type;

	struct expression_node* left, * right;
	matrix const* literal;

	dimensions result_size;

} expression_node;

/* Evaluate given node in the expression tree. Returns the result of the corresponding computation
	or stored matrix literal. For non-literal nodes recursive calls to left and right subtrees are issued
	in order to determine the left and right operands of computation. */
matrix node_evaluate(expression_node const* const expression_node) {
	switch (expression_node->type) {
	case NODE_LITERAL:
		return matrix_copy(expression_node->literal);
	case NODE_SUB:
	case NODE_ADD: {
		matrix left = node_evaluate(expression_node->left);
		matrix right = node_evaluate(expression_node->right);

		if (expression_node->type == NODE_SUB)
			matrix_negate(&right);

		matrix_add(&left, &right);
		matrix_destroy(&right);
		return left;
	}
	case NODE_MUL: {
		matrix left = node_evaluate(expression_node->left);
		matrix right = node_evaluate(expression_node->right);
		matrix const result = matrix_multiply(&left, &right);
		matrix_destroy(&left);
		matrix_destroy(&right);
		return result;
	}
	default:
		assert(false && "Invalid node_type!");
	}
}

/* Free memory allocated by this node and the node itself as well. */
void node_cleanup(expression_node* const expression_node) {
	switch (expression_node->type) {
	case NODE_LITERAL: //literal node does nothing, beacuse matrix lifetime is managed from main
		break;

	case NODE_ADD:
	case NODE_MUL:
	case NODE_SUB:
		node_cleanup(expression_node->right);
		node_cleanup(expression_node->left);
		break;
	}
	free(expression_node);
}

/* Create a new literal node and read matrix from stdin into it.
	Returns false iff an input error occured. */
expression_node* construct_literal_node(matrix const* const matrix) {
	assert(matrix);
	expression_node* const new_node = (expression_node*)malloc(sizeof(expression_node));
	assert(new_node);

	new_node->type = NODE_LITERAL;
	new_node->literal = matrix;
	new_node->left = NULL;
	new_node->right = NULL;
	new_node->result_size = (dimensions){ -1,-1 }; //is set later by validate_node

	return new_node;

}

/* Create new node specifying an operation with given type and left and right subtrees. */
expression_node* construct_operation_node(node_type type, expression_node* left, expression_node* right) {

	expression_node* const new_node = (expression_node*)malloc(sizeof(expression_node));
	assert(new_node);

	new_node->type = type;
	new_node->literal = NULL;
	new_node->left = left;
	new_node->right = right;
	new_node->result_size = (dimensions){ -1,-1 };

	return new_node;
}

/* Recursively traverse the expression tree and determine, whether dimensions on both sides of operators agree.
	Addition/subtraction must have same dimensions on the left and right; multiplication is strange. */
bool validate_node(expression_node* const node) {
	switch (node->type) {
	case NODE_LITERAL:
		node->result_size = (dimensions){ .height = node->literal->height, .width = node->literal->width };
		return true;

	case NODE_ADD:
	case NODE_SUB: { //Both matrices ought to have same dimensions
		bool const subtree_valid = validate_node(node->left) && validate_node(node->right);
		bool const heights_same = node->left->result_size.height == node->right->result_size.height;
		bool const widths_same = node->left->result_size.width == node->right->result_size.width;

		node->result_size = node->left->result_size;

		return subtree_valid && heights_same && widths_same;
	}

	case NODE_MUL: { //You know the rule to multiply matrices

		bool const subtree_valid = validate_node(node->left) && validate_node(node->right);

		node->result_size = (dimensions){ .height = node->left->result_size.height, .width = node->right->result_size.width };

		return subtree_valid && node->left->result_size.width == node->right->result_size.height;
	}
	}
	assert(false);


}


/* Read matrices and operators from stdin and form a tree describing order of compuations needed to evaluate
	the whole expression. Returns NULL iff an input error occured (e.g. an invalid int literal).*/
expression_node* construct_expression_tree(matrix const * const matrices) {

	expression_node* root = construct_literal_node(&matrices[getchar() - 'A']);
	if (!root)
		return NULL;

	for (int operation; (operation = getchar()) != EOF && operation != '\n';) {

		expression_node* const next_literal = construct_literal_node(&matrices[getchar() - 'A']);
		if (!next_literal) {
			node_cleanup(root);
			return NULL;
		}

		switch (operation) {
		case '*':
			if (root->type == NODE_LITERAL) //first iteration
				root = construct_operation_node(NODE_MUL, root, next_literal);
			else
				root->right = construct_operation_node(NODE_MUL, root->right, next_literal);
			break;

		case '+':
			root = construct_operation_node(NODE_ADD, root, next_literal);
			break;

		case '-': {
			root = construct_operation_node(NODE_SUB, root, next_literal);
			break;
		}
		default:
			assert(false);
		}
	}

	//Invalid expressions make no sense, so we will signal an error 
	//if e.g. addition of matrices with different sizes is required
	if (!validate_node(root)) {
		node_cleanup(root);
		return NULL;
	}
	return root;
}

#define alphabet_size ('z' - 'a' + 1)

int main() {

	// Reserve a matrix for each letter of latin alphabet
	matrix matrices[alphabet_size];
	for (int i = 0; i < alphabet_size; ++i)
		matrix_init(&matrices[i]);

	//And read actual data into them: Until empty line is hit, keep reading named matrices 
	for (char name; (name = getchar()) != '\n';) {
		matrix* const m = &matrices[name - 'A'];
		*m = read_matrix_bonus();
		m->name = name;
	}

	//Parse last line of input and simultaneously estabilish expression tree
	expression_node * const tree = construct_expression_tree(matrices);

	//Recursively compute the result
	matrix result = node_evaluate(tree);
	print_result(&result);
	matrix_destroy(&result);

	//And clean all allocated memory
	node_cleanup(tree);

	for (int i = 0; i < alphabet_size; ++i)
		matrix_destroy(&matrices[i]);

	return EXIT_SUCCESS;


}
