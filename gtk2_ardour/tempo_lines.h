/*
 * Copyright (C) 2007-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2012-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
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

#ifndef __ardour_tempo_lines_h__
#define __ardour_tempo_lines_h__

#include "ardour/tempo.h"

#include "canvas/line_set.h"

class TempoLines {
public:
	TempoLines (ArdourCanvas::Container* group, double screen_height, ARDOUR::BeatsSamplesConverter* bfc);
	~TempoLines ();

	void tempo_map_changed(samplepos_t new_origin);

	void draw (std::vector<ARDOUR::TempoMap::BBTPoint>& grid,
	           unsigned                                 divisions,
	           ARDOUR::samplecnt_t                      leftmost_sample,
	           ARDOUR::samplecnt_t                      sample_rate);

	void show();
	void hide();

private:
	void draw_ticks (std::vector<ARDOUR::TempoMap::BBTPoint>& grid,
	                 unsigned                                 divisions,
	                 ARDOUR::samplecnt_t                      leftmost_sample,
	                 ARDOUR::samplecnt_t                      sample_rate);

	ArdourCanvas::LineSet lines;
	ARDOUR::BeatsSamplesConverter* _bfc;
};

#endif /* __ardour_tempo_lines_h__ */
