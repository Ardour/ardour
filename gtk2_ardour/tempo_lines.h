/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_tempo_lines_h__
#define __ardour_tempo_lines_h__

#include "ardour/beats_samples_converter.h"
#include "ardour/tempo.h"

#include "canvas/line_set.h"

class TempoLines {
public:
	TempoLines (ArdourCanvas::Container* group, double screen_height, ARDOUR::BeatsSamplesConverter* bfc);
	~TempoLines ();

	void tempo_map_changed(samplepos_t new_origin);

	void draw (std::vector<ARDOUR::TempoMap::BBTPoint>& grid,
		   unsigned                                              divisions,
	           ARDOUR::samplecnt_t                                    leftmost_sample,
	           ARDOUR::samplecnt_t                                    sample_rate);

	void show();
	void hide();

private:
	void draw_ticks (std::vector<ARDOUR::TempoMap::BBTPoint>& grid,
			 unsigned                                              divisions,
	                 ARDOUR::samplecnt_t                                    leftmost_sample,
	                 ARDOUR::samplecnt_t                                    sample_rate);

	ArdourCanvas::LineSet lines;
	ARDOUR::BeatsSamplesConverter* _bfc;
};

#endif /* __ardour_tempo_lines_h__ */
