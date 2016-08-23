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

#ifndef __ardour_gtk2_tracker_pattern_h_
#define __ardour_gtk2_tracker_pattern_h_

#include "evoral/types.hpp"
#include "evoral/Beats.hpp"

#include "ardour/session_handle.h"
#include "ardour/beats_frames_converter.h"

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
class TrackerPattern {
public:
	TrackerPattern(ARDOUR::Session* session,
	               boost::shared_ptr<ARDOUR::Region> region);

	// Set the number of rows per beat. After changing that you probably need
	// to update the pattern, see below.
	void set_rows_per_beat(uint16_t rpb);

	// Build or rebuild the pattern
	virtual void update_pattern() = 0;

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

	// Like row_at_beats but use frame instead of beats
	uint32_t row_at_frame(framepos_t frame);

	// Return the row index assuming the beats is allowed to have the minimum
	// negative delay (1 - _ticks_per_row).
	uint32_t row_at_beats_min_delay(Evoral::Beats beats);

	// Like row_at_beats_min_delay but use frame instead of beats
	uint32_t row_at_frame_min_delay(framepos_t frame);

	// Return the row index assuming the beats is allowed to have the maximum
	// positive delay (_ticks_per_row - 1).
	uint32_t row_at_beats_max_delay(Evoral::Beats beats);

	// Like row_at_beats_max_delay but use frame instead of beats
	uint32_t row_at_frame_max_delay(framepos_t frame);

	// Return an event's delay in a certain row in ticks
	int64_t delay_ticks(const Evoral::Beats& event_time, uint32_t irow);

	// Like above but uses fram instead of beats
	int64_t delay_ticks(framepos_t frame, uint32_t irow);

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

#endif /* __ardour_gtk2_tracker_pattern_h_ */
