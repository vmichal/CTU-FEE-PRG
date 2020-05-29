
#include "protocol.h"
#include <assert.h>
#include <string.h>
#include <limits.h>


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
		return 4;
	case MSG_COMPUTE_DATA:
		return 5;

	default:
		assert(false);
	}
}

int message_size(message_type const type) {
	//Two bytes for type and checksum + size of payload
	return 2 + message_payload_size(type);
}

// fill the given buf by the message msg (marshaling);
void message_decompose(const message* msg, uint8_t* buf, int size) {
	assert(message_size(msg->type) <= size);
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
	case MSG_COMPUTE_DATA: {
		msg_compute_data const* const data = &msg->data.compute_data;
		buf[1] = data->chunk_id & 0xff;
		buf[2] = data->chunk_id >> CHAR_BIT;
		buf[3] = data->task_id & 0xff;
		buf[4] = data->task_id >> CHAR_BIT;
		buf[5] = data->result;
		break;
	}

	case MSG_COMPUTE: {
		msg_compute const* const data = &msg->data.compute;
		buf[1] = data->chunk_id & 0xff;
		buf[2] = data->chunk_id >> CHAR_BIT;
		buf[3] = data->nbr_tasks & 0xff;
		buf[4] = data->nbr_tasks >> CHAR_BIT;
		break;
	}
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
	case MSG_COMPUTE_DATA:
		msg.data.compute_data.chunk_id = buf[1] | (buf[2] << CHAR_BIT);
		msg.data.compute_data.task_id = buf[3] | (buf[4] << CHAR_BIT);
		msg.data.compute_data.result = buf[5];
		break;

	case MSG_COMPUTE:
		msg.data.compute.chunk_id = buf[1] | (buf[2] << CHAR_BIT);
		msg.data.compute.nbr_tasks = buf[3] | (buf[4] << CHAR_BIT);
		break;
	}
	return msg;

}

void message_calculate_checksum(message* msg) {
	unsigned char checksum = msg->type;
	int const size = message_size(msg->type) - 2;
	for (int i = 0; i < size; ++i)
		checksum ^= msg->data.startup.message[i];
	msg->cksum = checksum;
}

bool message_checksum_ok(message* msg) {
	uint8_t const stored_value = msg->cksum;
	message_calculate_checksum(msg);
	bool const result = stored_value == msg->cksum;
	msg->cksum = stored_value;
	return result;
}

