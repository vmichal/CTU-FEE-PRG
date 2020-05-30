
#include "protocol.h"
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int message_payload_size(message_type const type) {
	switch (type) {
	case MSG_OK: case MSG_ERROR: case MSG_ABORT:
	case MSG_DONE: case MSG_GET_VERSION:
		return 0;
	case MSG_VERSION:
		return 3;
	case MSG_STARTUP:
		return STARTUP_MSG_LEN;
	case MSG_COMPUTE:
		return 3 + 2 * sizeof(float);
	case MSG_COMPUTE_DATA:
		return 4;
	case MSG_SET_COMPUTE:
		return 1 + 4 * sizeof(float);
	case MSG_COMM:
		return 4+1;

	default:
		fprintf(stderr, "Communication integrity error - Nucleo still transmits data from "
			"previous application. Reset it, please.\r\n");
		exit(EXIT_FAILURE);
	}
}

int message_size(message_type const type) {
	//Two bytes for type and checksum + size of payload
	return 2 + message_payload_size(type);
}

// fill the given buf by the message msg (marshaling);
void message_decompose(const message* msg, uint8_t* buf, int size) {
	assert(msg && buf && message_size(msg->type) <= size);
	buf[0] = msg->type;
	buf[message_size(msg->type) - 1] = msg->cksum;
	switch (msg->type) {
	case MSG_VERSION:
		buf[1] = msg->data.version.major;
		buf[2] = msg->data.version.minor;
		buf[3] = msg->data.version.patch;
		break;
	case MSG_STARTUP:
		memcpy(&buf[1], msg->data.startup.message, STARTUP_MSG_LEN);
		break;

	case MSG_SET_COMPUTE:
		memcpy(&(buf[1 + 0 * sizeof(float)]), &(msg->data.set_compute.c_re), sizeof(float));
		memcpy(&(buf[1 + 1 * sizeof(float)]), &(msg->data.set_compute.c_im), sizeof(float));
		memcpy(&(buf[1 + 2 * sizeof(float)]), &(msg->data.set_compute.d_re), sizeof(float));
		memcpy(&(buf[1 + 3 * sizeof(float)]), &(msg->data.set_compute.d_im), sizeof(float));
		buf[1 + 4 * sizeof(float)] = msg->data.set_compute.n;
		break;

	case MSG_COMPUTE:
		buf[1] = msg->data.compute.cid; // cid
		memcpy(&(buf[2 + 0 * sizeof(float)]), &(msg->data.compute.re), sizeof(float));
		memcpy(&(buf[2 + 1 * sizeof(float)]), &(msg->data.compute.im), sizeof(float));
		buf[2 + 2 * sizeof(float) + 0] = msg->data.compute.n_re;
		buf[2 + 2 * sizeof(float) + 1] = msg->data.compute.n_im;
		break;

	case MSG_COMPUTE_DATA:
		buf[1] = msg->data.compute_data.cid;
		buf[2] = msg->data.compute_data.i_re;
		buf[3] = msg->data.compute_data.i_im;
		buf[4] = msg->data.compute_data.iter;
		break;
	case MSG_COMM:
		buf[1] = msg->data.comm.baudrate & 0xff;
		buf[2] = (msg->data.comm.baudrate >> 8) & 0xff;
		buf[3] = (msg->data.comm.baudrate >> 16) & 0xff;
		buf[4] = (msg->data.comm.baudrate >> 24) & 0xff;
		buf[5] = msg->data.comm.enable_burst;
		break;
	}
}

// parse the message from buf to msg (unmarshaling)
message message_parse(const uint8_t* buf, int size) {
	assert(message_size(buf[0]) <= size); //Make sure there is enough data
	message msg;
	msg.type = buf[0];
	msg.cksum = buf[message_size(msg.type) - 1];
	switch (msg.type) {
	case MSG_VERSION:
		msg.data.version.major = buf[1];
		msg.data.version.minor = buf[2];
		msg.data.version.patch = buf[3];
		break;
	case MSG_STARTUP:
		memcpy(msg.data.startup.message, &buf[1], STARTUP_MSG_LEN);
		break;

	case MSG_SET_COMPUTE:
		memcpy(&(msg.data.set_compute.c_re), &(buf[1 + 0 * sizeof(float)]), sizeof(float));
		memcpy(&(msg.data.set_compute.c_im), &(buf[1 + 1 * sizeof(float)]), sizeof(float));
		memcpy(&(msg.data.set_compute.d_re), &(buf[1 + 2 * sizeof(float)]), sizeof(float));
		memcpy(&(msg.data.set_compute.d_im), &(buf[1 + 3 * sizeof(float)]), sizeof(float));
		msg.data.set_compute.n = buf[1 + 4 * sizeof(float)];
		break;
	case MSG_COMPUTE: // type + chunk_id + nbr_tasks
		msg.data.compute.cid = buf[1];
		memcpy(&(msg.data.compute.re), &(buf[2 + 0 * sizeof(float)]), sizeof(float));
		memcpy(&(msg.data.compute.im), &(buf[2 + 1 * sizeof(float)]), sizeof(float));
		msg.data.compute.n_re = buf[2 + 2 * sizeof(float) + 0];
		msg.data.compute.n_im = buf[2 + 2 * sizeof(float) + 1];
		break;
	case MSG_COMPUTE_DATA:  // type + chunk_id + task_id + result
		msg.data.compute_data.cid = buf[1];
		msg.data.compute_data.i_re = buf[2];
		msg.data.compute_data.i_im = buf[3];
		msg.data.compute_data.iter = buf[4];
		break;
	case MSG_COMM:
		msg.data.comm.baudrate = buf[1] | (buf[2] << 8) | (buf[3] << 16) | (buf[4] << 24);
		msg.data.comm.enable_burst = buf[5];
		break;
	}
	return msg;

}

void message_calculate_checksum(message* msg) {
	unsigned checksum = 0;
	int const size = message_size(msg->type) - 1;
	uint8_t const* const view = (uint8_t const*)(void const*)msg;
	for (int i = 0; i < size; ++i)
		checksum += view[i];
	msg->cksum = checksum;
}

bool message_checksum_ok(message* msg) {
	uint8_t const stored_value = msg->cksum;
	message_calculate_checksum(msg);
	bool const result = stored_value == msg->cksum;
	msg->cksum = stored_value;
	return result;
}

