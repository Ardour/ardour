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

#define SMPTE_IS_AROUND_ZERO( sm ) (!(sm).frames && !(sm).seconds && !(sm).minutes && !(sm).hours)
#define SMPTE_IS_ZERO( sm ) (!(sm).frames && !(sm).seconds && !(sm).minutes && !(sm).hours && !(sm.subframes))

#include "control_protocol/smpte.h"
#include "ardour/rc_configuration.h"

namespace SMPTE {

float Time::default_rate = 30.0;


/** Increment @a smpte by exactly one frame (keep subframes value).
 * Realtime safe.
 * @return true if seconds wrap.
 */
Wrap
increment( Time& smpte, uint32_t subframes_per_frame )
{
	Wrap wrap = NONE;

	if (smpte.negative) {
		if (SMPTE_IS_AROUND_ZERO(smpte) && smpte.subframes) {
			// We have a zero transition involving only subframes
			smpte.subframes = subframes_per_frame - smpte.subframes;
			smpte.negative = false;
			return SECONDS;
		}
    
		smpte.negative = false;
		wrap = decrement( smpte, subframes_per_frame );
		if (!SMPTE_IS_ZERO( smpte )) {
			smpte.negative = true;
		}
		return wrap;
	}

	switch ((int)ceil(smpte.rate)) {
	case 24:
		if (smpte.frames == 23) {
			smpte.frames = 0;
			wrap = SECONDS;
		}
		break;
	case 25:
		if (smpte.frames == 24) {
			smpte.frames = 0;
			wrap = SECONDS;
		}
		break;
	case 30:
	        if (smpte.drop) {
		       if (smpte.frames == 29) {
			      if ( ((smpte.minutes + 1) % 10) && (smpte.seconds == 59) ) {
				     smpte.frames = 2;
			      }
			      else {
				     smpte.frames = 0;
			      }
			      wrap = SECONDS;
		       }
		} else {

		       if (smpte.frames == 29) {
			      smpte.frames = 0;
			      wrap = SECONDS;
		       }
		}
		break;
	case 60:
	        if (smpte.frames == 59) {
		        smpte.frames = 0;
			wrap = SECONDS;
		}
		break;
	}
  
	if (wrap == SECONDS) {
		if (smpte.seconds == 59) {
			smpte.seconds = 0;
			wrap = MINUTES;
			if (smpte.minutes == 59) {
				smpte.minutes = 0;
				wrap = HOURS;
				smpte.hours++;
			} else {
				smpte.minutes++;
			}
		} else {
			smpte.seconds++;
		}
	} else {
		smpte.frames++;
	}
  
	return wrap;
}


/** Decrement @a smpte by exactly one frame (keep subframes value)
 * Realtime safe.
 * @return true if seconds wrap. */
Wrap
decrement( Time& smpte, uint32_t subframes_per_frame )
{
	Wrap wrap = NONE;
  
  
	if (smpte.negative || SMPTE_IS_ZERO(smpte)) {
		smpte.negative = false;
		wrap = increment( smpte, subframes_per_frame );
		smpte.negative = true;
		return wrap;
	} else if (SMPTE_IS_AROUND_ZERO(smpte) && smpte.subframes) {
		// We have a zero transition involving only subframes
		smpte.subframes = subframes_per_frame - smpte.subframes;
		smpte.negative = true;
		return SECONDS;
	}
  
	switch ((int)ceil(smpte.rate)) {
	case 24:
		if (smpte.frames == 0) {
			smpte.frames = 23;
			wrap = SECONDS;
		}
		break;
	case 25:
		if (smpte.frames == 0) {
			smpte.frames = 24;
			wrap = SECONDS;
		}
		break;
	case 30:
	        if (smpte.drop) {
		        if ((smpte.minutes % 10) && (smpte.seconds == 0)) {
			        if (smpte.frames <= 2) {
				        smpte.frames = 29;
					wrap = SECONDS;
				}
			} else if (smpte.frames == 0) {
			        smpte.frames = 29;
				wrap = SECONDS;
			}
			
		} else {
		        if (smpte.frames == 0) {
			        smpte.frames = 29;
				wrap = SECONDS;
			}
		}
		break;
	case 60:
	        if (smpte.frames == 0) {
		        smpte.frames = 59;
			wrap = SECONDS;
		}
		break;
	}
  
	if (wrap == SECONDS) {
		if (smpte.seconds == 0) {
			smpte.seconds = 59;
			wrap = MINUTES;
			if (smpte.minutes == 0) {
				smpte.minutes = 59;
				wrap = HOURS;
				smpte.hours--;
			}
			else {
				smpte.minutes--;
			}
		} else {
			smpte.seconds--;
		}
	} else {
		smpte.frames--;
	}
  
	if (SMPTE_IS_ZERO( smpte )) {
		smpte.negative = false;
	}
  
	return wrap;
}


/** Go to lowest absolute subframe value in this frame (set to 0 :-) ) */
void
frames_floor( Time& smpte )
{
	smpte.subframes = 0;
	if (SMPTE_IS_ZERO(smpte)) {
		smpte.negative = false;
	}
}


/** Increment @a smpte by one subframe */
Wrap
increment_subframes( Time& smpte, uint32_t subframes_per_frame )
{
	Wrap wrap = NONE;
  
	if (smpte.negative) {
		smpte.negative = false;
		wrap = decrement_subframes( smpte, subframes_per_frame );
		if (!SMPTE_IS_ZERO(smpte)) {
			smpte.negative = true;
		}
		return wrap;
	}
  
	smpte.subframes++;
	if (smpte.subframes >= subframes_per_frame) {
		smpte.subframes = 0;
		increment( smpte, subframes_per_frame );
		return FRAMES;
	}
	return NONE;
}


/** Decrement @a smpte by one subframe */
Wrap
decrement_subframes( Time& smpte, uint32_t subframes_per_frame )
{
	Wrap wrap = NONE;
  
	if (smpte.negative) {
		smpte.negative = false;
		wrap = increment_subframes( smpte, subframes_per_frame );
		smpte.negative = true;
		return wrap;
	}
  
	if (smpte.subframes <= 0) {
		smpte.subframes = 0;
		if (SMPTE_IS_ZERO(smpte)) {
			smpte.negative = true;
			smpte.subframes = 1;
			return FRAMES;
		} else {
			decrement( smpte, subframes_per_frame );
			smpte.subframes = 79;
			return FRAMES;
		}
	} else {
		smpte.subframes--;
		if (SMPTE_IS_ZERO(smpte)) {
			smpte.negative = false;
		}
		return NONE;
	}
}


/** Go to next whole second (frames == 0 or frames == 2) */
Wrap
increment_seconds( Time& smpte, uint32_t subframes_per_frame )
{
	Wrap wrap = NONE;
  
	// Clear subframes
	frames_floor( smpte );
  
	if (smpte.negative) {
		// Wrap second if on second boundary
		wrap = increment(smpte, subframes_per_frame);
		// Go to lowest absolute frame value
		seconds_floor( smpte );
		if (SMPTE_IS_ZERO(smpte)) {
			smpte.negative = false;
		}
	} else {
		// Go to highest possible frame in this second
	  switch ((int)ceil(smpte.rate)) {
		case 24:
			smpte.frames = 23;
			break;
		case 25:
			smpte.frames = 24;
			break;
		case 30:
			smpte.frames = 29;
			break;
		case 60:
			smpte.frames = 59;
			break;
		}
    
		// Increment by one frame
		wrap = increment( smpte, subframes_per_frame );
	}
  
	return wrap;
}


/** Go to lowest (absolute) frame value in this second
 * Doesn't care about positive/negative */
void
seconds_floor( Time& smpte )
{
	// Clear subframes
	frames_floor( smpte );
  
	// Go to lowest possible frame in this second
	switch ((int)ceil(smpte.rate)) {
	case 24:
	case 25:
	case 30:
	case 60:
	        if (!(smpte.drop)) {
		        smpte.frames = 0;
		} else {

		        if ((smpte.minutes % 10) && (smpte.seconds == 0)) {
			        smpte.frames = 2;
			} else {
			        smpte.frames = 0;
			}
		}
		break;
	}
  
	if (SMPTE_IS_ZERO(smpte)) {
		smpte.negative = false;
	}
}


/** Go to next whole minute (seconds == 0, frames == 0 or frames == 2) */
Wrap
increment_minutes( Time& smpte, uint32_t subframes_per_frame )
{
	Wrap wrap = NONE;
  
	// Clear subframes
	frames_floor( smpte );
  
	if (smpte.negative) {
		// Wrap if on minute boundary
		wrap = increment_seconds( smpte, subframes_per_frame );
		// Go to lowest possible value in this minute
		minutes_floor( smpte );
	} else {
		// Go to highest possible second
		smpte.seconds = 59;
		// Wrap minute by incrementing second
		wrap = increment_seconds( smpte, subframes_per_frame );
	}
  
	return wrap;
}


/** Go to lowest absolute value in this minute */
void
minutes_floor( Time& smpte )
{
	// Go to lowest possible second
	smpte.seconds = 0;
	// Go to lowest possible frame
	seconds_floor( smpte );

	if (SMPTE_IS_ZERO(smpte)) {
		smpte.negative = false;
	}
}


/** Go to next whole hour (minute = 0, second = 0, frame = 0) */
Wrap
increment_hours( Time& smpte, uint32_t subframes_per_frame )
{
	Wrap wrap = NONE;
  
	// Clear subframes
	frames_floor(smpte);
  
	if (smpte.negative) {
		// Wrap if on hour boundary
		wrap = increment_minutes( smpte, subframes_per_frame );
		// Go to lowest possible value in this hour
		hours_floor( smpte );
	} else {
		smpte.minutes = 59;
		wrap = increment_minutes( smpte, subframes_per_frame );
	}
  
	return wrap;
}


/** Go to lowest absolute value in this hour */
void
hours_floor( Time& smpte )
{
	smpte.minutes = 0;
	smpte.seconds = 0;
	smpte.frames = 0;
	smpte.subframes = 0;
  
	if (SMPTE_IS_ZERO(smpte)) {
		smpte.negative = false;
	}
}


} // namespace SMPTE
