/*
*
* Implementation of ring buffer because Brute does not support custom makefiles
* HW starts around line 170
*/

#include <stdbool.h>
#include <stdlib.h>

typedef char queue_elem_t;

/* Queue structure which holds all necessary data */
typedef struct queue_t {

	/* Positions of read and write pointers within the buffer. Their values are not
	kept within the range 0..capacity as normal, instead they are free to grow
	above this value and modulo operations are performed only when push/pop is executed.

	This way none of the capacity is wasted, whilst normally, when read and write is stored
	modulo capacity, one spot is wasted to distinguish full and empty buffer (read == write
	would have ambiguous meaning). */
	unsigned int read, write;

	/* Size of allocated buffer. */
	unsigned int capacity;

	queue_elem_t* data;
} queue_t;

#ifdef __cpluscplus
constexpr int QUEUE_GROW_COEF = 2;
constexpr int QUEUE_SHRINK_DIVISOR = 3;
constexpr int QUEUE_MIN_CAPACITY = 10;
#else
#define QUEUE_GROW_COEF 2
#define QUEUE_SHRINK_COEF 3
#define QUEUE_MIN_CAPACITY 10
#endif

/* gets number of stored elements */
int get_queue_size(queue_t const* queue) {
	return queue->write - queue->read;
}

/*
 * gets idx-th element from the queue
 * returns the element that will be popped after idx calls of the pop_from_queue()
 * returns: the idx-th element on success; NULL otherwise
 */
queue_elem_t get_from_queue(queue_t const* queue, int idx) {
	if (idx < 0 || queue->read + idx >= queue->write) {
		return 0;
	}
	return queue->data[(queue->read + idx) % queue->capacity];
}

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

/* Returns true iff the given queue holds no elements. */
bool queue_empty(queue_t const* const queue) {
	return queue->write == queue->read;
}

/* Returns true iff the given queue's allocated memory cannot hold any more elements. */
bool queue_full(queue_t const* const queue) {
	return get_queue_size(queue) == queue->capacity;
}

/* creates a new queue with a given size */
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

/* deletes the queue and all allocated memory */
void delete_queue(queue_t* const queue) {
	free(queue->data);
	free(queue);
}

/*
 * inserts a reference to the element into the queue
 * returns: true on success; false otherwise
 */
bool push_to_queue(queue_t* const queue, queue_elem_t const data) {

	if (queue_full(queue) && !queue_grow(queue)) {
		return false;
	}

	queue->data[queue->write % queue->capacity] = data;
	++queue->write;
	return true;
}

/*
 * gets the first element from the queue and removes it from the queue
 * returns: the first element on success; NULL otherwise
 */
queue_elem_t pop_from_queue(queue_t* const queue) {

	if (queue_empty(queue)) {
		return 0;
	}
	queue_elem_t const result = queue->data[queue->read % queue->capacity];
	++queue->read;

	int const size = get_queue_size(queue); //Don't shrink the queue too much
	if (size * QUEUE_SHRINK_COEF < queue->capacity && size > QUEUE_MIN_CAPACITY) {
		queue_shrink(queue);
	}

	return result;
}


/*
*
* HW08a implementation
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <assert.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

/* Paths to named fifos connecting this application to the module. I hope that
they are correct, because the assignment lacks any information regarding this topic.

One stream is "me in, module out", the other is "me out, module in".
*/
const char* const module_input_path = "/tmp/prga-hw08.out";
const char* const module_output_path = "/tmp/prga-hw08.in";

/*How often the blinking period shall be estimated. */
int const estimation_period_ms = 5 * 1000;

typedef struct {
	bool LED_active; //true in between 's' and 'e' commands
	bool LED_on; //true iff last received update was 'x'

	//number of rising edges in LED activity. Cleared by estimation thread
	int rise_counter;
	volatile int period; //estimated by timing thread every estimation_period

} module;

typedef struct {

	volatile bool quit;

	module* module;
	pthread_mutex_t mutex; //mutex locked by main and estimation thread

} shared_data_t;

/*Main function for the estimation thread. Every 'estimation_period_ms' milliseconds it updates
 the estimated blink period. Access to shared data is protected by a mutex. */
void* estimation_thread_main(void* data_as_void) {
	shared_data_t* const data = (shared_data_t*)data_as_void;

	for (; !data->quit;) {
		pthread_mutex_lock(&data->mutex);

		//recalculate period estimation and clear counter of rising edges
		if (data->module->rise_counter == 0) {
			data->module->period = 0;
		}
		else {
			data->module->period = estimation_period_ms / data->module->rise_counter;
		}

		data->module->rise_counter = 0;

		pthread_mutex_unlock(&data->mutex);
		usleep(estimation_period_ms * 1000);
	}

	return NULL;
}

static void terminal_raw_mode(bool raw) {
	static bool is_raw = false;
	if (is_raw == raw) {
		return; //If the new mode is already set, no work is needed
	}


	static struct termios current, old;
	tcgetattr(STDIN_FILENO, &current);
	if (raw) {
		old = current;
		cfmakeraw(&current);
		tcsetattr(STDIN_FILENO, TCSANOW, &current);
		is_raw = true;
	}
	else {
		tcsetattr(STDIN_FILENO, TCSANOW, &old);
		is_raw = false;
	}

}


/* Switch the given file pointer to nonblocking mode.*/
static bool set_file_nonblocking(FILE* const ptr) {
	int const fd = fileno(ptr);
	return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}


int main(int argc, char** argv) {

	FILE* const tty = stdin;
	FILE* const module_output = fopen(module_output_path, "r");
	FILE* const module_input = fopen(module_input_path, "w");

	/* Enable non-blocking mode for both stdin as well as input from the module. */
	assert(module_input && module_output && tty);
	assert(0 == set_file_nonblocking(tty));
	assert(0 == set_file_nonblocking(module_output));

	terminal_raw_mode(true);

	/* Create ringbuffers for sent commands and expected acknowledgements. This way communication
	is safe even if the module would delay sending acknowledgements. It is still necessary to send
	ACKs in order they are expected. */
	queue_t* const expected_acks = create_queue(32);
	queue_t* const commands = create_queue(32);
	push_to_queue(expected_acks, 'i'); //module shall send 'i' during initialization
	push_to_queue(commands, ' '); //space so that received ACK 'i' has a command to pair to

	module module_data = { .LED_active = false, .LED_on = false, .rise_counter = 0, .period = 0 };
	shared_data_t shared_data = { .quit = false, .module = &module_data };
	pthread_mutex_init(&shared_data.mutex, NULL);

	//Start the period estimating thread
	pthread_t estimation_thread;
	pthread_create(&estimation_thread, NULL, &estimation_thread_main, &shared_data);

	//Variables storing last successfully sent and recieved chars.
	char last_sent = ' ';
	char last_recieved = '?';

	/*Main loop is based on the same variable as estimating thread.
	Should it switch to false, both threads will exit loops. */
	for (; !shared_data.quit;) {

		//Character read from the stdin. May be -1 if nothing was readable (std is in nonblocking mode).
		int const command = fgetc(tty);

		bool command_valid = command != -1 && command != 'q';
		switch (command) {
		case -1: //ignore 'empty' input.
			break;
		case 'q':
			shared_data.quit = true;
			break;
		case 's': case 'e': case '1': case '2': case '3': case '4': case '5':
			push_to_queue(expected_acks, 'a');
			break;
		case 'h':
			push_to_queue(expected_acks, 'h');
			break;
		case 'b':
			push_to_queue(expected_acks, 'b');
			break;
		default:
			fprintf(stderr, "\nCommand '%c' is not recognized! Ignored!\n", command);
			command_valid = false;
		}
		//If the read character was actually a command, send it to the module 
		if (command_valid) {
			fputc(command, module_input);
			fflush(module_input);
			push_to_queue(commands, command);

			last_sent = command;
		}

		//Character read from pipe, sent by module. May be -1 if nothing was pending
		int const ack = fgetc(module_output);

		last_recieved = ack == -1 ? last_recieved : ack;

		if (ack == -1) {
			//ignore empty read
		}
		else if (ack == 'x' || ack == 'o') { //periodical update of LED state
			assert(module_data.LED_active);
			pthread_mutex_lock(&shared_data.mutex);
			assert((ack == 'o') == module_data.LED_on);
			if (ack == 'x') {
				++module_data.rise_counter;
			}
			module_data.LED_on = !module_data.LED_on;
			pthread_mutex_unlock(&shared_data.mutex);
		}
		else if (get_queue_size(expected_acks) == 0) {
			fprintf(stderr, "Unexpected ACK '%c'. No command was sent recently.\n", ack);
		}
		else if (ack != get_from_queue(expected_acks, 0)) {
			fprintf(stderr, "Unexpected ACK '%c'. Was expecting '%c'.\n"
				, ack, get_from_queue(expected_acks, 0));
		}
		else { //successfully received an acknowledgement, that we actually expected
			int const corresponding_command = get_from_queue(commands, 0);

			if (ack == 'a' && (corresponding_command == 's' || corresponding_command == 'e')) {
				module_data.LED_active = corresponding_command == 's'; //switch LED state
				module_data.LED_on = false;
			}
			pop_from_queue(expected_acks);
			pop_from_queue(commands);
		}

		/*Finally update the terminal.*/
		printf("\rLED %3s send : '%c' received : '%c', T = %4d ms, ticker = %4d",
			module_data.LED_active ? "on" : "off", last_sent, last_recieved, module_data.period, module_data.rise_counter);
	}
	if (get_queue_size(expected_acks) > 0) { //Print informative message if some commands did not receive ack
		fprintf(stderr, "Remaining %d acknowledgements!\n\r", get_queue_size(expected_acks));
	}

	/*Cleanup resources first and postpone thread joining. There are up to 5s of
	delay, since the estimation thread has to wake up first */
	delete_queue(expected_acks);
	delete_queue(commands);
	terminal_raw_mode(false);
	printf("\n\n");

	pthread_join(estimation_thread, NULL);
	return EXIT_SUCCESS;
}

