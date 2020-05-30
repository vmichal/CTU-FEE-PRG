#ifndef FRACTAL_H
#define FRACTAL_H

#include "protocol.h"

enum selection_policy {
	policy_sequential,
	policy_random
};

void fractal_initialize(int width, int height, int precision, int columns, int rows);
void fractal_cleanup();

bool fractal_set_image_size(int width, int height);

void fractal_add_point(int row, int col, int iterations);
void fractal_add_point_in_chunk(int chunk_id, int relative_col, int relative_row, int iterations);

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

#endif

