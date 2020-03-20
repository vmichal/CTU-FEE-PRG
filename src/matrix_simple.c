#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "matrix.h"

/* Reads matrix representation from stdin. Returns false iff an error occured. */
matrix *read_matrix() {
	matrix *const result = (matrix*) malloc(sizeof(matrix));
	assert(result);

	matrix_init(result);
	if (scanf("%d %d", &result->height, &result->width) != 2) {
		free(result);
		return NULL;
	}

	result->data = allocate_2Darray(result->height, result->width);
	assert(result->data && "Matrix needs to allocate memory to read input.");

	for (int row = 0; row < result->height; ++row)
		for (int col = 0; col < result->width; ++col)
			if (scanf("%d", &result->data[row][col]) != 1) {
				matrix_destroy(result);
				free(result);
				return NULL;
			}
	scanf(" ");
	return result;
}

void print_result(matrix* const result) {
	printf("%d %d\n", result->height, result->width);
	for (int row = 0; row < result->height; ++row) {
		for (int col = 0; col < result->width - 1; ++col)
			printf("%d ", result->data[row][col]);
		printf("%d\n", result->data[row][result->width - 1]);
	}
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

	struct expression_node *left, *right;
	matrix *literal;

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
	case NODE_LITERAL: //literal node must take care of the matrix literal lifetime
		matrix_destroy(expression_node->literal);
		free(expression_node->literal);

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
expression_node* construct_literal_node() {

	expression_node* const new_node = (expression_node*) malloc(sizeof(expression_node));
	assert(new_node);

	new_node->type = NODE_LITERAL;
	new_node->literal = read_matrix();
	new_node->left = NULL;
	new_node->right = NULL;
	new_node->result_size = (dimensions) {-1,-1}; //is set later by validate_node

	if (!new_node->literal) {
		free(new_node);
		return NULL;
	}
	return new_node;

}

/* Create new node specifying an operation with given type and left and right subtrees. */
expression_node* construct_operation_node(node_type type, expression_node* left, expression_node* right) {

	expression_node * const new_node = (expression_node*) malloc(sizeof(expression_node));
	assert(new_node);

	new_node->type = type;
	new_node->literal = NULL;
	new_node->left = left;
	new_node->right = right;
	new_node->result_size = (dimensions) {-1,-1};

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
	case NODE_SUB: {
		bool const subtree_valid = validate_node(node->left) && validate_node(node->right);
		bool const heights_same = node->left->result_size.height == node->right->result_size.height;
		bool const widths_same = node->left->result_size.width == node->right->result_size.width;

		node->result_size = node->left->result_size;

		return subtree_valid && heights_same && widths_same;
	}

	case NODE_MUL: {

		bool const subtree_valid = validate_node(node->left) && validate_node(node->right);

		node->result_size = (dimensions){ .height = node->left->result_size.height, .width = node->right->result_size.width };

		return subtree_valid && node->left->result_size.width == node->right->result_size.height;
	}
	}
	assert(false);


}


/* Read matrices and operators from stdin and form a tree describing order of compuations needed to evaluate
	the whole expression. Returns NULL iff an input error occured (e.g. an invalid int literal).*/
expression_node * construct_expression_tree() {

	expression_node * root = construct_literal_node();
	if (!root)
		return NULL;

	for (int operation; (operation = getchar()) != EOF;) {

		expression_node * const next_literal = construct_literal_node();
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

	if (!validate_node(root)) {
		node_cleanup(root);
		return NULL;
	}
	return root;
}

int main() {

	expression_node * const tree = construct_expression_tree();
	if (!tree) {
		fprintf(stderr, "Error: Chybny vstup!\n");
		exit(100);
	}

	matrix result = node_evaluate(tree);
	print_result(&result);
	matrix_destroy(&result);

	node_cleanup(tree);
	return EXIT_SUCCESS;

}
