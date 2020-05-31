/*
* Semestral assignment - Master computer side
* Vojtìch Michal
* Compilable with GNU11 C; extended from HW08, HW09 and HW10
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
#include <time.h>

#include "xwin_sdl.h"
#include "protocol.h"
#include "ringbuffer.h"
#include "fractal_drawer.h"
#include "juliaset.h"

//How long (in sec) should the program hold off when communication stops.
//This may occur namely because of disconnect. Program then exits
#define COMMUNICATION_TIMEOUT  8
#define COMMUNICATION_TIMEOUT_WARN 5

char const* const basic_stdin_help = "Basic help:\r\n"
"h - Print this help message.\r\n"
"q - Abort computation and exit the program.\r\n"
"g - Request firmware version from connected module.\r\n"
"a - Abort current computation.\r\n"
"c - Clear Frame buffer. Effectively fill window with black.\r\n"
"r - Reset chunk state recordings - effectively resets all chunks to dark, but does not clear the frame buffer.\r\n"
"p - Perform the process natively on the computer using current configuration.\r\n"
"i - Transmit current settings to the connected worker module.\r\n"
"s - Start (or possibly resume if it was only interrupted) the computation.\r\n"
"\t\tImmediatelly draws intermediate results to the screen.\r\n"
"e - Export to ppm.\r\n"
"\r\n"
"Submenus:\r\n"
"b - Configure the communication baudrate.\r\n"
"d - Configure drawing process.\r\n"
"f - Move freely around the picture.\r\n"
"x - Move constant.\r\n";

char const* const baudrate_help = "Choose one of the following baudrates for communication.\r\n"
"1 - 110\r\n"
"2 - 9600\r\n"
"3 - 19200\r\n"
"4 - 115200 (default and reset state).\r\n"
"5 - 230400\r\n"
"n - view current baudrate.\r\n"
"q - return to basic menu.\r\n"
"h - print this message.\r\n";

char const* const drawing_config_help = "Drawing configuration.\r\n"
"q - return to the main menu.\r\n"
"\r\n"
"Chunk selection policy, e.g. by what criteria are unfinished chunks selected for computation:\r\n"
"    r - Random - simply random...\r\n"
"    s - Sequential - topmost and then leftmost empty chunk is selected.\r\n";

char const* const free_move_help = "Free move.\r\n"
"q  return to the main menu\r\n"
"r  restore default bounds\r\n"
"+  zoom in\r\n"
"-  zoom out\r\n"
"w  move up\r\n"
"a  move left\r\n"
"s  move down\r\n"
"d  move right\r\n";

char const* const constant_move_help = "Move constant.\r\n"
"q  return to the main menu\r\n"
"r  restore default constant\r\n"
"w  move up\r\n"
"a  move left\r\n"
"s  move down\r\n"
"d  move right\r\n";

//Default configuration

/*Coefficients by which camera moves and/or zooms the picture. Used as multiplicators.*/
float const zoom_coefficient = 0.8f;
float const move_coeeficient = 0.2f;

int const default_width = 320;
int const default_height = 240;
int const default_precision = 40;
int const default_chunk_rows = 10;
int const default_chunk_cols = 20;

my_complex const max_top_left = { -1.6, 1.1 }, max_bot_right = { 1.6, -1.1 };
my_complex const default_fractal_constant = { 0.0, 0.75 };

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
	speed_t baudrate;

} module_data = { .state = module_idle, .chunk_id = 0, .baudrate = B115200 };

struct {

	bool volatile quit;

} thread_data = { .quit = false };

//Describes, how input from tty shall be interpreted. Allows existence of submenus
enum tty_state {
	tty_basic,
	tty_baudrate_selection,
	tty_drawing_config,
	tty_free_move,
	tty_move_constant
} tty_state = tty_basic;

void terminal_raw_mode(bool raw) {
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

/* Conversion function from symbolic constant*/
static int baudrate_to_int(speed_t const s) {
	switch (s) {
	case B0: return 0;
	case B50: return 50;
	case B75: return 75;
	case B110: return 110;
	case B134: return 134;
	case B150: return 150;
	case B200: return 200;
	case B300: return 300;
	case B600: return 600;
	case B1200: return 1200;
	case B1800: return 1800;
	case B2400: return 2400;
	case B4800: return 4800;
	case B9600: return 9600;
	case B19200: return 19200;
	case B38400: return 38400;
	case B57600: return 57600;
	case B115200: return 115200;
	case B230400: return 230400;
	default:
		return -1;

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

	termios.c_lflag &= ~IEXTEN; // Make number 22 (character SYN) work
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
	cfsetispeed(&termios, module_data.baudrate);
	cfsetospeed(&termios, module_data.baudrate);

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

void send_message_compute() {
	message msg = { .type = MSG_COMPUTE };
	msg.data.compute = fractal_get_next_chunk();
	message_calculate_checksum(&msg);
	message_enqueue(&msg);
}

void send_connection_confirmation() {
	message msg = { .type = MSG_CONN_OK };
	message_calculate_checksum(&msg);
	message_enqueue(&msg);
}

void message_enqueue(message const* msg) {
	uint8_t buffer[sizeof(message)];
	message_decompose(msg, buffer, sizeof buffer);
	write(module_data.file_descriptor, buffer, message_size(msg->type));
}

/* Main function for the thread reading input from the serial port.
Reads characters in a 'never' ending loop and glues them together into sensible messages.*/
static int serial_input_thread() {
	fprintf(stderr, "INFO: Serial port listening thread started.\r\n");

	uint8_t buffer[sizeof(message)];
	unsigned write_index = 0;

	memset(buffer, 0, sizeof buffer);

	for (; !thread_data.quit;) {
		if (read(module_data.file_descriptor, &buffer[write_index], 1) == 1) {
			if (++write_index == 1) {//When reading first byte, check whether it makes sense
				if (!message_is_valid_type(buffer[0])) {
					//Received garbage, skip it and wait for sensible data to arrive
					fprintf(stderr, "WARN: Discarding received byte %x.\r\n", buffer[0]);
					write_index = 0;
					continue;
				}
			}

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

/*Main function for thread that updates the SDL window.*/
int redrawing_thread() {
	fprintf(stderr, "INFO: Redrawing thread started.\r\n");

	int const FPS = 100; //bound to 100FPS for now
	for (; !thread_data.quit;) {

		fractal_redraw();
		usleep(1000 * (1000 / FPS));
	}

	fprintf(stderr, "INFO: Redrawing thread exits.\r\n");
	return 0;
}

/* Select new_speed as the serial port frequency and communicate this to the module. */
void switch_baudrates(speed_t const new_speed) {

	module_data.baudrate = new_speed;
	message msg = { .type = MSG_COMM };
	msg.data.comm.baudrate = baudrate_to_int(new_speed);
	assert(msg.data.comm.baudrate != -1);

	message_calculate_checksum(&msg);
	message_enqueue(&msg);
	usleep(20 * 1000);

	struct termios termios;
	memset(&termios, 0, sizeof termios);

	assert(tcgetattr(module_data.file_descriptor, &termios) == 0);

	cfsetispeed(&termios, module_data.baudrate);
	cfsetospeed(&termios, module_data.baudrate);
	assert(tcsetattr(module_data.file_descriptor, TCSANOW, &termios) == 0);

	fprintf(stderr, "INFO: Selecting %d baud as the communication speed.\r\n"
		, baudrate_to_int(module_data.baudrate));
	usleep(20 * 1000);

}


/* This function encapsulates reading from stdin when the program is in baudrate selection mode. */
void poll_baudrate() {


	int const command = fgetc(stdin);
	if (command == -1) {
		return;
	}

	speed_t speeds[5] = { B110, B9600, B19200, B115200, B230400 };

	switch (command) {
	case 'h':
		fprintf(stderr, baudrate_help);
	case 'n':
		fprintf(stderr, "INFO: Serial communication currently uses frequency %d bps.\r\n", baudrate_to_int(module_data.baudrate));
		break;
	case '1': case '2': case '3': case '4': case '5':
		switch_baudrates(speeds[command - '1']);
		//fallthrough
	case 'q':
		tty_state = tty_basic;
		fprintf(stderr, "INFO: Returning to basic menu.\r\n");
		break;
	default:
		fprintf(stderr, "ERROR: This is not a valid option.\r\n");

	}

}

/* Encapsulate reading from stdin when the user is configuring drawing mode. */
void poll_drawing_config() {

	int const command = fgetc(stdin);

	if (command == -1) {
		return; //ignore empty read
	}

	switch (command) {
	case 's': case 'r':
		fractal_set_selection_policy(command == 's' ? policy_sequential : policy_random);
		fprintf(stderr, "INFO: Selected %s policy.\r\n", command == 's' ? "sequential" : "random");
		break;

	case 'q':
		tty_state = tty_basic;
		fprintf(stderr, "INFO: Returning to basic menu.\r\n");
		break;
	default:
		fprintf(stderr, "ERROR: This is not a valid option.\r\n");
	}

}

//Zoom in or out depending on the op. Simply pushes edges of the visible rectangle 
//toward or away freom the center of the screen.
void zoom(char const op) {
	assert(op == '+' || op == '-');
	my_complex const middle = fractal_get_center();

	my_complex top_left = sub(fractal_get_edge(bound_topleft), middle),
		bot_right = sub(fractal_get_edge(bound_botright), middle);

	float const scalar = op == '+' ? zoom_coefficient : 1 / zoom_coefficient;
	top_left = scalar_mul(top_left, scalar);
	bot_right = scalar_mul(bot_right, scalar);

	fractal_set_edge(bound_botright, add(bot_right, middle));
	fractal_set_edge(bound_topleft, add(top_left, middle));
	fractal_set_all_chunks_unseen();
	fractal_compute_locally();
}

//Moves edges of the visible rectangle and thus translates our view of the complex plane
void move(char const op) {

	my_complex const visible = sub(fractal_get_edge(bound_topleft),
		fractal_get_edge(bound_botright));

	my_complex dx = { fabs(visible.re), 0 };
	my_complex dy = { 0, fabs(visible.im) };
	dx = scalar_mul(dx, move_coeeficient);
	dy = scalar_mul(dy, move_coeeficient);

	my_complex displacement = { 0.0f,0.0f };
	switch (op) {
	case 'w':
		displacement = dy;
		break;
	case 's':
		displacement = negate(dy);
		break;
	case 'a':
		displacement = negate(dx);
		break;
	case 'd':
		displacement = dx;
		break;
	}

	fractal_set_edge(bound_topleft, add(fractal_get_edge(bound_topleft), displacement));
	fractal_set_edge(bound_botright, add(fractal_get_edge(bound_botright), displacement));
	fractal_set_all_chunks_unseen();
	fractal_compute_locally();
}

/* Encapsulates reading from stdin when the camera is flying free above the complex plane. */
void poll_free_move() {

	int const command = fgetc(stdin);

	if (command == -1) {
		return; //Ignore "empty" read
	}

	switch (command) {
	case 'r':
		fprintf(stderr, "Restoring default bounds.\r\n");
		fractal_set_edge(bound_topleft, max_top_left);
		fractal_set_edge(bound_botright, max_bot_right);
		fractal_set_all_chunks_unseen();
		fractal_clear_buffer();
		fractal_compute_locally();
		break;
	case 'q':
		tty_state = tty_basic;
		fprintf(stderr, "Returning to default menu.\r\n");
		return;
	case '+': case '-':
		zoom(command);
		break;
	case 'w': case 's': case 'a': case'd':
		move(command);
		break;
	default:
		fprintf(stderr, "ERROR: This is not a valid option.\r\n");
	}
	my_complex const center = fractal_get_center();
	my_complex const visible = sub(fractal_get_edge(bound_topleft),
		fractal_get_edge(bound_botright));

	fprintf(stderr, "You are now centered on [%.4f, %.4f]. ", center.re, center.im);
	fprintf(stderr, "Visible area is a rectangle [%.4f, %.4f].\r\n"
		, fabs(visible.re), fabs(visible.im));

}

/* Encapsulates reading from stdin when commads regarding the constant C positioning are expected. */
void poll_move_constant() {

	int const command = fgetc(stdin);

	if (command == -1) {
		return; //Ignore "empty" read
	}

	switch (command) {
	case 'r':
		fprintf(stderr, "Restoring default constant.\r\n");
		fractal_set_constant(default_fractal_constant);
		fractal_set_all_chunks_unseen();
		fractal_compute_locally();
		break;
	case 'q':
		tty_state = tty_basic;
		fprintf(stderr, "Returning to default menu.\r\n");
		return;
	case 'w': case 's': case 'a': case'd': {


		my_complex const dx = { 0.01, 0 };
		my_complex const dy = { 0, 0.01 };

		my_complex displacement = { 0.0f,0.0f };
		switch (command) {
		case 'w':
			displacement = dy;
			break;
		case 's':
			displacement = negate(dy);
			break;
		case 'a':
			displacement = negate(dx);
			break;
		case 'd':
			displacement = dx;
			break;
		}

		fractal_set_constant(add(fractal_get_constant(), displacement));
		fractal_set_all_chunks_unseen();
		fractal_compute_locally();
		break;
	}
	default:
		fprintf(stderr, "ERROR: This is not a valid option.\r\n");
		return;
	}
	my_complex const center = fractal_get_constant();

	fprintf(stderr, "Current position is [%.4f, %.4f].\r\n", center.re, center.im);

}

//Encapsulates the general puspose main menu 
void poll_stdin() {
	//Character read from the stdin. May be -1 if nothing was readable (stdin is in nonblocking mode).
	int const command = fgetc(stdin);

	if (command == -1) {
		return; //ignore 'empty' input.
	}

	switch (command) {
	case 'h':
		fprintf(stderr, basic_stdin_help);
		break;
	case 'q':
		fprintf(stderr, "INFO: Requested exit command.\r\n");
		thread_data.quit = true;
		//Notify Nucleo that we don't like it anymore (resets Nucleo)
		message exit_notification = { .type = MSG_RESET };
		message_calculate_checksum(&exit_notification);
		message_enqueue(&exit_notification);
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
	case 'c':
		if (module_data.state != module_idle) {
			fprintf(stderr, "ERROR: Cannot clear buffers, computation is currently in progress.\r\n");
		}
		else {
			fractal_clear_buffer();
			fprintf(stderr, "INFO: Cleared frame buffer.\r\n");
		}
		break;
	case 'r':
		if (module_data.state != module_idle) {
			fprintf(stderr, "ERROR: Cannot reset counter, computation is currently in progress.\r\n");
		}
		else {
			fprintf(stderr, "INFO: Chunk reset request.\r\n");
			fractal_set_all_chunks_unseen();
			module_data.chunk_id = 0;
		}
		break;
	case 'p':
		if (module_data.state != module_idle) {
			fprintf(stderr, "ERROR: Nucleo is already computing.\r\n");
		}
		else if (fractal_finished()) {
			fprintf(stderr, "WARN: Nothing to do. You must first reset chunks.\r\n");
		}
		else {
			fprintf(stderr, "INFO : Computing fractal locally.\r\n");
			fractal_compute_locally();
			fprintf(stderr, "INFO: Done.\r\n");
		}
		break;
	case 'i':
		if (module_data.state == module_idle) {
			message msg = { .type = MSG_SET_COMPUTE };
			msg.data.set_compute = fractal_get_settings();
			message_calculate_checksum(&msg);
			message_enqueue(&msg);
			fprintf(stderr, "INFO: Transmitted configuration.\r\n");
		}
		else {
			fprintf(stderr, "INFO: Cannot issue new command, a computation is already in progress.\r\n");
		}
		break;

	case 's':
		if (module_data.state != module_idle) {
			fprintf(stderr, "ERROR: Cannot issue command - Nucleo is already computing.\r\n");
		}
		else if (fractal_finished()) {
			fprintf(stderr, "WARN: Nothing to do. You must first reset chunks.\r\n");
		}
		else {
			send_message_compute();
			module_data.state = module_starting;
			fprintf(stderr, "INFO: Started computation.\r\n");
		}
		break;

	case 'b':
		if (module_data.state != module_idle) {
			fprintf(stderr, "ERROR: The module must be in idle state to change baudrate.\r\n");
		}
		else {
			tty_state = tty_baudrate_selection;
			fprintf(stderr, "INFO: Switching to baudrate selection mode.\r\n");
			fprintf(stderr, baudrate_help);
		}
		break;

	case 'd':
		if (module_data.state != module_idle) {
			fprintf(stderr, "ERROR: The module must be in idle state to configure drawing.\r\n");
		}
		else {
			tty_state = tty_drawing_config;
			fprintf(stderr, "INFO: Switching to drawing configuration mode.\r\n");
			fprintf(stderr, drawing_config_help);
		}
		break;

	case 'z':
		if (module_data.state != module_idle) {
			fprintf(stderr, "ERROR: The module must be in idle state to configure drawing.\r\n");
		}
		else {
			tty_state = tty_drawing_config;
			fprintf(stderr, "INFO: Switching to drawing configuration mode.\r\n");
			fprintf(stderr, drawing_config_help);
		}
		break;
	case 'f':
		if (module_data.state != module_idle) {
			fprintf(stderr, "ERROR: The module must be in idle state to enter free move.\r\n");
		}
		else {
			tty_state = tty_free_move;
			fprintf(stderr, "INFO: Switching to free move mode.\r\n");
			fprintf(stderr, free_move_help);
		}
		break;
	case 'x':
		if (module_data.state != module_idle) {
			fprintf(stderr, "ERROR: The module must be in idle state to move constant.\r\n");
		}
		else {
			tty_state = tty_move_constant;
			fprintf(stderr, "INFO: Switching to constant move mode.\r\n");
			fprintf(stderr, constant_move_help);
		}
		break;
	case 'e':
		if (module_data.state != module_idle) {
			fprintf(stderr, "ERROR: The module must be in idle state to export picture.\r\n");
		}
		save_to_ppm();
		break;

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
		/*fprintf(stderr, "INFO: Current progress: Chunk %3d at [%2d, %2d] ... %2d iterations.\r\n",
			data->cid, data->i_re, data->i_im, data->iter);
			*/
		fractal_add_point(data->cid, data->i_re, data->i_im, data->iter);
		break;
	}

	case MSG_DONE:
		fprintf(stderr, "INFO: Nucleo finished entire chunk.\r\n");
		fractal_finish_chunk();
		if (fractal_finished()) {
			fprintf(stderr, "INFO: Work done, whole fractal calculated.\r\n");
			module_data.state = module_idle;
		}
		else {
			fprintf(stderr, "INFO: Starting next chunk.\r\n");
			send_message_compute();
		}
		break;

	case MSG_ABORT:
		fprintf(stderr, "WARN: Nucleo signaled abort.\r\n");
		module_data.state = module_idle;
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
		default:
			break;
		}
		break;
	case MSG_CONN_TEST: {
		fprintf(stderr, "INFO: Received connection test.\r\n");
		send_connection_confirmation();
		break;
	}
	default:
		assert(false);
	}

}

/* Perform initialization tasks like opening serial ports, seting raw mode etc.
 Return true on success, false on failure. */
bool startup(int argc, char** argv) {

	srand(time(0));

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

	fractal_initialize(default_width, default_height, default_precision, default_chunk_cols,
		default_chunk_rows, max_top_left, max_bot_right, default_fractal_constant);

	return true;
}

/* Checks command line arguments, their count, order etc. Returns false on error.
  If an error is detected, prints simple help. Does not modify program state.*/
bool check_args(int argc, char** argv) {
	const char* const help = "Usage: %s serial_port\n\n"
		"This application is a driver for Julia set computation using device connected to\n"
		"given serial port. Forwards commands from the user and draws intermediate results\n"
		"to screen. Contains help (press h within the program).\r\n";

	if (argc != 2) {
		fprintf(stderr, help, argv[0]);
		return false;
	}
	//More checking done in function startup
	return true;
}


int main(int argc, char** argv) {

	if (!check_args(argc, argv)) {
		return EXIT_FAILURE;
	}

	if (!startup(argc, argv)) {
		fprintf(stderr, "ERROR: Cannot open serial port %s!\n", argv[1]);
		return EXIT_FAILURE;
	}
	fprintf(stderr, "DEBUG: Startup successful. Communication speed is %d bps.\r\n"
		, baudrate_to_int(module_data.baudrate));

	thrd_t listening_thread;
	if (thrd_success != thrd_create(&listening_thread, &serial_input_thread, &thread_data)) {
		fprintf(stderr, "ERROR: Cannot start thread to read from serial port. Exiting!\r\n");
		terminal_raw_mode(false);
		fractal_cleanup();
		return EXIT_FAILURE;
	}


	thrd_t redrawing;
	if (thrd_success != thrd_create(&redrawing, &redrawing_thread, &thread_data)) {
		fprintf(stderr, "ERROR: Cannot start thread to read from serial port. Exiting!\r\n");
		terminal_raw_mode(false);
		thread_data.quit = true;
		thrd_join(listening_thread, NULL);
		fractal_cleanup();
		return EXIT_FAILURE;
	}


	time_t last_received = time(NULL);

	/*Main loop is based on the same variable as estimating thread.
	Should it switch to false, both threads will exit loops. */
	for (; !thread_data.quit;) {

		time_t const now = time(NULL); //Second resolution is just enough
		if (now - last_received > COMMUNICATION_TIMEOUT_WARN) {
			static time_t last_sent = 0;
			if (now - last_received > COMMUNICATION_TIMEOUT) {
				fprintf(stderr, "Communication timed out. Nucleo disconnected.\r\n");
				thread_data.quit = true; //Leave the program, our computational unit is dead
			}
			else if (now - last_sent > 0) {
				fprintf(stderr, "Communication was quiet for too long.\r\n");
				message msg = { .type = MSG_CONN_TEST };
				message_calculate_checksum(&msg);
				message_enqueue(&msg);
				last_sent = now;
			}

		}

		switch (tty_state) {
		case tty_basic:
			poll_stdin();
			break;
		case tty_baudrate_selection:
			poll_baudrate();
			break;
		case tty_drawing_config:
			poll_drawing_config();
			break;
		case tty_free_move:
			poll_free_move();
			break;
		case tty_move_constant:
			poll_move_constant();
			break;
		}

		if (get_queue_size(messages) == 0) {
			continue;
		}
		last_received = now;

		handle_message(pop_from_queue(messages));
	}

	if (get_queue_size(messages) > 0) { //Print message if there were still messages pending 
		fprintf(stderr, "WARN: Remaining %d unparsed messages!\r\n", get_queue_size(messages));
	}


	/*Cleanup resources first and postpone thread joining.*/
	delete_queue(messages);
	terminal_raw_mode(false);
	printf("\n\n");

	fprintf(stderr, "INFO: Closing serial port.\n");
	close(module_data.file_descriptor);

	if (thrd_join(listening_thread, NULL) != 0 || thrd_join(redrawing, NULL) != 0) {
		fprintf(stderr, "ERROR: Cannot join listening thread!\n");
		return EXIT_FAILURE;
	}
	fractal_cleanup();
	fprintf(stderr, "INFO: Program successfully deinitialized.\n");
	return EXIT_SUCCESS;
}
