#pragma once

#include "protocol.h"
#include "juliaset.h"

enum class module_state {
	computing,
	idle,
	finished
};

struct JuliaSetComputer {

	my_complex constant;
	float dx, dy;
	int max_steps;

	my_complex upper_left_corner;
	int chunk_id, width, height;
	int row, col;

	volatile module_state state_ = module_state::idle;

public:

	volatile module_state& state() { return state_; }

	void update_settings(msg_set_compute const& msg) {
		constant = { msg.c_re, msg.c_im };
		dx = msg.d_re;
		dy = msg.d_im;
		max_steps = msg.n;
	}

	void start_computation(msg_compute const& msg) {
		chunk_id = msg.cid;
		width = msg.n_re;
		height = msg.n_im;
		upper_left_corner = { msg.re, msg.im };
		state_ = module_state::computing;
		row = col = 0;
	}

	msg_compute_data compute_next() {
		msg_compute_data result;
		result.cid = chunk_id;
		result.i_im = row;
		result.i_re = col;
		result.iter = convergence_test(add(upper_left_corner, my_complex{ dx * col,-dy * row }), constant, max_steps);
		if (++col == width) {
			col = 0;
			if (++row == height) {
				row = 0;
				state_ = module_state::finished;
			}
		}
		return result;
	}

};