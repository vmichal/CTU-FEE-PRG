#ifndef FRACTAL_H
#define FRACTAL_H

#include "protocol.h"
#include "juliaset.h"

//Chooses, how should the program select chunks for calculations
enum selection_policy {
	policy_sequential, //Tho topmost and leftmost is selested
	policy_random //Any random unfinished chunk
};

//Distinguishes between two edges defining the visible section of complex plane
enum boundary {
	bound_topleft,
	bound_botright
};

/* Initializes SDL wrapper and most important data (like bounds or precision). */
void fractal_initialize(int w, int h, int pr, int columns, int rows,
	my_complex upper_left, my_complex lower_right, my_complex c);

/* Free allocated memory, close windows. Call before the program exits.*/
void fractal_cleanup();

/*Resize the window. Dimensions must be divisible by 10.*/
bool fractal_set_image_size(int width, int height);

/* Directly modifies the underlying buffer of raw pixels. */
void fractal_write_pixel(int row, int col, int r, int g, int b);

/* Writes a pixel in given chunk with given relative coordinates. */
void fractal_add_point(int chunk_id, int relative_col, int relative_row, int iterations);

/* Getter for config required by Nucleo (message set_compute). */
msg_set_compute fractal_get_settings();

/* Returns data about the next chunk, for which data colors shall be computed. */
msg_compute fractal_get_next_chunk();

/* Report that all pixels within this chunk have been filled. Advances to a next one. */
void fractal_finish_chunk();

//Return true iff all pixels of all chunks are filled.
bool fractal_finished();

//Fills pixel array by zeros thus painting the window black.
void fractal_clear_buffer();

//Returns number of chunks that are still waiting to be finished.
int fractal_remaining_chunks();

//Redraw the window with current contents of underlying frame_buffer
void fractal_redraw();

//Resets chunk data - window is not affected, but a new computation can be initiated
void fractal_set_all_chunks_unseen();

//Compute all chunks using local CPU (don't delegate to worker module)
void fractal_compute_locally();

//Determines, how many chunks make up a row and a column. Thus controls the size
//of chunks, which are considered computation primitive.
bool fractal_set_screen_division(int rows, int columns);

//Sets the desird policy for selecting unfinished chunks
void fractal_set_selection_policy(enum selection_policy p);

//Sets new coordinates of visible rectangle
void fractal_set_edge(enum boundary b, my_complex new_value);
//Sets new value for constant C
void fractal_set_constant(my_complex c);
//Returns coordinates of either top-left or bottom-right edge of the visible rectangle
my_complex fractal_get_edge(enum boundary);
//Geter returning coordinates of the point in the middle of the screen
my_complex fractal_get_center();
//Getter for constant C used in the Julia set computation
my_complex fractal_get_constant();

//Export the current frame_buffer to ppm file "fractal.ppm"
//Return true on success
bool save_to_ppm();
#endif

