#include "ringbuffer.h"

#include <assert.h>


bool queue_empty(queue_t const* const queue) {
	return queue->write == queue->read;
}

bool queue_full(queue_t const* const queue) {
	return get_queue_size(queue) == queue->capacity;
}

queue_t* create_queue(int capacity) {

	queue_t* const queue = (queue_t*)malloc(sizeof(queue_t));
	queue_elem_t* const data = (queue_elem_t*)malloc(sizeof(queue_elem_t) * capacity);

	if (!queue || !data) {
		free(queue);
		free(data);
		return NULL;
	}

	queue->capacity = capacity;
	queue->read = queue->write = 0;
	queue->data = data;
	return queue;
}

void delete_queue(queue_t* const queue) {
	free(queue->data);
	free(queue);
}

bool push_to_queue(queue_t* const queue, queue_elem_t const data) {

	if (queue_full(queue)) {
		return false;
	}

	queue->data[queue->write % queue->capacity] = data;
	++queue->write;
	return true;
}

queue_elem_t pop_from_queue(queue_t* const queue) {

	assert(!queue_empty(queue));
	queue_elem_t const result = queue->data[queue->read % queue->capacity];
	++queue->read;

	return result;
}

queue_elem_t get_from_queue(queue_t const* queue, int idx) {
	assert(idx >= 0);
	assert(queue->read + idx < queue->write);
	return queue->data[(queue->read + idx) % queue->capacity];
}

int get_queue_size(queue_t const* queue) {
	return queue->write - queue->read;
}

