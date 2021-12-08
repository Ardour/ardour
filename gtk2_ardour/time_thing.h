/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk2_ardour_time_thing_h__
#define __gtk2_ardour_time_thing_h__

struct TimeThing {
	virtual ~TimeThing() {}
	virtual samplepos_t pixel_to_sample (double pixel) const = 0;
	virtual samplepos_t playhead_cursor_sample () const = 0;
	virtual double sample_to_pixel (samplepos_t sample) const = 0;
	virtual double sample_to_pixel_unrounded (samplepos_t sample) const = 0;
	virtual double time_to_pixel (Temporal::timepos_t const &) const = 0;
	virtual double time_to_pixel_unrounded (Temporal::timepos_t const &) const = 0;
	virtual double duration_to_pixels (Temporal::timecnt_t const &) const = 0;
	virtual double duration_to_pixels_unrounded (Temporal::timecnt_t const &) const = 0;
};

#endif /* __gtk2_ardour_time_thing_h__ */
