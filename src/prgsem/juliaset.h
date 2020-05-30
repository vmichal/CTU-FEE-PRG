#ifndef JULIA_H
#define JULIA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <stdint.h>

typedef struct my_complex {
	double re, im;
} my_complex;

my_complex mul(my_complex, my_complex);
my_complex add(my_complex, my_complex);
my_complex sub(my_complex, my_complex);
my_complex negate(my_complex);
my_complex scalar_mul(my_complex, double);
double magnitude(my_complex);

/* Examine convergence of the expression describing the Julia set for given constant
 c and given starting point 'point'. Up to max_steps can be taken. If none of them achieves
  a magnitude not less than two, then the original point falls into the set.

  Returns index k of the first z_k, that does not fall into the magnitude <= 2.0f range. */
int convergence_test(my_complex point, my_complex c, int max_steps);

/* Separate color components. Calculation based on selected precision (max_steps) and the
actual number of steps required to make the series diverge. */
uint8_t red_component(int first_lost, int max_steps);
uint8_t green_component(int first_lost, int max_steps);
uint8_t blue_component(int first_lost, int max_steps);


#if __cplusplus
}
#endif

#endif

