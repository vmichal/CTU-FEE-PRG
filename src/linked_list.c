#include "linked_list.h"
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#define INVALID_ACCESS -1

//Type of elements stored in the list.
typedef int elem_t;

typedef struct node {
	struct node* next; //pointer to next node in chain; NULL if this one's last
	elem_t data; //data held by this node
} node;


typedef struct list {

	node* head, //pointer to the first node in the chain
		* tail; //last node in the chain. Both are NULL iff list is empty

	int size; //Counter of the chain's length

} list_t;

list_t global_list = { .head = NULL, .tail = NULL, .size = 0 };

bool list_empty() {
	return size() == 0;
}

void clear() {

	for (; !list_empty();) {
		node* const tmp = global_list.head;
		global_list.head = tmp->next;
		free(tmp);

		--global_list.size;
	}

	global_list.tail = NULL;
}

bool push(elem_t const entry) {
	if (entry < 0) {
		return false;
	}

	node* const new_node = (node*)malloc(sizeof(node));
	if (!new_node) {
		return false;
	}
	new_node->next = NULL;
	new_node->data = entry;

	if (list_empty()) {//the queue is empty, thus both pointers are null
		global_list.tail = global_list.head = new_node;
	}
	else {
		global_list.tail->next = new_node;
		global_list.tail = new_node;
	}
	++global_list.size;
	return true;
}



elem_t pop(void) {

	if (list_empty()) {
		return INVALID_ACCESS;
	}

	--global_list.size;
	node* const node = global_list.head;
	global_list.head = node->next;

	elem_t const result = node->data;
	free(node);

	if (list_empty()) {
		global_list.tail = NULL;
	}

	return result;
}

bool insert(elem_t const entry) {

	if (list_empty()) {
		return push(entry);
	}

	++global_list.size;

	node* const new_node = (node*)malloc(sizeof(node));
	new_node->data = entry;
	new_node->next = NULL;

	if (entry > global_list.head->data) {//new greatest element
		new_node->next = global_list.head;
		global_list.head = new_node;
		return true;
	}

	node* previous = global_list.head;
	for (; previous->next && entry < previous->next->data;) {
		previous = previous->next;
	}

	new_node->next = previous->next;
	previous->next = new_node;

	if (!new_node->next) {
		global_list.tail = new_node;
	}

	return true;
}

bool erase(elem_t const entry) {

	if (list_empty()) {
		return false;
	}

	bool removed_some = false;
	for (; !list_empty() && global_list.head->data == entry;) {
		node* const node = global_list.head;
		global_list.head = node->next;
		--global_list.size;

		free(node);
		removed_some = true;
	}

	if (list_empty()) {
		global_list.tail = NULL;
		return removed_some;
	}

	node* previous = global_list.head;
	for (; previous && previous->next; previous = previous->next) {
		for (node* current = previous->next; current && current->data == entry;) {
			previous->next = current->next;
			--global_list.size;

			node *const tmp = current;
			current = current->next;
			free(tmp);

			removed_some = true;
		}
		if (!previous->next) {
			break;
		}
	}
	global_list.tail = previous;

	return removed_some;
}

elem_t getEntry(int index) {

	if (index < 0 || index >= size()) {
		return INVALID_ACCESS;
	}

	if (index == size() - 1) {
		return global_list.tail->data;
	}

	node const* node = global_list.head;
	for (; index; --index, node = node->next) {}
	return node->data;
}

int size() {
	return global_list.size;
}

