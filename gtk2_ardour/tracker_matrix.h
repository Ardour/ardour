/*
    Copyleft (C) 2016 Nil Geisweiller

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_gtk2_tracker_matrix_h_
#define __ardour_gtk2_tracker_matrix_h_

#include <gtkmm/treeview.h>
#include <gtkmm/table.h>
#include <gtkmm/box.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>

#include "gtkmm2ext/bindings.h"

#include "evoral/types.hpp"

#include "ardour/session_handle.h"

#include "ardour_dropdown.h"
#include "ardour_window.h"
#include "editing.h"

namespace ARDOUR {
	class Region;
	class Session;
};

/**
 * Shared methods for storing and handling data for the midi, audio and
 * automation tracker editor.
 */
class TrackerMatrix {
public:
	TrackerMatrix(ARDOUR::Session* session,
	              boost::shared_ptr<ARDOUR::Region> region);

	// Set the number of rows per beat. After changing that you probably need
	// to update the matrix, see below.
	void set_rows_per_beat(uint16_t rpb);

	// Build or rebuild the matrix
	virtual void update_matrix() = 0;

	// Find the beats corresponding to the first row
	Evoral::Beats find_first_row_beats();

	// Find the beats corresponding to the last row
	Evoral::Beats find_last_row_beats();

	// Find the number of rows of the region
	uint32_t find_nrows();

	// Return the frame at the corresponding row index
	framepos_t frame_at_row(uint32_t irow);

	// Return the beats at the corresponding row index
	Evoral::Beats beats_at_row(uint32_t irow);

	// Return the row index corresponding to the given beats, assuming the
	// minimum allowed delay is -_ticks_per_row/2 and the maximum allowed delay
	// is _ticks_per_row/2.
	uint32_t row_at_beats(Evoral::Beats beats);

	// Return the row index assuming the beats is allowed to have the minimum
	// negative delay (1 - _ticks_per_row).
	uint32_t row_at_beats_min_delay(Evoral::Beats beats);

	// Return the row index assuming the beats is allowed to have the maximum
	// positive delay (_ticks_per_row - 1).
	uint32_t row_at_beats_max_delay(Evoral::Beats beats);

	// Number of rows per beat
	uint8_t rows_per_beat;

	// Determined by the number of rows per beat
	Evoral::Beats beats_per_row;

	// Beats corresponding to the first row
	Evoral::Beats first_beats;

	// Beats corresponding to the last row
	Evoral::Beats last_beats;

	// Number of rows of that region (given the choosen resolution)
	uint32_t nrows;

private:
	uint32_t _ticks_per_row;		// number of ticks per rows
	ARDOUR::Session* _session;
	boost::shared_ptr<ARDOUR::Region> _region;
	ARDOUR::BeatsFramesConverter _conv;	
};

#endif /* __ardour_gtk2_tracker_matrix_h_ */
