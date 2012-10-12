/*
  Copyright (C) 2006-2010 Paul Davis
	
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
  License for more details.
  
  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software Foundation,
  Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#define Timecode_IS_AROUND_ZERO(sm) (!(sm).frames && !(sm).seconds && !(sm).minutes && !(sm).hours)
#define Timecode_IS_ZERO(sm) (!(sm).frames && !(sm).seconds && !(sm).minutes && !(sm).hours && !(sm.subframes))

#include <math.h>

#include "timecode/time.h"

namespace Timecode {

float Time::default_rate = 30.0;


/** Increment @a timecode by exactly one frame (keep subframes value).
 * Realtime safe.
 * @return true if seconds wrap.
 */
Wrap
increment (Time& timecode, uint32_t subframes_per_frame)
{
	Wrap wrap = NONE;

	if (timecode.negative) {
		if (Timecode_IS_AROUND_ZERO (timecode) && timecode.subframes) {
			// We have a zero transition involving only subframes
			timecode.subframes = subframes_per_frame - timecode.subframes;
			timecode.negative = false;
			return SECONDS;
		}
    
		timecode.negative = false;
		wrap = decrement (timecode, subframes_per_frame);
		if (!Timecode_IS_ZERO (timecode)) {
			timecode.negative = true;
		}
		return wrap;
	}

	switch ((int)ceil (timecode.rate)) {
	case 24:
		if (timecode.frames == 23) {
			timecode.frames = 0;
			wrap = SECONDS;
		}
		break;
	case 25:
		if (timecode.frames == 24) {
			timecode.frames = 0;
			wrap = SECONDS;
		}
		break;
	case 30:
		if (timecode.drop) {
			if (timecode.frames == 29) {
				if (((timecode.minutes + 1) % 10) && (timecode.seconds == 59)) {
					timecode.frames = 2;
				}
				else {
					timecode.frames = 0;
				}
				wrap = SECONDS;
			}
		} else {

			if (timecode.frames == 29) {
				timecode.frames = 0;
				wrap = SECONDS;
			}
		}
		break;
	case 60:
		if (timecode.frames == 59) {
			timecode.frames = 0;
			wrap = SECONDS;
		}
		break;
	}
  
	if (wrap == SECONDS) {
		if (timecode.seconds == 59) {
			timecode.seconds = 0;
			wrap = MINUTES;
			if (timecode.minutes == 59) {
				timecode.minutes = 0;
				wrap = HOURS;
				timecode.hours++;
			} else {
				timecode.minutes++;
			}
		} else {
			timecode.seconds++;
		}
	} else {
		timecode.frames++;
	}
  
	return wrap;
}


/** Decrement @a timecode by exactly one frame (keep subframes value)
 * Realtime safe.
 * @return true if seconds wrap. */
Wrap
decrement (Time& timecode, uint32_t subframes_per_frame)
{
	Wrap wrap = NONE;
  
	if (timecode.negative || Timecode_IS_ZERO (timecode)) {
		timecode.negative = false;
		wrap = increment (timecode, subframes_per_frame);
		timecode.negative = true;
		return wrap;
	} else if (Timecode_IS_AROUND_ZERO (timecode) && timecode.subframes) {
		// We have a zero transition involving only subframes
		timecode.subframes = subframes_per_frame - timecode.subframes;
		timecode.negative = true;
		return SECONDS;
	}
  
	switch ((int)ceil (timecode.rate)) {
	case 24:
		if (timecode.frames == 0) {
			timecode.frames = 23;
			wrap = SECONDS;
		}
		break;
	case 25:
		if (timecode.frames == 0) {
			timecode.frames = 24;
			wrap = SECONDS;
		}
		break;
	case 30:
		if (timecode.drop) {
			if ((timecode.minutes % 10) && (timecode.seconds == 0)) {
				if (timecode.frames <= 2) {
					timecode.frames = 29;
					wrap = SECONDS;
				}
			} else if (timecode.frames == 0) {
				timecode.frames = 29;
				wrap = SECONDS;
			}
			
		} else {
			if (timecode.frames == 0) {
				timecode.frames = 29;
				wrap = SECONDS;
			}
		}
		break;
	case 60:
		if (timecode.frames == 0) {
			timecode.frames = 59;
			wrap = SECONDS;
		}
		break;
	}
  
	if (wrap == SECONDS) {
		if (timecode.seconds == 0) {
			timecode.seconds = 59;
			wrap = MINUTES;
			if (timecode.minutes == 0) {
				timecode.minutes = 59;
				wrap = HOURS;
				timecode.hours--;
			}
			else {
				timecode.minutes--;
			}
		} else {
			timecode.seconds--;
		}
	} else {
		timecode.frames--;
	}
  
	if (Timecode_IS_ZERO (timecode)) {
		timecode.negative = false;
	}
  
	return wrap;
}


/** Go to lowest absolute subframe value in this frame (set to 0 :-)) */
void
frames_floor (Time& timecode)
{
	timecode.subframes = 0;
	if (Timecode_IS_ZERO (timecode)) {
		timecode.negative = false;
	}
}


/** Increment @a timecode by one subframe */
Wrap
increment_subframes (Time& timecode, uint32_t subframes_per_frame)
{
	Wrap wrap = NONE;
  
	if (timecode.negative) {
		timecode.negative = false;
		wrap = decrement_subframes (timecode, subframes_per_frame);
		if (!Timecode_IS_ZERO (timecode)) {
			timecode.negative = true;
		}
		return wrap;
	}
  
	timecode.subframes++;
	if (timecode.subframes >= subframes_per_frame) {
		timecode.subframes = 0;
		increment (timecode, subframes_per_frame);
		return FRAMES;
	}
	return NONE;
}


/** Decrement @a timecode by one subframe */
Wrap
decrement_subframes (Time& timecode, uint32_t subframes_per_frame)
{
	Wrap wrap = NONE;
  
	if (timecode.negative) {
		timecode.negative = false;
		wrap = increment_subframes (timecode, subframes_per_frame);
		timecode.negative = true;
		return wrap;
	}
  
	if (timecode.subframes <= 0) {
		timecode.subframes = 0;
		if (Timecode_IS_ZERO (timecode)) {
			timecode.negative = true;
			timecode.subframes = 1;
			return FRAMES;
		} else {
			decrement (timecode, subframes_per_frame);
			timecode.subframes = 79;
			return FRAMES;
		}
	} else {
		timecode.subframes--;
		if (Timecode_IS_ZERO (timecode)) {
			timecode.negative = false;
		}
		return NONE;
	}
}


/** Go to next whole second (frames == 0 or frames == 2) */
Wrap
increment_seconds (Time& timecode, uint32_t subframes_per_frame)
{
	Wrap wrap = NONE;
  
	// Clear subframes
	frames_floor (timecode);
  
	if (timecode.negative) {
		// Wrap second if on second boundary
		wrap = increment (timecode, subframes_per_frame);
		// Go to lowest absolute frame value
		seconds_floor (timecode);
		if (Timecode_IS_ZERO (timecode)) {
			timecode.negative = false;
		}
	} else {
		// Go to highest possible frame in this second
		switch ((int)ceil (timecode.rate)) {
		case 24:
			timecode.frames = 23;
			break;
		case 25:
			timecode.frames = 24;
			break;
		case 30:
			timecode.frames = 29;
			break;
		case 60:
			timecode.frames = 59;
			break;
		}
    
		// Increment by one frame
		wrap = increment (timecode, subframes_per_frame);
	}
  
	return wrap;
}


/** Go to lowest (absolute) frame value in this second
 * Doesn't care about positive/negative */
void
seconds_floor (Time& timecode)
{
	// Clear subframes
	frames_floor (timecode);
  
	// Go to lowest possible frame in this second
	switch ((int)ceil (timecode.rate)) {
	case 24:
	case 25:
	case 30:
	case 60:
		if (!(timecode.drop)) {
			timecode.frames = 0;
		} else {
			if ((timecode.minutes % 10) && (timecode.seconds == 0)) {
				timecode.frames = 2;
			} else {
				timecode.frames = 0;
			}
		}
		break;
	}
  
	if (Timecode_IS_ZERO (timecode)) {
		timecode.negative = false;
	}
}


/** Go to next whole minute (seconds == 0, frames == 0 or frames == 2) */
Wrap
increment_minutes (Time& timecode, uint32_t subframes_per_frame)
{
	Wrap wrap = NONE;
  
	// Clear subframes
	frames_floor (timecode);
  
	if (timecode.negative) {
		// Wrap if on minute boundary
		wrap = increment_seconds (timecode, subframes_per_frame);
		// Go to lowest possible value in this minute
		minutes_floor (timecode);
	} else {
		// Go to highest possible second
		timecode.seconds = 59;
		// Wrap minute by incrementing second
		wrap = increment_seconds (timecode, subframes_per_frame);
	}
  
	return wrap;
}


/** Go to lowest absolute value in this minute */
void
minutes_floor (Time& timecode)
{
	// Go to lowest possible second
	timecode.seconds = 0;
	// Go to lowest possible frame
	seconds_floor (timecode);

	if (Timecode_IS_ZERO (timecode)) {
		timecode.negative = false;
	}
}


/** Go to next whole hour (minute = 0, second = 0, frame = 0) */
Wrap
increment_hours (Time& timecode, uint32_t subframes_per_frame)
{
	Wrap wrap = NONE;
  
	// Clear subframes
	frames_floor (timecode);
  
	if (timecode.negative) {
		// Wrap if on hour boundary
		wrap = increment_minutes (timecode, subframes_per_frame);
		// Go to lowest possible value in this hour
		hours_floor(timecode);
	} else {
		timecode.minutes = 59;
		wrap = increment_minutes (timecode, subframes_per_frame);
	}
  
	return wrap;
}


/** Go to lowest absolute value in this hour */
void
hours_floor(Time& timecode)
{
	timecode.minutes   = 0;
	timecode.seconds   = 0;
	timecode.frames    = 0;
	timecode.subframes = 0;
  
	if (Timecode_IS_ZERO (timecode)) {
		timecode.negative = false;
	}
}

float
timecode_to_frames_per_second(TimecodeFormat t)
{
	switch (t) {
		case timecode_23976:
			return (24000.0/1001.0); //23.976;

			break;
		case timecode_24:
			return 24;

			break;
		case timecode_24976:
			return (25000.0/1001.0); //24.976;

			break;
		case timecode_25:
			return 25;

			break;
		case timecode_2997:
			return 29.97;

			break;
		case timecode_2997drop:
			return (30000.0/1001.0); //29.97;

			break;
		case timecode_30:
			return 30;

			break;
		case timecode_30drop:
			return 30;

			break;
		case timecode_5994:
			return (60000.0/1001.0); //59.94;

			break;
		case timecode_60:
			return 60;

			break;
	        default:
			//std::cerr << "Editor received unexpected timecode type" << std::endl;
			break;
	}
	return 30.0;
}

bool
timecode_has_drop_frames(TimecodeFormat t)
{
	switch (t) {
		case timecode_23976:
			return false;

			break;
		case timecode_24:
			return false;

			break;
		case timecode_24976:
			return false;

			break;
		case timecode_25:
			return false;

			break;
		case timecode_2997:
			return false;

			break;
		case timecode_2997drop:
			return true;

			break;
		case timecode_30:
			return false;

			break;
		case timecode_30drop:
			return true;

			break;
		case timecode_5994:
			return false;

			break;
		case timecode_60:
			return false;

			break;
	        default:
			//error << "Editor received unexpected timecode type" << endmsg;
			break;
	}

	return false;
}

std::string
timecode_format_name (TimecodeFormat const t)
{
	switch (t) {
		case timecode_23976:
			return "23.98";

			break;
		case timecode_24:
			return "24";

			break;
		case timecode_24976:
			return "24.98";

			break;
		case timecode_25:
			return "25";

			break;
		case timecode_2997:
			return "29.97";

			break;
		case timecode_2997drop:
			return "29.97 drop";

			break;
		case timecode_30:
			return "30";

			break;
		case timecode_30drop:
			return "30 drop";

			break;
		case timecode_5994:
			return "59.94";

			break;
		case timecode_60:
			return "60";

			break;
	        default:
			break;
	}

	return "??";
}

} // namespace Timecode

std::ostream& 
operator<<(std::ostream& ostr, const Timecode::Time& t) 
{
	return t.print (ostr);
}
