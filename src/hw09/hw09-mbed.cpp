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
#include <map>

#include <LowPowerTimer.h>
#include <Serial.h>
#include <DigitalOut.h>

namespace {

	constexpr int init_baudrate = 115200;

	/*I've chosen five pins for leds, the middle one (PA4) is green, others are red. */
	std::array<mbed::DigitalOut, 5> leds{ { PA_0, PA_1, PA_4, PB_0, PC_1 } };
	//Blue button bound to the board. Internally connected to Pin PC13
	mbed::Serial pc{ USBTX, USBRX, init_baudrate };
	mbed::LowPowerTimer timer;

	static std::map<char, char> const acknowledges{
		{'h', 'h'}, {'b', 'b'}, {'1', 'a'},
		{'2', 'a'}, {'3', 'a'}, {'4', 'a'},
		{'5', 'a'}, {'s', 'a'}, {'e', 'a'}
	};

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

	/* Array of selected periods*/
	constexpr static std::array<Duration, 5> periods{ {
			50_ms, 100_ms, 200_ms, 500_ms, 1000_ms
		} };

	enum class LED_state {
		on, off, blink
	};

	class LightManager {

		Duration period_ = periods.back(), lastChange_ = 0_ms;
		LED_state state_ = LED_state::off;

	public:

		void update() {
			switch (state_) {
			case LED_state::blink:
				if (lastChange_.time_elapsed(period_)) {
					lastChange_ = Duration::now();

					bool const new_state = !leds.front();
					std::for_each(leds.begin(), leds.end(),
						[new_state](mbed::DigitalOut& led) {led = new_state; });

					pc.putc(new_state ? 'x' : 'o');
				}
				break;
			case LED_state::on:
				std::for_each(leds.begin(), leds.end(), [](mbed::DigitalOut& led) {led = true; });
				break;
			case LED_state::off:
				std::for_each(leds.begin(), leds.end(), [](mbed::DigitalOut& led) {led = false; });
				break;
			}
		}

		LED_state& state() { return state_; }
		Duration& period() { return period_; }
	};
}


int main() {
	timer.start();
	LightManager blinker;

	pc.putc('i');
	pc.set_blocking(false);

	bool quit = false;
	for (; !quit;) {

		blinker.update();

		if (!pc.readable()) {
			continue; //If no cómmand is pending
		}

		int const command = pc.getc();

		switch (command) {
		case 's':
			blinker.state() = LED_state::on;
			break;
		case 'e':
			blinker.state() = LED_state::off;
			break;
		case '1': case '2': case '3': case '4': case '5': {
			blinker.period() = periods[command - '1'];
			blinker.state() = LED_state::blink;
			break;
		}
		case 'b':
			quit = true;
		case 'h':
			break;
		default:
			continue;
		}

		pc.putc(acknowledges.at(command));
	}
}