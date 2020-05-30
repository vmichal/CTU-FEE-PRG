#include "juliaset.h"
#include <math.h>

my_complex add(my_complex const a, my_complex const b) {
	my_complex const result = { a.re + b.re, a.im + b.im };
	return result;
}

my_complex mul(my_complex const a, my_complex const b) {
	my_complex const result = { a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re };
	return result;
}

double magnitude(my_complex const a) {
	return sqrt(a.re * a.re + a.im * a.im);
}


int convergence_test(my_complex point, my_complex c, int max_steps) {
	if (magnitude(point) >= 2.0f) {
		return 0;
	}

	for (int i = 1; i <= max_steps; ++i) {
		if (magnitude(point) >= 2.0f) {
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

