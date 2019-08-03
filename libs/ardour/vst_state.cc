/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string.h>
#include "ardour/vst_types.h"

void
vststate_init (VSTState* state) {
	memset (state, 0, sizeof (VSTState));
	pthread_mutex_init (&state->lock, 0);
	pthread_mutex_init (&state->state_lock, 0);
	pthread_cond_init (&state->window_status_change, 0);
	pthread_cond_init (&state->plugin_dispatcher_called, 0);
	pthread_cond_init (&state->window_created, 0);
	state->want_program = -1;
}

/* This is to be called while handling VST UI events.
 *
 * Many plugins expect program dispatch from the GUI event-loop
 * only  (VSTPlugin::load_plugin_preset/set_chunk is invoked by
 * the user in ardour's main GUI thread, which on Windows and Linux
 * may *not* the VST event loop).
 */
void
vststate_maybe_set_program (VSTState* state)
{
	if (state->want_program != -1) {
		if (state->vst_version >= 2) {
			state->plugin->dispatcher (state->plugin, effBeginSetProgram, 0, 0, NULL, 0);
		}

		state->plugin->dispatcher (state->plugin, effSetProgram, 0, state->want_program, NULL, 0);

		if (state->vst_version >= 2) {
			state->plugin->dispatcher (state->plugin, effEndSetProgram, 0, 0, NULL, 0);
		}
		state->want_program = -1;
	}

	if (state->want_chunk == 1) {
		pthread_mutex_lock (&state->state_lock);
		state->plugin->dispatcher (state->plugin, 24 /* effSetChunk */, 1, state->wanted_chunk_size, state->wanted_chunk, 0);
		state->want_chunk = 0;
		pthread_mutex_unlock (&state->state_lock);
	}
}
