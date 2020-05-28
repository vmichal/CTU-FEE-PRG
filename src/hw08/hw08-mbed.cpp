/*
* hw08
* Vojtìch Michal
* Compilable with C++14 standard
*/

#include <algorithm>
#include <utility>
#include <array>
#include <iterator>
#include <optional>
#include <cstdint>

#include <LowPowerTimer.h>
#include <InterruptIn.h>
#include <DigitalOut.h>

namespace {

	/*I've chosen five pins for leds, the middle one (PA4) is green, others are red. */
	std::array<mbed::DigitalOut, 5> leds{ { PA_0, PA_1, PA_4, PB_0, PC_1 } };
	//Blue button bound to the board. Internally connected to Pin PC13
	mbed::InterruptIn button{ PC_13 };
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

	};

	constexpr Duration operator""_ms(std::uint64_t ms) { return Duration::from_ms(ms); }
	constexpr Duration operator""_us(std::uint64_t us) { return Duration::from_us(us); }

	/*Interrupts execution for given duration.*/
	void wait(Duration const d) {
		auto const start = Duration::now();

		for (; Duration::now() - start < d;);
	}

	//Minimal duration of button press before blinking period is reset
	constexpr auto longPressTime = 200_ms;
	/* Array of selected periods*/
	constexpr static std::array<Duration, 7> periods{ {
			1000_ms, 500_ms, 400_ms, 300_ms, 200_ms, 100_ms, 50_ms
		} };

	class period_manager {

		volatile decltype(periods)::const_iterator iterator_ = periods.begin();
		Duration lastChange_ = Duration::now();
		volatile bool pressPending_;

	public:
		void release() {
			if (!pressPending_) {
				return; //Bounce effect protection
			}
			pressPending_ = false;
			Duration const timePressed = Duration::now() - lastChange_;
			if (timePressed < longPressTime) {
				//advance iterator on short press
				if (iterator_ != periods.end()) {
					++iterator_;
				}
			}
			else { //reset the period to max on extended press
				iterator_ = periods.begin();
			}

			lastChange_ = Duration::now();
		}

		void press() { 
			if (Duration::now() - lastChange_ < 50_ms) {
				return; //Bounce effect protection
			}

			pressPending_ = true;
			lastChange_ = Duration::now(); 
		}

		bool paused() { return iterator_ == periods.end(); }

		auto current_period() { return iterator_; }
	};
}

int main() {
	timer.start();
	period_manager period_manager;

	button.fall(mbed::callback(&period_manager, &period_manager::press));
	button.rise(mbed::callback(&period_manager, &period_manager::release));

	/*Lambda expression performing one blink on the given LED. */
	auto const blink = [&period_manager](mbed::DigitalOut& led) {
		led = true;

		for (; period_manager.paused();) {}
		wait(*period_manager.current_period());

		led = false;
	};

	for (;;) { //Traverse array of LEDs in both directions and switch them sequentially on 
		std::for_each(leds.begin(), std::prev(leds.end()), blink);
		std::for_each(leds.rbegin(), std::prev(leds.rend()), blink);
	}
}
