/*
* Semestral assignment - Nucleo side
* Vojtìch Michal
* Compilable with C++14 standard; extended from HW08, HW09 and HW10
*/

#include <algorithm>
#include <array>
#include <cstdint>

#include <LowPowerTimer.h>
#include <Serial.h>
#include <InterruptIn.h>

#include "protocol.h"
#include "julia_computer.hpp"


namespace {

	constexpr uint8_t VERSION_MAJOR = 4, VERSION_MINOR = 2, VERSION_PATCH = 0;

	constexpr char startup_string[] = "This4uHeli";
	constexpr int init_baudrate = 115200;


	mbed::Serial pc{ USBTX, USBRX, init_baudrate };
	mbed::LowPowerTimer timer;

	/*Wrapper structure for time operations. Relies on microsecond accuracy of mbed library.*/
	struct Duration {
		//Internally stores time in microseconds
		unsigned us;

		constexpr static Duration from_ms(unsigned ms) { return { ms * 1000 }; }
		constexpr static Duration from_us(unsigned us) { return { us }; }

		static Duration now() { return Duration::from_us(timer.read_us()); }

		constexpr bool operator<(Duration rhs) const { return us < rhs.us; }
		constexpr auto operator-(Duration rhs) const { return Duration::from_us(us - rhs.us); }

		/* Returns true iff the duration 'time' already elapsed since 'this' timestamp. */
		bool time_elapsed(Duration time) const {
			return time < now() - *this;
		}

	};

	void wait(Duration const d) {
		Duration const start = Duration::now();
		for (; !start.time_elapsed(d););
	}

	constexpr Duration operator""_ms(std::uint64_t ms) { return Duration::from_ms(ms); }
	constexpr Duration operator""_us(std::uint64_t us) { return Duration::from_us(us); }


	struct ButtonManager {

		mbed::InterruptIn button{ PC_13 };
		volatile bool been_pressed_ = false;

		ButtonManager() {
			button.rise(mbed::callback(this, &ButtonManager::release));
		}

		void release() {
			been_pressed_ = true;
		}

		bool reset_if_pressed() {
			if (been_pressed_) {
				been_pressed_ = false;
				return true;
			}
			return false;
		}
	};


	template<typename T, unsigned capacity>
	class ringbuffer {

		std::array<T, capacity> storage;
		unsigned read_ = 0, write_ = 0;

	public:

		int size() const { return write_ - read_; }

		bool can_fit(unsigned size) const {
			return this->size() + size < capacity;
		}
		bool can_read(unsigned size) {
			return this->size() >= size;
		}

		void write(T const value) {
			storage[write_ % capacity] = value;
			++write_;
		}

		T read() {
			T const ret = peek();
			++read_;
			return ret;
		}

		T peek() const {
			return storage[read_ % capacity];

		}

		bool empty() const {
			return size() == 0;
		}
	};

	ringbuffer<uint8_t, 256> tx_buffer;
	ringbuffer<uint8_t, 256> rx_buffer;

	void serial_rx_isr() {
		while (pc.readable() && rx_buffer.can_fit(1))
			rx_buffer.write(pc.getc());
	}

	void serial_tx_isr() {
		if (tx_buffer.empty()) {
			pc.attach(0, mbed::Serial::TxIrq);
		}
		else {
			while (!pc.writeable());
			pc.putc(tx_buffer.read());
		}
	}



	void send_startup() {
		message startup = { .type = MSG_STARTUP };
		std::copy(std::begin(startup_string), std::prev(std::end(startup_string))
			, std::begin(startup.data.startup.message));
		message_calculate_checksum(&startup);
		message_enqueue(&startup);
	}

	void send_version() {
		message msg{ .type = MSG_VERSION };
		msg.data.version.major = VERSION_MAJOR;
		msg.data.version.minor = VERSION_MINOR;
		msg.data.version.patch = VERSION_PATCH;

		message_calculate_checksum(&msg);
		message_enqueue(&msg);
	}

	void send_abort_request() {
		message msg = { .type = MSG_ABORT };
		message_calculate_checksum(&msg);
		message_enqueue(&msg);
	}

	void send_ok() {
		message msg = { .type = MSG_OK };
		message_calculate_checksum(&msg);
		message_enqueue(&msg);
	}

}

extern "C" void message_enqueue(message const* msg) {
	int const required_size = message_size(static_cast<message_type>(msg->type));
	uint8_t buffer[sizeof(message)];
	message_decompose(msg, buffer, sizeof buffer);
	for (; !tx_buffer.can_fit(required_size););

	for (int i = 0; i < required_size; ++i)
		tx_buffer.write(buffer[i]);

	pc.attach(&serial_tx_isr, mbed::Serial::TxIrq);
}

int main() {
	timer.start();
	send_startup();

	pc.attach(&serial_rx_isr, mbed::Serial::RxIrq);

	ButtonManager buttonManager;
	JuliaSetComputer julia;

	for (; ;) {

		if (buttonManager.reset_if_pressed()) {
			send_abort_request();
			julia.state() = module_state::idle;
		}

		if (!rx_buffer.empty()) { //Handle incomming messages
			int const size = message_size(static_cast<message_type>(rx_buffer.peek()));
			if (rx_buffer.can_read(size)) {
				uint8_t buffer[sizeof(message)];
				std::generate_n(buffer, size, [&]() { return rx_buffer.read(); });
				message msg = message_parse(buffer, sizeof buffer);

				switch (msg.type) {
				case MSG_GET_VERSION:
					send_version();
					break;
				case MSG_SET_COMPUTE:
					julia.update_settings(msg.data.set_compute);
					send_ok();
					break;

				case MSG_COMPUTE:
					julia.start_computation(msg.data.compute);
					send_ok();
					break;

				case MSG_ABORT: //Interrupt calculation
					send_ok();
					julia.state() = module_state::idle;
					send_abort_request();
					break;
				case MSG_COMM:
					wait(50_ms);
					pc.baud(msg.data.comm.baudrate);
					wait(50_ms);
					send_ok();
					break;
				}
			}
		}

		switch (julia.state()) {
		case module_state::idle:
			break;
		case module_state::computing: {

			message msg = { .type = MSG_COMPUTE_DATA };
			msg.data.compute_data = julia.compute_next();

			message_calculate_checksum(&msg);
			message_enqueue(&msg);
			break;

		}
		case module_state::finished: {
			julia.state() = module_state::idle;
			message done = { .type = MSG_DONE };
			message_calculate_checksum(&done);
			message_enqueue(&done);
		}
		}
	}
}