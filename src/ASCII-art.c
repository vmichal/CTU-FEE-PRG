#include <stdio.h>
#include <stdbool.h>

/*Stores house parameters for drawing. */
struct house_data {
	int width, height, fence_size;
	bool fence_requested;
};

int is_between(int value, int min, int max) {
	return value >= min && value <= max;
}

void print_char_n(int count, char character) {
	for (; count; --count)
		putc(character, stdout);
}

void draw_roof(int const width) {
	print_char_n(width / 2, ' ');
	printf("X\n");

	for (int line = 1; line < width / 2; ++line) {
		print_char_n(width / 2 - line, ' ');
		putc('X', stdout);
		print_char_n(line * 2 - 1, ' ');
		printf("X\n");
	}
}

/* Prints one line of fence alternating between | and 'alternative' as chars to be printed. */
void print_fence_line(int const size, char const alternative) {
	if (size % 2 == 1)
		printf("|");
	for (int i = 0; i < size / 2; ++i)
		printf("%c|", alternative);
}

/* Draws ASCII art house using information supplied via the first parameter. */
void draw_house(struct house_data const* const house) {

	draw_roof(house->width);
	print_char_n(house->width, 'X'); //one full line under the roof
	putc('\n', stdout);

	for (int line = house->height -1 ; line > 1; --line) {
		printf("X");

		for (int column = 2; column < house->width; ++column)
			if (house->fence_requested) {
				static char next = 'o';
				putc(next, stdout);
				next = next == 'o' ? '*' : 'o';
			}
			else
				putc(' ', stdout);

		printf("X");

		if (house->fence_requested) {
			if (line == house->fence_size)
				print_fence_line(house->fence_size, '-');

			if (line < house->fence_size)
				print_fence_line(house->fence_size, ' ');
		}
		putc('\n', stdout);
	}

	print_char_n(house->width, 'X'); //one full line as floor
	if (house->fence_requested)
		print_fence_line(house->fence_size, '-');
	putc('\n', stdout);
}

int main(int argc, char** argv) {

	struct house_data house;

	if (scanf("%d %d", &house.width, &house.height) != 2) {
		fprintf(stderr, "Error: Chybny vstup!\n");
		return 100;
	}

	if (house.width == house.height) {
		house.fence_requested = true;
		if (scanf("%d", &house.fence_size) != 1) {
			fprintf(stderr, "Error: Chybny vstup!\n");
			return 100;
		}
	}
	else
		house.fence_requested = false;

	int const size_min = 3, size_max = 69;

	if (!is_between(house.width, size_min, size_max) || !is_between(house.height, size_min, size_max)) {
		fprintf(stderr, "Error: Vstup mimo interval!\n");
		return 101;
	}

	if (house.width % 2 == 0) {
		fprintf(stderr, "Error: Sirka neni liche cislo!\n");
		return 102;
	}

	if (house.fence_requested)
		if (house.fence_size <= 0 || house.fence_size >= house.height) {
			fprintf(stderr, "Error: Neplatna velikost plotu!\n");
			return 103;
		}

	draw_house(&house);

	return 0;
}
