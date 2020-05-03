#include "queue.h"
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

//Type of elements stored in the list.
typedef void* elem_t;

typedef struct node {
	struct node* next; //pointer to next node in chain; NULL if this one's last
	elem_t data; //data held by this node
} node;


typedef struct list {

	node* head, //pointer to the first node in the chain
		* tail; //last node in the chain. Both are NULL iff list is empty

	int size; //Counter of the chain's length

	/*comparator predicate. Returns number <0, =0, > when element a compares
	less than, equal or greater than element b.*/
	int (*compare)(const void* a, const void* b);

	//Deleter function for stored elements. Called in clear.
	//Default deleter is library function free
	void (*deleter)(void*);

} list_t;

bool list_empty(list_t const* const list) {
	return size(list) == 0;
}

void* create() {
	list_t* const new_list = (list_t*)malloc(sizeof(list_t));
	if (!new_list) {
		return NULL;
	}

	new_list->size = 0;
	new_list->head = new_list->tail = NULL;
	new_list->compare = NULL;
	new_list->deleter = &free;
	return new_list;
}

void clear(void* queue) {
	assert(queue);
	list_t* const list = (list_t*)queue;

	for (; !list_empty(list);) {
		node* const tmp = list->head;
		list->head = tmp->next;
		list->deleter(tmp->data);
		free(tmp);

		--list->size;
	}
	list->tail = NULL;
}

bool push(void* queue, void* entry) {
	assert(queue);
	if (!entry) {
		return false;
	}

	list_t* const list = (list_t*)queue;

	node* const new_node = (node*)malloc(sizeof(node));
	if (!new_node) {
		return false;
	}
	new_node->next = NULL;
	new_node->data = entry;

	if (list_empty(list)) {//the queue is empty, thus both pointers are null
		list->tail = list->head = new_node;
	}
	else {
		list->tail->next = new_node;
		list->tail = new_node;
	}
	++list->size;
	return true;
}



elem_t pop(elem_t queue) {
	assert(queue);
	list_t* const list = (list_t*)queue;

	if (list_empty(list)) {
		return NULL;
	}

	--list->size;
	node* const node = list->head;
	list->head = node->next;

	elem_t const result = node->data;
	free(node);

	if (list_empty(list)) {
		list->tail = NULL;
	}

	return result;
}

bool insert(void* queue, elem_t entry) {
	assert(queue);
	list_t* const list = (list_t*)queue;

	if (!list->compare) {
		return false;
	}

	if (list_empty(list)) {
		return push(queue, entry);
	}

	++list->size;

	node* const new_node = (node*)malloc(sizeof(node));
	if (!new_node) {
		return false;
	}
	new_node->data = entry;
	new_node->next = NULL;

	if (list->compare(entry, list->head->data) >= 0) {//new greatest element
		new_node->next = list->head;
		list->head = new_node;
		return true;
	}

	node* previous = list->head;
	for (; previous->next && list->compare(entry, previous->next->data) < 0;) {
		previous = previous->next;
	}

	new_node->next = previous->next;
	previous->next = new_node;

	if (!new_node->next) { //new last node
		list->tail = new_node;
	}

	return true;
}

bool erase(void* queue, elem_t entry) {
	assert(queue);
	list_t* const list = (list_t*)queue;

	if (list_empty(list)) {
		return false;
	}

	bool removed_some = false;
	for (; !list_empty(list) && 0 == list->compare(list->head->data, entry);) {
		node* const node = list->head;
		list->head = node->next;
		--list->size;

		list->deleter(node->data);
		free(node);
		removed_some = true;
	}

	if (list_empty(list)) {
		list->tail = NULL;
		return removed_some;
	}

	node* previous = list->head;
	for (; previous->next; previous = previous->next) {
		node* current = previous->next;
		for (; current && 0 == list->compare(current->data, entry);) {
			previous->next = current->next;
			--list->size;
			list->deleter(current->data);

			node* const tmp = current;
			current = current->next;
			free(tmp);

			removed_some = true;
		}
		if (!previous->next) {
			break;
		}
	}
	list->tail = previous;

	return removed_some;
}

void* getEntry(void const* queue, int index) {
	assert(queue);
	list_t const* const list = (list_t*)queue;

	if (index < 0 || index >= size(list)) {
		return NULL;
	}

	if (index == size(list) - 1) {
		return list->tail->data;
	}

	node const* node = list->head;
	for (; index; --index, node = node->next) {}
	return node->data;
}

int size(void const* queue) {
	assert(queue);
	list_t const* const list = (list_t*)queue;
	return list->size;
}

void setCompare(void* queue, int (*compare)(void const*, void const*)) {
	assert(queue);
	list_t* const list = (list_t*)queue;
	list->compare = compare;
}

void setClear(void* queue, void (*clear)(void*)) {
	assert(queue);
	list_t* const list = (list_t*)queue;
	list->deleter = clear ? clear : &free;
}




