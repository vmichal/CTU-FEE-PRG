#include "queue.h"

bool queue_grow(queue_t * const queue) {
	int const size = get_queue_size(queue);
	int const new_capacity = queue->capacity * 2;

	void ** new_storage = malloc(sizeof(void*) * new_capacity);
	if (!new_storage)
		return false;

	for (int i = 0; queue->read < queue->write; ++i, ++queue->read)
		new_storage[i] = queue->data[queue->read % queue->capacity];

	free(queue->data);
	queue->data = new_storage;
	queue->read = 0;
	queue->write = size;
	queue->capacity = new_capacity;

	return true;
}

void queue_shrink(queue_t * const queue) {
	int const size = get_queue_size(queue);
	int const new_capacity = size + 3;
	void ** new_storage = malloc(new_capacity * sizeof(void*));
	if (!new_storage) //If we can't shrink the queue than don't try to do so
		return;

	for (int i = 0; queue->read <queue->write; ++i, ++queue->read)
		new_storage[i] = queue->data[queue->read % queue->capacity];

	free(queue->data);
	queue->data = new_storage;
	queue->read = 0;
	queue->write = size;
	queue->capacity = new_capacity;
}

bool queue_empty(queue_t* queue) {
	return queue->write == queue->read;
}

bool queue_full(queue_t* queue) {
	return get_queue_size(queue) == queue->capacity;
}

queue_t* create_queue(int capacity) {
	
	queue_t *const queue = malloc(sizeof(queue_t));
	if (!queue)
		return NULL;

	queue->capacity = capacity;
	queue->read = queue->write = 0;
	queue->data = malloc(sizeof(void*) * capacity);
	if (!queue->data) {
		free(queue);
		return NULL;
	}
	return queue;
}

void delete_queue(queue_t* queue) {
	free(queue->data);
	free(queue);
}

bool push_to_queue(queue_t* queue, void* data) {

	if (queue_full(queue))
		if (!queue_grow(queue))
			return false;

	queue->data[queue->write % queue->capacity] = data;
	++queue->write;
	return true;
}

void* pop_from_queue(queue_t* const queue) {

	if (queue_empty(queue))
		return NULL;
	void * const result = queue->data[queue->read % queue->capacity];
	++queue->read;

	int const size = get_queue_size(queue); //Don't shrink the queue too much
	if (size < queue->capacity / 3 && size > 10)
		queue_shrink(queue);

	return result;
}

void* get_from_queue(queue_t* queue, int idx) {
	if (idx < 0 || queue->read + idx >= queue->write)
		return NULL;
	return queue->data[(queue->read + idx) % queue->capacity];
}

int get_queue_size(queue_t* queue) {
	return queue->write - queue->read;
}

