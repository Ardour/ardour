/*
	Copyright (C) 2006-2010 Paul Davis
	
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

#ifndef __timecode_time_h__
#define __timecode_time_h__

#include <ostream>
#include <inttypes.h>

namespace Timecode {

enum Wrap {
	NONE = 0,
	FRAMES,
	SECONDS,
	MINUTES,
	HOURS
};

struct Time {
	bool         negative;
	uint32_t     hours;
	uint32_t     minutes;
	uint32_t     seconds;
	uint32_t     frames;        ///< Timecode frames (not audio samples)
	uint32_t     subframes;     ///< Typically unused
	float        rate;          ///< Frame rate of this Time
	static float default_rate;  ///< Rate to use for default constructor
	bool         drop;          ///< Whether this Time uses dropframe Timecode

	Time (float a_rate = default_rate) {
		negative = false;
		hours = 0;
		minutes = 0;
		seconds = 0;
		frames = 0;
		subframes = 0;
		rate = a_rate;
	}

	std::ostream& print (std::ostream& ostr) const {
		if (negative) {
			ostr << '-';
		}
		ostr << hours << ':' << minutes << ':' << seconds << ':'
		     << frames << '.' << subframes
		     << " @" << rate << (drop ? " drop" : " nondrop");
		return ostr;
	}

};

Wrap increment (Time& timecode, uint32_t);
Wrap decrement (Time& timecode, uint32_t);
Wrap increment_subframes (Time& timecode, uint32_t);
Wrap decrement_subframes (Time& timecode, uint32_t);
Wrap increment_seconds (Time& timecode, uint32_t);
Wrap increment_minutes (Time& timecode, uint32_t);
Wrap increment_hours (Time& timecode, uint32_t);
void frames_floor (Time& timecode);
void seconds_floor (Time& timecode);
void minutes_floor (Time& timecode);
void hours_floor (Time& timecode);

} // namespace Timecode

std::ostream& operator<< (std::ostream& ostr, const Timecode::Time& t);

#endif  // __timecode_time_h__
