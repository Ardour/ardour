/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2010 Paul Davis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "ardour/vst_types.h"

void vststate_init (VSTState* state) {
	pthread_mutex_init (&state->lock, 0);
	pthread_mutex_init (&state->state_lock, 0);
	pthread_cond_init (&state->window_status_change, 0);
	pthread_cond_init (&state->plugin_dispatcher_called, 0);
	pthread_cond_init (&state->window_created, 0);
	state->want_program = -1;
	state->want_chunk = 0;
	state->n_pending_keys = 0;
	state->has_editor = 0;
	state->program_set_without_editor = 0;
	state->linux_window = 0;
	state->linux_plugin_ui_window = 0;
	state->eventProc = 0;
	state->extra_data = 0;
	state->want_resize = 0;
}
