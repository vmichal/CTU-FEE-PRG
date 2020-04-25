#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>

/* Albeit not being thread safe, this is the best option I have to prevent multiple
	calls to the function, which uses this macro. It defines a local static variable
	and asserts that this block is executed only once. */
#define ASSERT_CALLED_ONLY_ONCE														\
	static bool _already_called = false;											\
	assert(!_already_called && "This function cannot be called multiple times!");	\
	_already_called = true;

enum exit_code {
	//Normal program execution and termination - input is ok etc.
	EXIT_OK = EXIT_SUCCESS,

	/*The assignment states that 100 shall be returned when character other than
	digit is read from the standard input. */
	EXIT_INVALID_INPUT = 100
};


#define digit_to_char(x) (x + '0')

#define char_to_digit(x) (x - '0')

//maximal number of decimal digits representable by bigint type
#define bigint_size 128

typedef struct bigint {
	char digits[bigint_size];
} bigint;

/*Initialize bigint by copying numbers from given buffer into the pointed to bigint.*/
void bigint_from_buffer(bigint* ptr, char const* buffer) {
	int index = 0;

	for (; index < bigint_size && isdigit(buffer[index]); ++index) {
		ptr->digits[index] = buffer[index];
	}
	ptr->digits[index] = '\0';
}

//Returns true iff given bigint represents number one (end condition for division)
bool bigint_is_one(bigint const* ptr) {
	return ptr->digits[0] == '1' && ptr->digits[1] == '\0';
}


#define PRIMES_COUNT 80*1000
#define UPPER_BOUND 1000*1000
/*There are cca 76 thousand primes under one million. The asignment guarantees that there 
won't be any number that would consist of prime factors >= million. */
int primes[PRIMES_COUNT];


void precompute_primes() {
	//this function shall be called only once, because it is very computationally intensive
	ASSERT_CALLED_ONLY_ONCE;

	static char is_prime[UPPER_BOUND];
	//All numbers are believed to be primes in the beginning
	memset(is_prime, true, sizeof(is_prime));

	//Perform Erathostenes' sieve
	for (int current_prime = 2; current_prime < UPPER_BOUND; ++current_prime) {
		if (!is_prime[current_prime]) //if the current number was already eliminated, find a new one
			continue;

		//current_prime contains now a prime number => eliminate all its multiples
		for (int eliminated = 2 * current_prime; eliminated < UPPER_BOUND; eliminated += current_prime) {
			is_prime[eliminated] = false;
		}
	}

	//copy all found primes into the global array primes
	for (int read_index = 2, write_index = 0; read_index < UPPER_BOUND; ++read_index) {
		if (is_prime[read_index]) {
			primes[write_index++] = read_index;
		}
	}

}


/*Computes the prime factorization of 64bit number.*/
void compute_natively(long long number) {

	bool already_printing = false;

	for (int const* prime = primes; number > 1 ; ++prime) {

		int power = 0;
		for (; number % *prime == 0; ++power) {
			number /= *prime;
		}

		if (power == 0) {
			continue;
		}

		if (already_printing) {
			printf(" x ");
		}

		already_printing = true;
		if (power > 1) {
			printf("%d^%d", *prime, power);
		}
		else {
			printf("%d", *prime);
		}
	}
	printf("\n");

}

void bigint_copy(bigint* to, bigint* from) {
	memcpy(to->digits, from->digits, bigint_size);
}
 
/* Performs operation 'number' / 'divisor' and writes the result to 'result'. 
	Returns true iff the division finished without remainder. */
bool bigint_divide(bigint* const number, int const divisor, bigint* result) {
	int carry = 0;

	int result_index = 0;

	for (int source_index = 0; number->digits[source_index]; source_index++) {

		carry = carry * 10 + char_to_digit(number->digits[source_index]);

		result->digits[result_index++] = digit_to_char(carry / divisor);

		carry %= divisor;
	}
	result->digits[result_index] = '\0';

	//trim leading whitespace

	int first_non_zero_digit = 0;
	for (; result->digits[first_non_zero_digit] == '0'; ++first_non_zero_digit);

	int write = 0, read = first_non_zero_digit;
	for (; isdigit(result->digits[read]); ++write, ++read) {
		result->digits[write] = result->digits[read];
	}
	result->digits[write] = '\0';

	return carry == 0;
}

/*Computes the prime factorization of bigint.*/
void compute_bigint(bigint* dividend) {

	bigint tmp;
	bool already_printing = false;

	for (int* prime = primes; !bigint_is_one(dividend); ++prime) {
		int power = 0;

		//while dividend can be divided by the current prime, do so
		for (; bigint_divide(dividend, *prime, &tmp); ++power) {
			bigint_copy(dividend, &tmp);
		}

		if (power == 0) {
			continue;
		}

		if (already_printing) {
			printf(" x ");
		}

		already_printing = true;
		if (power == 1) {
			printf("%d", *prime);
		}
		else {
			printf("%d^%d", *prime, power);
		}
	}
	printf("\n");

}

/* Returns true iff given string contains valid number. */
bool is_input_valid(const char* buffer) {
	for (; *buffer; ++buffer) {
		if (!isdigit(*buffer)) {
			return false;
		}
	}
	return true;
}


void exit_with_error(enum exit_code code, const char* message) {
	fprintf(stderr, "%s", message);
	exit(code);
}

int main() {

	precompute_primes();


	for (;;) {

		char buffer[bigint_size];
		memset(buffer, 0, sizeof(buffer));
		fgets(buffer, sizeof(buffer), stdin);

		buffer[strlen(buffer) - 1] = '\0'; //overwrite the trailing newline

		if (buffer[0] == '0') {
			break;
		}

		if (!is_input_valid(buffer)) {
			exit_with_error(EXIT_INVALID_INPUT, "Error: Chybny vstup!\n");
		}

		printf("Prvociselny rozklad cisla %s je:\n", buffer);

		if (buffer[0] == '1' && buffer[1] == '\0') {
			printf("1\n");
			continue;
		}

		/*up to eighteen decimal digits fit safely into a 64bit signed int
		and can be computed natively. */
		if (strlen(buffer) <= 18) { 
			compute_natively(atoll(buffer));
		}
		else {
			bigint number;
			bigint_from_buffer(&number, buffer);
			compute_bigint(&number);
		}
		
	}
	return EXIT_OK;
}

