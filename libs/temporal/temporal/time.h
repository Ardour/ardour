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

#include <cmath>
#include <ostream>
#include <inttypes.h>

#include "temporal/visibility.h"

namespace Timecode {

enum Wrap {
	NONE = 0,
	FRAMES,
	SECONDS,
	MINUTES,
	HOURS
};

enum TimecodeFormat {
	timecode_23976,
	timecode_24,
	timecode_24976,
	timecode_25,
	timecode_2997,
	timecode_2997drop,
	timecode_2997000,
	timecode_2997000drop,
	timecode_30,
	timecode_30drop,
	timecode_5994,
	timecode_60
};

struct LIBTEMPORAL_API Time {
	bool          negative;
	uint32_t      hours;
	uint32_t      minutes;
	uint32_t      seconds;
	uint32_t      frames;        ///< Timecode frames (not audio frames)
	uint32_t      subframes;     ///< Typically unused
	double        rate;          ///< Frame rate of this Time
	static double default_rate;  ///< Rate to use for default constructor
	bool          drop;          ///< Whether this Time uses dropframe Timecode

	Time (double a_rate = default_rate) {
		negative = false;
		hours = 0;
		minutes = 0;
		seconds = 0;
		frames = 0;
		subframes = 0;
		rate = a_rate;
		drop = (lrintf(100.f * (float)a_rate) == (long)2997);
	}

	bool operator== (const Time& other) const {
		return negative == other.negative && hours == other.hours &&
		       minutes == other.minutes && seconds == other.seconds &&
		       frames == other.frames && subframes == other.subframes &&
		       rate == other.rate && drop == other.drop;
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

Wrap LIBTEMPORAL_API increment (Time& timecode, uint32_t);
Wrap LIBTEMPORAL_API decrement (Time& timecode, uint32_t);
Wrap LIBTEMPORAL_API increment_subframes (Time& timecode, uint32_t);
Wrap LIBTEMPORAL_API decrement_subframes (Time& timecode, uint32_t);
Wrap LIBTEMPORAL_API increment_seconds (Time& timecode, uint32_t);
Wrap LIBTEMPORAL_API increment_minutes (Time& timecode, uint32_t);
Wrap LIBTEMPORAL_API increment_hours (Time& timecode, uint32_t);
void LIBTEMPORAL_API frames_floot (Time& timecode);
void LIBTEMPORAL_API seconds_floor (Time& timecode);
void LIBTEMPORAL_API minutes_floor (Time& timecode);
void LIBTEMPORAL_API hours_floor (Time& timecode);

double LIBTEMPORAL_API timecode_to_frames_per_second(TimecodeFormat const t);
bool LIBTEMPORAL_API timecode_has_drop_frames(TimecodeFormat const t);

std::string LIBTEMPORAL_API timecode_format_name (TimecodeFormat const t);

std::string LIBTEMPORAL_API timecode_format_time (Timecode::Time const timecode);

std::string LIBTEMPORAL_API timecode_format_sampletime (
		int64_t sample,
		double sample_sample_rate,
		double timecode_frames_per_second, bool timecode_drop_frames
		);

bool LIBTEMPORAL_API parse_timecode_format(std::string tc, Timecode::Time &TC);

void LIBTEMPORAL_API timecode_to_sample(
		Timecode::Time& timecode, int64_t& sample,
		bool use_offset, bool use_subframes,
    /* Note - framerate info is taken from Timecode::Time& */
		double sample_sample_rate /**< may include pull up/down */,
		uint32_t subframes_per_frame /**< must not be 0 if use_subframes==true */,
    /* optional offset  - can be improved: function pointer to lazily query this*/
		bool offset_is_negative, int64_t offset_samples
		);

void LIBTEMPORAL_API sample_to_timecode (
		int64_t sample, Timecode::Time& timecode,
		bool use_offset, bool use_subframes,
    /* framerate info */
		double timecode_frames_per_second,
		bool   timecode_drop_frames,
		double sample_sample_rate/**< can include pull up/down */,
		uint32_t subframes_per_frame,
    /* optional offset  - can be improved: function pointer to lazily query this*/
		bool offset_is_negative, int64_t offset_samples
		);


} // namespace Timecode

extern LIBTEMPORAL_API std::ostream& operator<< (std::ostream& ostr, const Timecode::Time& t);

#endif  // __timecode_time_h__
