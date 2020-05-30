#ifndef FRACTAL_H
#define FRACTAL_H

#include "protocol.h"
#include "juliaset.h"


enum selection_policy {
	policy_sequential,
	policy_random,
	policy_centered
};

enum boundary {
	bound_topleft,
	bound_botright
};

void fractal_initialize(int w, int h, int pr, int columns, int rows,
	my_complex upper_left, my_complex lower_right, my_complex c);
void fractal_cleanup();

bool fractal_set_image_size(int width, int height);

void fractal_write_pixel(int row, int col, int r, int g, int b);
void fractal_add_point(int chunk_id, int relative_col, int relative_row, int iterations);

msg_set_compute fractal_get_settings();

msg_compute fractal_get_next_chunk();

void fractal_finish_chunk();

bool fractal_finished();

void fractal_clear_buffer();

int fractal_remaining_chunks();

void fractal_redraw();

void fractal_set_all_chunks_unseen();

void fractal_compute_locally();

bool fractal_set_screen_division(int rows, int columns);

void fractal_set_selection_policy(enum selection_policy p);

void fractal_draw_red_line(int x1, int y1, int x2, int y2);

bool save_to_ppm();

void fractal_set_edge(enum boundary b, my_complex new_value);
my_complex fractal_get_edge(enum boundary);
my_complex fractal_get_center();
void fractal_set_constant(my_complex c);
my_complex fractal_get_constant();

#endif

