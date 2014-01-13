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
#include <stdio.h>
#include <stdlib.h>

#include "timecode/time.h"

namespace Timecode {

double Time::default_rate = 30.0;


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

double
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
			return (30000.0/1001.0); //29.97;

			break;
		case timecode_2997drop:
			return (30000.0/1001.0); //29.97;

			break;
		case timecode_2997000:
			return 29.97;

			break;
		case timecode_2997000drop:
			return 29.97;

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
		case timecode_2997000:
			return false;

			break;
		case timecode_2997000drop:
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
		case timecode_2997000:
		case timecode_2997:
			return "29.97";

			break;
		case timecode_2997000drop:
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

std::string timecode_format_time (Timecode::Time TC)
{
	char buf[32];
	if (TC.negative) {
		snprintf (buf, sizeof (buf), "-%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 "%c%02" PRIu32,
				TC.hours, TC.minutes, TC.seconds, TC.drop ? ';' : ':', TC.frames);
	} else {
		snprintf (buf, sizeof (buf), " %02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 "%c%02" PRIu32,
				TC.hours, TC.minutes, TC.seconds, TC.drop ? ';' : ':', TC.frames);
	}
	return std::string(buf);
}

std::string timecode_format_sampletime (
		int64_t sample,
		double sample_frame_rate,
		double timecode_frames_per_second, bool timecode_drop_frames)
{
	Time t;
	sample_to_timecode(
			sample, t, false, false,
			timecode_frames_per_second, timecode_drop_frames,
			sample_frame_rate,
			80, false, 0);
	return timecode_format_time(t);
}

bool parse_timecode_format(std::string tc, Timecode::Time &TC) {
	char negative[2];
	char ignored[2];
	TC.subframes = 0;
	if (sscanf (tc.c_str(), "%[- ]%" PRId32 ":%" PRId32 ":%" PRId32 "%[:;]%" PRId32,
				negative, &TC.hours, &TC.minutes, &TC.seconds, ignored, &TC.frames) != 6) {
		TC.hours = TC.minutes = TC.seconds = TC.frames = 0;
		TC.negative = false;
		return false;
	}
	if (negative[0]=='-') {
		TC.negative = true;
	} else {
		TC.negative = false;
	}
	return true;
}

void
timecode_to_sample(
		Timecode::Time& timecode, int64_t& sample,
		bool use_offset, bool use_subframes,
    /* Note - framerate info is taken from Timecode::Time& */
		double sample_frame_rate /**< may include pull up/down */,
		uint32_t subframes_per_frame,
    /* optional offset  - can be improved: function pointer to lazily query this*/
		bool offset_is_negative, int64_t offset_samples
		)
{
	const double frames_per_timecode_frame = (double) sample_frame_rate / (double) timecode.rate;

	if (timecode.drop) {
		// The drop frame format was created to better approximate the 30000/1001 = 29.97002997002997....
		// framerate of NTSC color TV. The used frame rate of drop frame is 29.97, which drifts by about
		// 0.108 frame per hour, or about 1.3 frames per 12 hours. This is not perfect, but a lot better
		// than using 30 non drop, which will drift with about 1.8 frame per minute.
		// Using 29.97, drop frame real time can be accurate only every 10th minute (10 minutes of 29.97 fps
		// is exactly 17982 frames). One minute is 1798.2 frames, but we count 30 frames per second
		// (30 * 60 = 1800). This means that at the first minute boundary (at the end of 0:0:59:29) we
		// are 1.8 frames too late relative to real time. By dropping 2 frames (jumping to 0:1:0:2) we are
		// approx. 0.2 frames too early. This adds up with 0.2 too early for each minute until we are 1.8
		// frames too early at 0:9:0:2 (9 * 0.2 = 1.8). The 10th minute brings us 1.8 frames later again
		// (at end of 0:9:59:29), which sums up to 0 (we are back to zero at 0:10:0:0 :-).
		//
		// In table form:
		//
		// Timecode value    frames offset   subframes offset   seconds (rounded)  44100 sample (rounded)
		//  0:00:00:00        0.0             0                     0.000                0 (accurate)
		//  0:00:59:29        1.8           144                    60.027          2647177
		//  0:01:00:02       -0.2           -16                    60.060          2648648
		//  0:01:59:29        1.6           128                   120.020          5292883
		//  0:02:00:02       -0.4           -32                   120.053          5294354
		//  0:02:59:29        1.4           112                   180.013          7938588
		//  0:03:00:02       -0.6           -48                   180.047          7940060
		//  0:03:59:29        1.2            96                   240.007         10584294
		//  0:04:00:02       -0.8           -64                   240.040         10585766
		//  0:04:59:29        1.0            80                   300.000         13230000
		//  0:05:00:02       -1.0           -80                   300.033         13231471
		//  0:05:59:29        0.8            64                   359.993         15875706
		//  0:06:00:02       -1.2           -96                   360.027         15877177
		//  0:06:59:29        0.6            48                   419.987         18521411
		//  0:07:00:02       -1.4          -112                   420.020         18522883
		//  0:07:59:29        0.4            32                   478.980         21167117
		//  0:08:00:02       -1.6          -128                   480.013         21168589
		//  0:08:59:29        0.2            16                   539.973         23812823
		//  0:09:00:02       -1.8          -144                   540.007         23814294
		//  0:09:59:29        0.0+            0+                  599.967         26458529
		//  0:10:00:00        0.0             0                   600.000         26460000 (accurate)
		//
		//  Per Sigmond <per@sigmond.no>
		//
		//  This schma would compensate exactly for a frame-rate of 30 * 0.999. but the
		//  actual rate is 30000/1001 - which results in an offset of -3.6ms per hour or
		//  about -86ms over a 24-hour period. (SMPTE 12M-1999)
		//
		//  Robin Gareus <robin@gareus.org>

		const int64_t fps_i = ceil(timecode.rate);
		int64_t totalMinutes = 60 * timecode.hours + timecode.minutes;
		int64_t frameNumber  = fps_i * 3600 * timecode.hours
			+ fps_i * 60 * timecode.minutes
			+ fps_i * timecode.seconds + timecode.frames
			- 2 * (totalMinutes - totalMinutes / 10);
		sample = frameNumber * sample_frame_rate / (double) timecode.rate;
	} else {
		/*
		   Non drop is easy.. just note the use of
		   rint(timecode.rate) * frames_per_timecode_frame
		   (frames per Timecode second), which is larger than
		   frame_rate() in the non-integer Timecode rate case.
		*/

		sample = (int64_t)lrint((((timecode.hours * 60 * 60) + (timecode.minutes * 60) + timecode.seconds) * (lrint(timecode.rate) * frames_per_timecode_frame)) + (timecode.frames * frames_per_timecode_frame));
	}

	if (use_subframes) {
		sample += (int64_t) lrint(((double)timecode.subframes * frames_per_timecode_frame) / (double)subframes_per_frame);
	}

	if (use_offset) {
		if (offset_is_negative) {
			if (sample >= offset_samples) {
				sample -= offset_samples;
			} else {
				/* Prevent song-time from becoming negative */
				sample = 0;
			}
		} else {
			if (timecode.negative) {
				if (sample <= offset_samples) {
					sample = offset_samples - sample;
				} else {
					sample = 0;
				}
			} else {
				sample += offset_samples;
			}
		}
	}
}


void
sample_to_timecode (
		int64_t sample, Timecode::Time& timecode,
		bool use_offset, bool use_subframes,
    /* framerate info */
		double timecode_frames_per_second,
		bool   timecode_drop_frames,
		double sample_frame_rate/**< can include pull up/down */,
		uint32_t subframes_per_frame,
    /* optional offset  - can be improved: function pointer to lazily query this*/
		bool offset_is_negative, int64_t offset_samples
		)
{
	int64_t offset_sample;

	if (!use_offset) {
		timecode.negative = (sample < 0);
		offset_sample = llabs(sample);
	} else {
		if (offset_is_negative) {
			offset_sample = sample + offset_samples;
			timecode.negative = false;
		} else {
			if (sample < offset_samples) {
				offset_sample = (offset_samples - sample);
				timecode.negative = true;
			} else {
				offset_sample =  sample - offset_samples;
				timecode.negative = false;
			}
		}
	}

	if (timecode_drop_frames) {
		int64_t frameNumber = floor( (double)offset_sample * timecode_frames_per_second / sample_frame_rate);

		/* there are 17982 frames in 10 min @ 29.97df */
		const int64_t D = frameNumber / 17982;
		const int64_t M = frameNumber % 17982;

		timecode.subframes = lrint(subframes_per_frame
				* ((double)offset_sample * timecode_frames_per_second / sample_frame_rate - (double)frameNumber));

		if (timecode.subframes == subframes_per_frame) {
			timecode.subframes = 0;
			frameNumber++;
		}

		frameNumber +=  18*D + 2*((M - 2) / 1798);

		timecode.frames  =    frameNumber % 30;
		timecode.seconds =   (frameNumber / 30) % 60;
		timecode.minutes =  ((frameNumber / 30) / 60) % 60;
		timecode.hours   = (((frameNumber / 30) / 60) / 60);

	} else {
		double timecode_frames_left_exact;
		double timecode_frames_fraction;
		int64_t timecode_frames_left;
		const double frames_per_timecode_frame = sample_frame_rate / timecode_frames_per_second;
		const int64_t frames_per_hour = (int64_t)(3600 * lrint(timecode_frames_per_second) * frames_per_timecode_frame);

		timecode.hours = offset_sample / frames_per_hour;

		// Extract whole hours. Do this to prevent rounding errors with
		// high sample numbers in the calculations that follow.
		timecode_frames_left_exact = (double)(offset_sample % frames_per_hour) / frames_per_timecode_frame;
		timecode_frames_fraction = timecode_frames_left_exact - floor( timecode_frames_left_exact );

		timecode.subframes = (int32_t) lrint(timecode_frames_fraction * subframes_per_frame);
		timecode_frames_left = (int64_t) floor (timecode_frames_left_exact);

		if (use_subframes && timecode.subframes == subframes_per_frame) {
			timecode_frames_left++;
			timecode.subframes = 0;
		}

		timecode.minutes = timecode_frames_left / ((int32_t) lrint (timecode_frames_per_second) * 60);
		timecode_frames_left = timecode_frames_left % ((int32_t) lrint (timecode_frames_per_second) * 60);
		timecode.seconds = timecode_frames_left / (int32_t) lrint(timecode_frames_per_second);
		timecode.frames = timecode_frames_left % (int32_t) lrint(timecode_frames_per_second);
	}

	if (!use_subframes) {
		timecode.subframes = 0;
	}
	/* set frame rate and drop frame */
	timecode.rate = timecode_frames_per_second;
	timecode.drop = timecode_drop_frames;
}

} // namespace Timecode

std::ostream& 
operator<<(std::ostream& ostr, const Timecode::Time& t) 
{
	return t.print (ostr);
}
