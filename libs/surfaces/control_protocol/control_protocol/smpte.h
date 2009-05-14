/*
	Copyright (C) 2006 Paul Davis
	
	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU Lesser General Public License as published
	by the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	
	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
	FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
	for more details.
	
	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_smpte_h__
#define __ardour_smpte_h__

#include <inttypes.h>

namespace SMPTE {

enum Wrap {
	NONE = 0,
	FRAMES,
	SECONDS,
	MINUTES,
	HOURS
};

struct Time {
	bool       negative;
	uint32_t   hours;
	uint32_t   minutes;
	uint32_t   seconds;
	uint32_t   frames;        ///< SMPTE frames (not audio samples)
	uint32_t   subframes;     ///< Typically unused
	float      rate;          ///< Frame rate of this Time
	static float default_rate;///< Rate to use for default constructor
	bool       drop;          ///< Whether this Time uses dropframe SMPTE

	Time(float a_rate = default_rate) {
		negative = false;
		hours = 0;
		minutes = 0;
		seconds = 0;
		frames = 0;
		subframes = 0;
		rate = a_rate;
	}
};

Wrap increment( Time& smpte, uint32_t );
Wrap decrement( Time& smpte, uint32_t );
Wrap increment_subframes( Time& smpte, uint32_t );
Wrap decrement_subframes( Time& smpte, uint32_t );
Wrap increment_seconds( Time& smpte, uint32_t );
Wrap increment_minutes( Time& smpte, uint32_t );
Wrap increment_hours( Time& smpte, uint32_t );
void frames_floor( Time& smpte );
void seconds_floor( Time& smpte );
void minutes_floor( Time& smpte );
void hours_floor( Time& smpte );

} // namespace SMPTE

#endif  // __ardour_smpte_h__
