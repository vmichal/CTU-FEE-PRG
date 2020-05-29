/*
* hw10
* Vojtìch Michal
* Compilable with C++14 standard; extended from HW09
*/

#include <algorithm>
#include <array>
#include <cstdint>

#include <LowPowerTimer.h>
#include <Serial.h>
#include <DigitalOut.h>
#include <InterruptIn.h>

#include "protocol.h"

namespace {

	constexpr uint8_t VERSION_MAJOR = 4, VERSION_MINOR = 2, VERSION_PATCH = 0;

	constexpr char startup_string[] = "PRG-HW 10";
	constexpr int init_baudrate = 115200;


	/*I've chosen five pins for leds, the middle one (PA4) is green, others are red. */
	std::array<mbed::DigitalOut, 5> leds{ { PA_0, PA_1, PA_4, PB_0, PC_1 } };
	//Blue button bound to the board. Internally connected to Pin PC13
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
	constexpr Duration operator""_ms(std::uint64_t ms) { return Duration::from_ms(ms); }
	constexpr Duration operator""_us(std::uint64_t us) { return Duration::from_us(us); }

	/* Suspend execution for given time. */
	void wait(Duration const dur) {
		Duration const start = Duration::now();

		for (; !start.time_elapsed(dur););
	}

	struct ButtonManager {

		mbed::InterruptIn button{ PC_13 };
		volatile bool been_pressed_ = false;

		ButtonManager() {
			button.rise(mbed::callback(this, &ButtonManager::release));
		}

		void release() {
			been_pressed_ = true;
		}

		bool been_pressed() const { return been_pressed_; }
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



	void startup_sequence() {
		auto const blink_with_delay = [](mbed::DigitalOut& led) {
			led = true;
			wait(50_ms);
			led = false;
		};

		for (int i = 0; i < 2; ++i) { //Startup animation
			std::for_each(leds.begin(), std::prev(leds.end()), blink_with_delay);
			std::for_each(leds.rbegin(), std::prev(leds.rend()), blink_with_delay);
		}

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

	struct LEDManager {

		constexpr static Duration blinkPeriod = 150_ms;
		Duration previousBLink = Duration::now();

		void update() {

			if (previousBLink.time_elapsed(blinkPeriod)) {
				previousBLink = Duration::now();
				bool const new_state = !leds.front();
				std::for_each(leds.begin(), leds.end(),
					[new_state](mbed::DigitalOut& led) { led = new_state; });

			}
		}

	};



	enum class state {
		idle,
		computing,
		aborted
	};

	constexpr Duration computation_time = 50_ms;

	struct unit {
		state state_ = state::idle;
		unsigned short chunk_id = 0, task_count = 0;

		unsigned short current_task = 0;

		Duration computation_start;
	} unit;

}

extern "C" void message_enqueue(message const* msg) {
	int const required_size = message_size(static_cast<message_type>(msg->type));
	uint8_t buffer[sizeof(message)];
	message_decompose(msg, buffer, sizeof buffer);
	for (; !tx_buffer.can_fit(required_size););

	for (int i = 0; i < required_size; ++i)
		tx_buffer.write(buffer[i]);

	pc.attach(serial_tx_isr, mbed::Serial::TxIrq);
}

int main() {
	timer.start();
	startup_sequence();

	pc.attach(serial_rx_isr, mbed::Serial::RxIrq);

	ButtonManager buttonManager;
	LEDManager ledManager;

	bool quit = false;
	for (; !quit;) {

		if (buttonManager.been_pressed()) {
			quit = true;
			send_abort_request();
			unit.state_ = state::aborted;
			std::fill(leds.begin(), leds.end(), true);
			break;
		}

		if (!rx_buffer.empty()) {
			int const size = message_size(static_cast<message_type>(rx_buffer.peek()));
			if (rx_buffer.can_read(size)) {
				uint8_t buffer[sizeof(message)];
				for (int i = 0; i < size; ++i) {
					buffer[i] = rx_buffer.read();
				}
				message msg = message_parse(buffer, sizeof buffer);

				switch (msg.type) {
				case MSG_GET_VERSION:
					send_version();
					break;
				case MSG_ABORT: //Interrupt calculation and turn off leds
					unit.state_ = state::aborted;
					std::fill(leds.begin(), leds.end(), false);
					send_ok();
					break;

				case MSG_COMPUTE:
					unit.state_ = state::computing;
					unit.current_task = 0;
					unit.chunk_id = msg.data.compute.chunk_id;
					unit.task_count = msg.data.compute.nbr_tasks;
					unit.computation_start = Duration::now();
					send_ok();
					break;
				}
			}
		}

		if (unit.state_ == state::computing) {
			ledManager.update();

			if (unit.computation_start.time_elapsed(computation_time)) {
				message msg = { .type = MSG_COMPUTE_DATA };
				msg.data.compute_data.chunk_id = unit.chunk_id;
				msg.data.compute_data.task_id = unit.current_task;
				msg.data.compute_data.result = unit.chunk_id * unit.current_task;

				message_calculate_checksum(&msg);
				message_enqueue(&msg);

				unit.computation_start = Duration::now();
				if (++unit.current_task == unit.task_count) {
					//Computation ended - report it to the master and turn off leds.
					unit.state_ = state::idle;
					std::fill(leds.begin(), leds.end(), false);
					message msg = { .type = MSG_DONE };
					message_calculate_checksum(&msg);
					message_enqueue(&msg);
				}
			}
		}
	}
}