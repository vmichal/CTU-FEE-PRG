/*
*
* HW10 implementation
* Vojtìch Michal
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

#include "protocol.h"
#include "ringbuffer.h"

/*How often the blinking period shall be estimated. */
speed_t const default_communication_speed = B115200;
//Ring buffer containing incoming messages
queue_t* messages;

/* Enumeration of valid states of the computation module. */
enum module_state {
	module_computing, //currently performing computation (data flowing in from the module)
	module_idle, //waiting for new commands
	module_starting, //Module received request to start, but did not send an acknowledge yet
	module_aborting //Module received request to abort computation, but did not respond yet
};

struct module_data {
	enum module_state state;

	//Currently (or lastly) computed pile of data
	int chunk_id;

	//FD corresponding to nucleo's serial port
	int file_descriptor;

} module_data = { .state = module_idle, .chunk_id = 0 };

struct {

	bool volatile quit;

} thread_data = { .quit = false };


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

	termios.c_lflag &= ~IEXTEN; // Make number 22 work
	termios.c_lflag &= ~ECHONL; // Disable new-line echo
	termios.c_lflag &= ~ECHO; // Disable echo
	termios.c_lflag &= ~ECHOE; // Disable erasure
	termios.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
	termios.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
	termios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
	termios.c_lflag &= ~ICANON;
	termios.c_cflag &= ~OPOST;

	termios.c_cc[VTIME] = 10; //One second of waiting
	termios.c_cc[VMIN] = 0;

	termios.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

	// Set in/out baud rate to be 9600
	cfsetispeed(&termios, default_communication_speed);
	cfsetospeed(&termios, default_communication_speed);

	// Save tty settings, also checking for error

	assert(tcsetattr(fd, TCSANOW, &termios) == 0);
}

void send_version_request() {
	message msg = { .type = MSG_GET_VERSION };
	message_calculate_checksum(&msg);
	message_enqueue(&msg);
}

void send_abort_request() {
	message msg = { .type = MSG_ABORT };
	message_calculate_checksum(&msg);
	message_enqueue(&msg);
}

void send_message_compute(int const tasks) {
	message msg = { .type = MSG_COMPUTE };
	msg.data.compute = (msg_compute){ module_data.chunk_id, tasks };
	message_calculate_checksum(&msg);
	message_enqueue(&msg);
}

void message_enqueue(message const* msg) {
	uint8_t buffer[sizeof(message)];
	message_decompose(msg, buffer, sizeof buffer);
	write(module_data.file_descriptor, buffer, message_size(msg->type));
}

/* Main function for the thread reading input from the serial port. */
int serial_input_thread() {
	fprintf(stderr, "INFO: Serial port listening thread started.\r\n");

	uint8_t buffer[sizeof(message)];
	unsigned write_index = 0;

	memset(buffer, 0, sizeof buffer);

	for (; !thread_data.quit;) {
		if (read(module_data.file_descriptor, &buffer[write_index], 1) == 1) {
			++write_index;

			if (write_index == message_size(buffer[0])) {
				message const msg = message_parse(buffer, write_index);
				push_to_queue(messages, msg);
				memset(buffer, 0, write_index);
				write_index = 0;

			}
		}
	}
	fprintf(stderr, "INFO: Serial port listening thread exits.\r\n");
	if (write_index != 0) {
		fprintf(stderr, "WARN: Listening thread exits without receiving the whole message!\r\n");
	}
	return 0;
}

void poll_stdin() {
	//Character read from the stdin. May be -1 if nothing was readable (stdin is in nonblocking mode).
	int const command = fgetc(stdin);

	switch (command) {
	case -1: //ignore 'empty' input.
		break;
	case 'q':
		fprintf(stderr, "INFO: Requested exit command.\r\n");
		thread_data.quit = true;
		if (module_data.state != module_idle) {
			send_abort_request();
		}
		break;
	case 'g':
		fprintf(stderr, "INFO: Firmware version requested.\r\n");
		send_version_request();
		break;
	case 'a':
		if (module_data.state != module_idle) {
			fprintf(stderr, "INFO: Abort request sent.\r\n");
			send_abort_request();
			module_data.state = module_aborting;
		}
		else {
			fprintf(stderr, "INFO: Abort is no-op when no computation is in progress.\r\n");
		}
		break;
	case 'r':
		if (module_data.state != module_idle) {
			fprintf(stderr, "ERROR: Cannot reset counter, computation is currently in progress.\r\n");
		}
		else {
			fprintf(stderr, "INFO: Chunk reset request.\r\n");
			module_data.chunk_id = 0;
		}
		break;
	case '1':case '2':case '3':case '4': case '5': {
		if (module_data.state == module_idle) {
			int const tasks = 10 * (command - '0');
			send_message_compute(tasks);
			module_data.state = module_starting;
			fprintf(stderr, "INFO: Started computation of %d tasks.\r\n", tasks);
		}
		else {
			fprintf(stderr, "INFO: Cannot issue new command, a computation is already in progress.\r\n");
		}
		break;
	}

	default:
		fprintf(stderr, "WARN: Command '%c' is not recognized! Ignored.\r\n", command);
	}
}

void handle_message(message msg) {
	if (!message_checksum_ok(&msg)) {
		fprintf(stderr, "WARN: Incomming message has incorrect checksum.\r\n");
	}

	switch (msg.type) {
	case MSG_VERSION: {
		msg_version const* const version = &msg.data.version;
		fprintf(stderr, "INFO: Nucleo firmware version %d.%d.%d\r\n"
			, version->major, version->minor, version->patch);
		break;
	}
	case MSG_STARTUP: {

		char buffer[STARTUP_MSG_LEN + 1];
		memcpy(buffer, msg.data.startup.message, STARTUP_MSG_LEN);
		buffer[STARTUP_MSG_LEN] = '\0';
		fprintf(stderr, "INFO: Nucleo reporting for duty. Startup message: '%s'.\r\n", buffer);
		module_data.chunk_id = 0;
		module_data.state = module_idle;
		break;
	}
	case MSG_COMPUTE_DATA: {
		msg_compute_data const* const data = &msg.data.compute_data;
		fprintf(stderr, "INFO: Current progress: Chunk %3d, task %2d => result %d.\r\n",
			data->chunk_id, data->task_id, data->result);
		break;
	}

	case MSG_DONE:
		fprintf(stderr, "INFO: Nucleo finished entire chunk %d.\r\n", module_data.chunk_id);
		++module_data.chunk_id;
		module_data.state = module_idle;
		break;

	case MSG_ABORT:
		fprintf(stderr, "WARN: Nucleo aborted computation on its own. Leaving.\r\n");
		module_data.state = module_idle;
		thread_data.quit = true;
		break;
	case MSG_ERROR:
		fprintf(stderr, "WARN: Nucleo encountered error.\r\n");
		module_data.state = module_idle;
		break;

	case MSG_OK:
		fprintf(stderr, "INFO: Nucleo approves.\r\n");
		switch (module_data.state) {
		case module_starting:
			fprintf(stderr, "INFO: Computation started.\r\n");
			module_data.state = module_computing;
			break;
		case module_aborting:
			fprintf(stderr, "INFO: Computation aborted.\r\n");
			module_data.state = module_idle;
			break;
		}
		break;

	default:
		assert(false);
	}

}

/* Perform initialization tasks like opening serial ports, seting raw mode etc.
 Return true on success, false on failure. */
bool startup(int argc, char** argv) {

	assert(argc == 2);

	fprintf(stderr, "DEBUG: Opening serial port...\n");
	module_data.file_descriptor = open(argv[1], O_RDWR | O_NOCTTY | O_SYNC);
	assert(module_data.file_descriptor != -1);

	/* Enable non-blocking mode for both stdin as well as input from the module. */
	fprintf(stderr, "DEBUG: Configuring serial port...\n");
	assert(0 == set_file_nonblocking(fileno(stdin)));
	assert(0 == set_file_nonblocking(module_data.file_descriptor));
	configure_serial(module_data.file_descriptor);

	terminal_raw_mode(true);

	/* Create ringbuffers for sent commands and expected acknowledgements. This way communication
	is safe even if the module would delay sending acknowledgements. It is still necessary to send
	ACKs in order they are expected. */
	messages = create_queue(64);

	return true;
}

int main(int argc, char** argv) {

	if (!startup(argc, argv)) {
		fprintf(stderr, "ERROR: Cannot open serial port %s!\n", argv[1]);
		return EXIT_FAILURE;
	}
	fprintf(stderr, "DEBUG: Startup successful.\r\n");

	thrd_t listening_thread;
	if (thrd_success != thrd_create(&listening_thread, &serial_input_thread, &thread_data)) {
		fprintf(stderr, "ERROR: Cannot start thread to read from serial port. Exiting!\r\n");
		terminal_raw_mode(false);
		return EXIT_FAILURE;
	}

	/*Main loop is based on the same variable as estimating thread.
	Should it switch to false, both threads will exit loops. */
	for (; !thread_data.quit;) {

		poll_stdin();

		if (get_queue_size(messages) == 0) {
			continue;
		}

		handle_message(pop_from_queue(messages));
	}

	if (get_queue_size(messages) > 0) { //Print message if there were still messages pending 
		fprintf(stderr, "WARN: Remaining %d messages!\r\n", get_queue_size(messages));
	}

	/*Cleanup resources first and postpone thread joining. There are up to 5s of
	delay, since the estimation thread has to wake up first */
	delete_queue(messages);
	terminal_raw_mode(false);
	printf("\n\n");

	fprintf(stderr, "INFO: Closing serial port.\n");
	close(module_data.file_descriptor);

	if (thrd_join(listening_thread, NULL) != 0) {
		fprintf(stderr, "ERROR: Cannot join listening thread!\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
