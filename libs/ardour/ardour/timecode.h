/*  Copyright (C) 2006 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef __ardour_timecode_h__
#define __ardour_timecode_h__

#include <inttypes.h>

namespace Timecode {

enum Wrap {
	NONE = 0,
	FRAMES,
	SECONDS,
	MINUTES,
	HOURS
};

/** Timecode frame rate (in frames per second).
 *
 * This should be eliminated in favour of a float to support arbitrary rates.
 */
enum FPS {
    MTC_24_FPS = 0,
    MTC_25_FPS = 1,
    MTC_30_FPS_DROP = 2,
    MTC_30_FPS = 3
};

struct Time {
	bool       negative;
	uint32_t   hours;
	uint32_t   minutes;
	uint32_t   seconds;
	uint32_t   frames;       ///< Timecode frames (not audio samples)
	uint32_t   subframes;    ///< Typically unused
	FPS        rate;         ///< Frame rate of this Time
	static FPS default_rate; ///< Rate to use for default constructor

	Time(FPS a_rate = default_rate) {
		negative = false;
		hours = 0;
		minutes = 0;
		seconds = 0;
		frames = 0;
		subframes = 0;
		rate = a_rate;
	}
};

Wrap increment( Time& timecode );
Wrap decrement( Time& timecode );
Wrap increment_subframes( Time& timecode );
Wrap decrement_subframes( Time& timecode );
Wrap increment_seconds( Time& timecode );
Wrap increment_minutes( Time& timecode );
Wrap increment_hours( Time& timecode );
void frames_floor( Time& timecode );
void seconds_floor( Time& timecode );
void minutes_floor( Time& timecode );
void hours_floor( Time& timecode );

} // namespace Timecode

#endif  // __ardour_timecode_h__
