/*
* hw09
* Vojtìch Michal
* Compilable with C++14 standard; extended from HW08
*/

#include <algorithm>
#include <utility>
#include <array>
#include <iterator>
#include <optional>
#include <cstdint>

#include <LowPowerTimer.h>
#include <Serial.h>
#include <DigitalOut.h>

namespace {

	constexpr int chosen_baudrate = 115200;

	/*I've chosen five pins for leds, the middle one (PA4) is green, others are red. */
	std::array<mbed::DigitalOut, 5> leds{ { PA_0, PA_1, PA_4, PB_0, PC_1 } };
	//Blue button bound to the board. Internally connected to Pin PC13
	mbed::Serial pc{ SERIAL_TX, SERIAL_RX, chosen_baudrate };
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

	/*Interrupts execution for given duration.*/
	void wait(Duration const d) {
		auto const start = Duration::now();

		for (; Duration::now() - start < d;);
	}

	/* Array of selected periods*/
	constexpr static std::array<Duration, 5> periods{ {
			50_ms, 100_ms, 200_ms, 500_ms, 1000_ms
		} };

	enum class LED_state {
		on, off, blink
	};
}

int main() {
	timer.start();
	pc.putc('i');

	LED_state led_state = LED_state::off;
	Duration last_blink = Duration::now();
	Duration blink_period = periods.front();

	bool shall_stop = false; //Becomes true when pc sends the command 'b'.
	for (; !shall_stop;) {

		if (led_state == LED_state::blink && last_blink.time_elapsed(blink_period)) {
			last_blink = Duration::now();
			std::for_each(leds.begin(), leds.end(), [](mbed::DigitalOut& led) {led = !led; });
			while (!pc.writeable());
			pc.putc(leds.front() ? 'x' : 'o');
		}

		int const command = pc.readable() ? pc.getc() : -1;
		if (command == -1) {
			continue;
		}

		char response = 'a';
		switch (command) {
		case 'b':
			shall_stop = true;
			response = 'b';
			break;
		case 'h':
			response = 'h';
			break;

		case 's': case 'e': {
			bool const on = command == 's';

			led_state = on ? LED_state::on : LED_state::off;
			std::for_each(leds.begin(), leds.end(), [on](mbed::DigitalOut& led) {led = on; });
			break;
		}

		case '1': case '2': case '3': case '4': case '5':
			last_blink = Duration::now();
			led_state = LED_state::blink;
			blink_period = periods[command - '1'];
			break;

		default:
			response = 'f';
		}
		while (!pc.writeable());
		pc.putc(response);
	}
}
