
#include "fractal_drawer.h"
#include "xwin_sdl.h"
#include "juliaset.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <assert.h>

int chunks_in_row = 10;
int chunks_in_col = 10;

enum selection_policy selection_policy = policy_random;

int buffer_size = 0;
uint8_t* frame_buffer = NULL;
bool* chunks_done = NULL;
int width = 0, height = 0;
int precision = 0;
int current_chunk = -1;

my_complex top_left = { -1.6, 1.1 }, bot_right = { 1.6, -1.1 };

my_complex constant = { -0.4, 0.6 };

int chunk_row() { return current_chunk / chunks_in_row; }
int chunk_col() { return current_chunk % chunks_in_row; }

double pixel_width() { return (bot_right.re - top_left.re) / width; }
double pixel_height() { return (top_left.im - bot_right.im) / height; }

double chunk_width() { return width / chunks_in_row; }
double chunk_height() { return height / chunks_in_col; }

int chunk_count() { return chunks_in_col * chunks_in_row; }

void fractal_initialize(int w, int h, int pr, int columns, int rows) {
	xwin_init(w, h);
	fractal_set_image_size(w, h);
	fractal_set_screen_division(rows, columns);
	precision = pr;


}

void fractal_cleanup() {
	free(frame_buffer);
	free(chunks_done);
	xwin_close();
}


bool fractal_set_image_size(int w, int h) {
	if (h % chunks_in_col || w % chunks_in_row) {
		fprintf(stderr, "ERROR: Cannot have window side size not divisible by number of chunks.\r\n");
		return false;
	}
	int const new_size = sizeof(uint8_t) * h * w * 3;
	uint8_t* const new_buffer = malloc(new_size);
	if (!new_buffer) {
		fprintf(stderr, "ERROR: Cannot allocate %d bytes of new frame buffer.\r\n", new_size);
		return false;
	}
	buffer_size = new_size;
	width = w;
	height = h;

	SDL_SetWindowSize(win, width, height);

	free(frame_buffer);
	frame_buffer = new_buffer;
	fractal_clear_buffer();
	return true;
}

void fractal_add_point(int row, int col, int iterations) {

	frame_buffer[3 * (row * width + col) + 0] = red_component(iterations, precision);
	frame_buffer[3 * (row * width + col) + 1] = green_component(iterations, precision);
	frame_buffer[3 * (row * width + col) + 2] = blue_component(iterations, precision);

}

void fractal_add_point_in_chunk(int chunk, int relative_col, int relative_row, int iterations) {
	assert(current_chunk == chunk);

	int const row = chunk_row() * chunk_height() + relative_row;
	int const col = chunk_col() * chunk_width() + relative_col;

	fractal_add_point(row, col, iterations);
}

void fractal_finish_chunk() {
	assert(current_chunk >= 0 && current_chunk < chunk_count());
	chunks_done[current_chunk] = true;
}

static int find_new_chunk() {
	int const count = chunk_count();

	if (selection_policy == policy_sequential) {
		for (int i = 0; i < count; ++i) {
			if (!chunks_done[i]) {
				return i;
			}
		}
		assert(false);
	}
	int const middle = rand() % count;
	for (int chunk = middle; chunk < count; ++chunk) {
		if (!chunks_done[chunk]) {
			return chunk;
		}
	}
	for (int chunk = middle - 1; chunk >= 0; --chunk) {
		if (!chunks_done[chunk]) {
			return chunk;
		}
	}
	assert(false);
}

msg_compute fractal_get_next_chunk() {

	assert(!fractal_finished());

	msg_compute result;
	current_chunk = find_new_chunk();
	assert(current_chunk != -1);
	result.cid = current_chunk;
	result.n_re = width / chunks_in_row;
	result.n_im = height / chunks_in_col;
	result.re = top_left.re + chunk_width() * chunk_col() * pixel_width();
	result.im = top_left.im - chunk_height() * chunk_row() * pixel_height();

	return result;
}
msg_set_compute fractal_get_settings() {
	msg_set_compute result;

	result.n = precision;
	result.c_re = constant.re;
	result.c_im = constant.im;
	result.d_im = pixel_height();
	result.d_re = pixel_width();
	return result;
}



bool fractal_finished() {
	return fractal_remaining_chunks() == 0;
}

void fractal_clear_buffer() {
	memset(frame_buffer, 0, buffer_size);
}

int fractal_remaining_chunks() {
	int const max = chunk_count();
	int count = 0;
	for (int i = 0; i < max; ++i) {
		if (!chunks_done[i]) {
			++count;
		}
	}
	return count;
}

void fractal_redraw() {
	xwin_redraw(width, height, frame_buffer);
	xwin_poll_events();
}

void fractal_set_all_chunks_unseen() {
	memset(chunks_done, false, chunk_count());
}

void fractal_compute_locally() {
	fractal_set_all_chunks_unseen();
	while (!fractal_finished()) {
		msg_compute data = fractal_get_next_chunk();
		for (int row = 0; row < data.n_im; ++row) {
			for (int col = 0; col < data.n_re; ++col) {
				my_complex point;
				point.re = data.re + col * pixel_width();
				point.im = data.im - row * pixel_height();
				fractal_add_point_in_chunk(data.cid, col, row, convergence_test(point, constant, precision));
			}
		}
		fractal_finish_chunk();
	}

}

bool fractal_set_screen_division(int rows, int columns) {
	assert(rows > 0 && columns > 0);

	int const new_size = sizeof(bool) * rows * columns;
	bool* const new_buffer = malloc(new_size);
	if (!new_buffer) {
		fprintf(stderr, "ERROR: Cannot allocate %d bytes for chunk manager.\r\n", new_size);
		return false;
	}
	free(chunks_done);
	fractal_clear_buffer();
	chunks_done = new_buffer;

	chunks_in_col = rows;
	chunks_in_row = columns;

	return true;
}

void fractal_set_selection_policy(enum selection_policy p) {
	selection_policy = p;
}

