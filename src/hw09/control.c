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
#include <threads.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

/*How often the blinking period shall be estimated. */
int const estimation_period_ms = 5 * 1000;
speed_t const default_communication_speed = B115200;

typedef struct {
	bool LED_on; //true iff last received update was 'x' or an acknowledge for 's' was received

	//number of rising and falling edges in LED activity. Cleared by estimation thread
	int volatile edge_counter;
	int volatile period; //estimated by timing thread every estimation_period

} module;

typedef struct {

	bool volatile quit;

	module* module;
	mtx_t mutex; //mutex locked by main and estimation thread

} shared_data_t;

/*Main function for the estimation thread. Every 'estimation_period_ms' milliseconds it updates
 the estimated blink period. Access to shared data is protected by a mutex. */
int estimation_thread_main(void* data_as_void) {
	shared_data_t* const data = (shared_data_t*)data_as_void;

	for (; !data->quit;) {
		mtx_lock(&data->mutex);

		//recalculate period estimation and clear counter of rising edges
		if (data->module->edge_counter == 0) {
			data->module->period = 0;
		}
		else { //Based on the measurements during 5s window, estimate the frequency of blinking
			data->module->period = (estimation_period_ms * 2) / data->module->edge_counter;
		}

		data->module->edge_counter = 0;

		mtx_unlock(&data->mutex);
		usleep(estimation_period_ms * 1000);
	}

	return thrd_success;
}

static void terminal_raw_mode(bool raw) {
	static bool is_raw = false;
	if (is_raw == raw) {
		return; //If the new mode is already set, no work is needed
	}


	static struct termios old;
	struct termios current;

	tcgetattr(fileno(stdin), &current);
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
static bool set_file_nonblocking(int const fd) {
	return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

static void configure_serial(int const fd) {
	struct termios termios;
	memset(&termios, 0, sizeof termios);

	// Read in existing settings, and handle any error
	assert(tcgetattr(fd, &termios) == 0);

	termios.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
	termios.c_cflag &= ~CSTOPB;
	termios.c_cflag &= ~CSIZE;
	termios.c_cflag |= CS8; // 8 bits per byte (most common)
	termios.c_cflag |= CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
	termios.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

	termios.c_lflag &= ~ECHONL; // Disable new-line echo
	termios.c_lflag &= ~ECHO; // Disable echo
	termios.c_lflag &= ~ECHOE; // Disable erasure
	termios.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
	termios.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
	termios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); 
	termios.c_lflag &= ~ICANON;
	termios.c_cflag &= ~OPOST;

	termios.c_cc[VTIME] = 1;
	termios.c_cc[VMIN] = 0;

	termios.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

	// Set in/out baud rate to be 9600
	cfsetispeed(&termios, default_communication_speed);
	cfsetospeed(&termios, default_communication_speed);

	// Save tty settings, also checking for error

	assert(tcsetattr(fd, TCSANOW, &termios) == 0);
}

int read_char(int const fd) {
	char result;
	return read(fd, &result, 1) < 0 ? -1 : result;
}

void write_char(int const fd, char c) {
	write(fd, &c, 1);
}


int main(int argc, char** argv) {

	assert(argc == 2);

	int const serial_port = open(argv[1], O_RDWR | O_NOCTTY | O_SYNC);
	assert(serial_port != -1);

	/* Enable non-blocking mode for both stdin as well as input from the module. */
	assert(0 == set_file_nonblocking(fileno(stdin)));
	assert(0 == set_file_nonblocking(serial_port));
	configure_serial(serial_port);

	terminal_raw_mode(true);

	/* Create ringbuffers for sent commands and expected acknowledgements. This way communication
	is safe even if the module would delay sending acknowledgements. It is still necessary to send
	ACKs in order they are expected. */
	queue_t* const expected_acks = create_queue(32);
	queue_t* const sent_commands = create_queue(32);
	push_to_queue(expected_acks, 'i'); //module shall send 'i' during initialization
	push_to_queue(sent_commands, ' '); //space so that received ACK 'i' has a command to pair to

	module module_data = { .LED_on = false, .edge_counter = 0, .period = 0 };
	shared_data_t shared_data = { .quit = false, .module = &module_data };
	assert(0 == mtx_init(&shared_data.mutex, mtx_plain));

	//Start the period estimating thread
	thrd_t estimation_thread;
	assert(0 == thrd_create(&estimation_thread, &estimation_thread_main, &shared_data));

	//Variables storing last successfully sent and recieved chars.
	char last_sent = ' ';
	char last_recieved = '?';

	/*Main loop is based on the same variable as estimating thread.
	Should it switch to false, both threads will exit loops. */
	for (; !shared_data.quit;) {
		//Character read from the stdin. May be -1 if nothing was readable (stdin is in nonblocking mode).
		int const command = fgetc(stdin);

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
			fprintf(stderr, "\nCommand '%c' is not recognized! Ignored.\n", command);
			command_valid = false;
		}
		//If the read character was actually a command, send it to the module 
		if (command_valid) {
			write_char(serial_port, command);
			push_to_queue(sent_commands, command);

			last_sent = command;
		}

		//Character read from pipe, sent by module. May be -1 if nothing was pending
		int const ack = read_char(serial_port);

		last_recieved = ack == -1 ? last_recieved : ack;

		if (ack == -1) {
			//ignore empty read
			continue;
		}
		else if (ack == 'x' || ack == 'o') { //periodical update of LED state
			mtx_lock(&shared_data.mutex);
			module_data.LED_on = ack == 'x';
			++module_data.edge_counter;
			mtx_unlock(&shared_data.mutex);
		}
		else if (get_queue_size(expected_acks) == 0) {
			fprintf(stderr, "Unexpected ACK '%d'. No command was sent recently.\n", ack);
		}
		else if (ack != get_from_queue(expected_acks, 0)) {
			fprintf(stderr, "Unexpected ACK '%d'. Was expecting '%c'.\n"
				, ack, get_from_queue(expected_acks, 0));
		}
		else { //successfully received an acknowledgement, that we actually expected
			int const corresponding_command = get_from_queue(sent_commands, 0);

			if (ack == 'a' && (corresponding_command == 's' || corresponding_command == 'e')) {
				module_data.LED_on = corresponding_command == 's'; //switch LED state
			}
			else if (ack == 'b') { //If module confirmed end of communication, quit
				shared_data.quit = true;
			}
			pop_from_queue(expected_acks);
			pop_from_queue(sent_commands);
		}

		/*Finally update the terminal if needed.*/
		printf("\rLED %3s send : '%c' received : '%c', T = %4d ms, ticker = %4d"
			, module_data.LED_on ? "on" : "off", last_sent, last_recieved
			, module_data.period, module_data.edge_counter);
	}
	if (get_queue_size(expected_acks) > 0) { //Print message if some commands did not receive ack yet
		fprintf(stderr, "Remaining %d acknowledgements!\n\r", get_queue_size(expected_acks));
	}
	/*Cleanup resources first and postpone thread joining. There are up to 5s of
	delay, since the estimation thread has to wake up first */
	delete_queue(expected_acks);
	delete_queue(sent_commands);
	terminal_raw_mode(false);
	printf("\n\n");

	//tty is not freed, it's alias to stdin
	close(serial_port);

	assert(0 == thrd_join(estimation_thread, NULL));
	mtx_destroy(&shared_data.mutex);
	return EXIT_SUCCESS;
}
