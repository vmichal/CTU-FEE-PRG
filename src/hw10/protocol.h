/*
 * File name: messages.h
 */

#ifndef __MESSAGES_H__
#define __MESSAGES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

 // Definition of the communication messages
typedef enum {
	MSG_OK = 37,               // ack of the received message
	MSG_ERROR,            // report error on the previously received command
	MSG_ABORT,            // abort - from user button or from serial port
	MSG_DONE,             // report the requested work has been done
	MSG_GET_VERSION,      // request version of the firmware
	MSG_VERSION,          // send version of the firmware as major,minor, patch level, e.g., 1.0p1
	MSG_STARTUP,          // init of the message (id, up to 9 bytes long string, cksum
	MSG_COMPUTE,          // request computation of a batch of tasks (chunk_id, nbr_tasks)
	MSG_COMPUTE_DATA,     // computed result (chunk_id, result)
	MSG_NBR
} message_type;

#define STARTUP_MSG_LEN 9

typedef struct {
	uint8_t major;
	uint8_t minor;
	uint8_t patch;
} msg_version;

typedef struct {
	uint8_t message[STARTUP_MSG_LEN];
} msg_startup;

typedef struct {
	uint16_t chunk_id;
	uint16_t nbr_tasks;
} msg_compute;

typedef struct {
	uint16_t chunk_id;
	uint16_t task_id;
	uint8_t result;
} msg_compute_data;

typedef struct {
	uint8_t type;   // message type
	union {
		msg_version version;
		msg_startup startup;
		msg_compute compute;
		msg_compute_data compute_data;
	} data;
	uint8_t cksum; // message command
} message;


// return the size of the message in bytes
int message_size(message_type msg_type);

// fill the given buf by the message msg (marhaling);
void message_decompose(const message* msg, uint8_t* buf, int size);

// parse the message from buf to msg (unmarshaling)
message message_parse(const uint8_t* buf, int size);

void message_calculate_checksum(message* msg);

void message_enqueue(message const* msg);

/* Returns true iff messages's checksum corresponds to the contained data. */
bool message_checksum_ok(message* msg);

#endif

/* end of messages.h */


#ifdef __cplusplus
}
#endif
