#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Queue structure which holds all necessary data */
typedef struct {

    /* Positions of read and write pointers within the buffer. Their values are not
    kept within the range 0..capacity as normal, instead they are free to grow 
    above this value and modulo operations are performed only when push/pop is executed.

    This way none of the capacity is wasted, whilst normally, when read and write is stored
    modulo capacity, one spot is wasted to distinguish full and empty buffer (read == write
    would have ambiguous meaning). */
    unsigned int read, write;

    /* Size of allocated buffer. */
    unsigned int capacity;

    void** data;
} queue_t;

/* creates a new queue with a given size */
queue_t* create_queue(int capacity);

/* deletes the queue and all allocated memory */
void delete_queue(queue_t* queue);

/*
 * inserts a reference to the element into the queue
 * returns: true on success; false otherwise
 */
bool push_to_queue(queue_t* queue, void* data);

/*
 * gets the first element from the queue and removes it from the queue
 * returns: the first element on success; NULL otherwise
 */
void* pop_from_queue(queue_t* queue);

/*
 * gets idx-th element from the queue
 * returns the element that will be popped after idx calls of the pop_from_queue()
 * returns: the idx-th element on success; NULL otherwise
 */
void* get_from_queue(queue_t* queue, int idx);

/* gets number of stored elements */
int get_queue_size(queue_t* queue);

/* Returns true iff the given queu holds no elements. */
bool queue_empty(queue_t* queue);

/* Returns true iff the given queue cannot contain any more elements. */
bool queue_full(queue_t* queue);

#endif /* QUEUE_H */
