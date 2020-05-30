#include "juliaset.h"
#include <math.h>

my_complex add(my_complex const a, my_complex const b) {
	my_complex const result = { a.re + b.re, a.im + b.im };
	return result;
}

my_complex sub(my_complex const a, my_complex const b) {
	return add(a, negate(b));
}

my_complex mul(my_complex const a, my_complex const b) {
	my_complex const result = { a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re };
	return result;
}

my_complex negate(my_complex const val) {
	my_complex const result = { -val.re, -val.im };
	return result;
}

my_complex scalar_mul(my_complex const a, double const scalar) {
	my_complex const result = { a.re * scalar, a.im * scalar };
	return result;
}

static double magnitude_squared(my_complex const a) {
	return a.re * a.re + a.im * a.im;
}

double magnitude(my_complex const a) {
	return sqrt(magnitude_squared(a));
}



int convergence_test(my_complex point, my_complex c, int max_steps) {
	if (magnitude_squared(point) >= 4.0f) {
		return 0;
	}

	for (int i = 1; i <= max_steps; ++i) {
		if (magnitude_squared(point) >= 4.0f) {
			return i;
		}
		point = add(c, mul(point, point));
	}
	return max_steps;
}



uint8_t red_component(int first_lost, int max_steps) {
	float const t = first_lost / (float)max_steps;
	return 9 * 255 * pow(t, 3) * (1 - t);
}

uint8_t green_component(int first_lost, int max_steps) {
	float const t = first_lost / (float)max_steps;
	return 15 * 255 * pow(t, 2) * pow(1 - t, 2);
}

uint8_t blue_component(int first_lost, int max_steps) {
	float const t = first_lost / (float)max_steps;
	return 9 * 255 * t * pow(1 - t, 3);
}

