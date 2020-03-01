#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define digit_to_char(x) (x + '0')

#define char_to_digit(x) (x - '0')

//maximal number of decimal digits representable by bigint type
#define bigint_size 128

typedef struct bigint {
	char digits[bigint_size];
} bigint;

/*Initialize bigint by copying numbers from given buffer into the pointed to bigint.*/
void bigint_from_buff(bigint* ptr, char const* buffer) {
	int index = 0;

	for (; index < bigint_size && isdigit(buffer[index]); ++index)
		ptr->digits[index] = buffer[index];
	ptr->digits[index] = '\0';
}

//Returns true iff given bigint represents number one (end condition for division)
bool bigint_is_one(bigint* ptr) {
	return ptr->digits[0] == '1' && ptr->digits[1] == '\0';
}


#define PRIMES_COUNT 80*1000
#define UPPER_BOUND 1000*1000
/*There are cca 76 thousand primes under one million. The asignment guarantees that there 
won't be any number that would consist of prime factors >= million. */
int primes[PRIMES_COUNT];


void precompute_primes() {
	static char is_prime[UPPER_BOUND];
	//All numbers are primes in the beginning
	memset(is_prime, true, sizeof(is_prime));

	//Perform Erathostenes' sieve
	for (int current_prime = 2; current_prime < UPPER_BOUND; ++current_prime) {
		if (!is_prime[current_prime]) //if the current number was already eliminated, find a new one
			continue;

		//current_prime contains now a prime number => eliminate all its multiples
		for (int eliminated = 2 * current_prime; eliminated < UPPER_BOUND; eliminated += current_prime)
			is_prime[eliminated] = false;
	}

	//copy all found primes into the global array primes
	for (int read_index = 2, write_index = 0; read_index < UPPER_BOUND; ++read_index)
		if (is_prime[read_index])
			primes[write_index++] = read_index;

}


/*Computes the prime factorization of 64bit number.*/
void compute_natively(long long number) {

	bool already_printing = false;

	for (int const* prime = primes; number > 1 ; ++prime) {

		int power = 0;
		for (; number % *prime == 0; ++power)
			number /= *prime;

		if (power == 0)
			continue;

		if (already_printing)
			printf(" x ");

		already_printing = true;
		if (power > 1)
			printf("%d^%d", *prime, power);
		else
			printf("%d", *prime);
	}
	printf("\n");

}

void bigint_copy(bigint* to, bigint* from) {
	memcpy(to->digits, from->digits, bigint_size);
}
 
bool bigint_divide(bigint* const number, int const divisor, bigint* result) {
	//printf("Divide |%s| by %d, please\n", number->digits, divisor);
	int carry = 0;

	int result_index = 0;

	for (int source_index = 0; number->digits[source_index]; source_index++) {

		carry = carry * 10 + char_to_digit(number->digits[source_index]);
		//printf("carry %d, divisor %d, ratio %d, modulo %d\n", carry, divisor, carry / divisor, carry % divisor);

		result->digits[result_index++] = digit_to_char(carry / divisor);

		carry %= divisor;
	}
	result->digits[result_index] = '\0';

	//trim leading whitespace

	int start = 0;
	for (; result->digits[start] == '0'; ++start);

	int write = 0, read = start;
	for (; isdigit(result->digits[read]); ++write, ++read)
		result->digits[write] = result->digits[read];
	result->digits[write] = '\0';

	//printf("Division ends! |%s| (len %lu) divided by %d is |%s| (len %lu) with remainder %d.\n", number->digits, strlen(number->digits), divisor, result->digits, strlen(result->digits), carry);
	return carry == 0;
}

/*Computes the prime factorization of bigint.*/
void compute_bigint(bigint* ptr) {

	bigint tmp;
	bool already_printing = false;

	for (int* prime = primes; !bigint_is_one(ptr); ++prime) {
		int power = 0;

		for (; bigint_divide(ptr, *prime, &tmp); ++power) 
			bigint_copy(ptr, &tmp);

		if (power == 0)
			continue;

		if (already_printing)
			printf(" x ");

		already_printing = true;
		if (power == 1)
			printf("%d", *prime);
		else
			printf("%d^%d", *prime, power);
	}
	printf("\n");

}

bool is_input_valid(const char* buffer) {
	for (; *buffer; ++buffer)
		if (!isdigit(*buffer))
			return false;
	return true;
}




int main() {

	precompute_primes();


	for (;;) {

		char buffer[bigint_size];
		memset(buffer, 0, sizeof(buffer));
		fgets(buffer, sizeof(buffer), stdin);

		buffer[strlen(buffer) - 1] = '\0'; //overwrite the trailing newline

		if (buffer[0] == '0')
			break;

		if (!is_input_valid(buffer)) {
			fprintf(stderr, "Error: Chybny vstup!\n");
			return 100;
		}

		printf("Prvociselny rozklad cisla %s je:\n", buffer);

		if (buffer[0] == '1' && buffer[1] == '\0') {
			printf("1\n");
			continue;
		}

		if (strlen(buffer) < 18)
			compute_natively(atoll(buffer));
		else {

			bigint number;
			bigint_from_buff(&number, buffer);
			compute_bigint(&number);
		}

	}
	return EXIT_SUCCESS;
}

