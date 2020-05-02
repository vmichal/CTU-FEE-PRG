#include "queue.h"

#ifdef __cpluscplus
constexpr int QUEUE_GROW_COEF = 2;
constexpr int QUEUE_SHRINK_DIVISOR = 3;
constexpr int QUEUE_MIN_CAPACITY = 10;
#else
#define QUEUE_GROW_COEF 2
#define QUEUE_SHRINK_COEF 3
#define QUEUE_MIN_CAPACITY 10
#endif

/* Doubles the allocated capacity for queue or does nothing if error occurs.
	Returns true iff reallocation was successful.*/
static bool queue_grow(queue_t* const queue) {
	int const size = get_queue_size(queue);
	int const new_capacity = queue->capacity * 2;

	queue_elem_t* const new_storage = (queue_elem_t*)malloc(sizeof(queue_elem_t) * new_capacity);
	if (!new_storage) {
		return false;
	}
	//Copy stored data into the new buffer.
	for (int i = 0; i < size; ++i) {
		new_storage[i] = get_from_queue(queue, i);
	}

	free(queue->data);
	queue->data = new_storage;
	queue->read = 0;
	queue->write = size;
	queue->capacity = new_capacity;

	return true;
}

/* Reduce the queue's capacity to one third and shrink allocated storage. */
static void queue_shrink(queue_t* const queue) {
	int const size = get_queue_size(queue);
	//safety 3 ensures presence of some extra space to prevent immediate reallocation right after
	int const new_capacity = size + 3;
	queue_elem_t* const new_storage = (queue_elem_t*)malloc(new_capacity * sizeof(queue_elem_t));
	if (!new_storage) { //If we can't shrink the queue then don't try to do so
		return;
	}

	for (int i = 0; i < size; ++i) {
		new_storage[i] = get_from_queue(queue, i);
	}

	free(queue->data);
	queue->data = new_storage;
	queue->read = 0;
	queue->write = size;
	queue->capacity = new_capacity;
}

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

	if (queue_full(queue) && !queue_grow(queue)) {
		return false;
	}

	queue->data[queue->write % queue->capacity] = data;
	++queue->write;
	return true;
}

queue_elem_t pop_from_queue(queue_t* const queue) {

	if (queue_empty(queue)) {
		return NULL;
	}
	queue_elem_t const result = queue->data[queue->read % queue->capacity];
	++queue->read;

	int const size = get_queue_size(queue); //Don't shrink the queue too much
	if (size * QUEUE_SHRINK_COEF < queue->capacity && size > QUEUE_MIN_CAPACITY) {
		queue_shrink(queue);
	}

	return result;
}

queue_elem_t get_from_queue(queue_t const* queue, int idx) {
	if (idx < 0 || queue->read + idx >= queue->write) {
		return NULL;
	}
	return queue->data[(queue->read + idx) % queue->capacity];
}

int get_queue_size(queue_t const* queue) {
	return queue->write - queue->read;
}

