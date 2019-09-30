/*
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __timecode_time_h__
#define __timecode_time_h__

#include <cmath>
#include <inttypes.h>
#include <ostream>

#include "temporal/visibility.h"

namespace Timecode
{

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
	uint32_t      frames;       ///< Timecode frames (not audio frames)
	uint32_t      subframes;    ///< Typically unused
	double        rate;         ///< Frame rate of this Time
	static double default_rate; ///< Rate to use for default constructor
	bool          drop;         ///< Whether this Time uses dropframe Timecode

	Time (double a_rate = default_rate)
	{
		negative  = false;
		hours     = 0;
		minutes   = 0;
		seconds   = 0;
		frames    = 0;
		subframes = 0;
		rate      = a_rate;
		drop      = (lrintf (100.f * (float)a_rate) == (long)2997);
	}

	bool operator== (const Time& other) const
	{
		return negative == other.negative && hours == other.hours &&
		       minutes == other.minutes && seconds == other.seconds &&
		       frames == other.frames && subframes == other.subframes &&
		       rate == other.rate && drop == other.drop;
	}

	std::ostream& print (std::ostream& ostr) const
	{
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

double LIBTEMPORAL_API timecode_to_frames_per_second (TimecodeFormat const t);
bool LIBTEMPORAL_API timecode_has_drop_frames (TimecodeFormat const t);

std::string LIBTEMPORAL_API timecode_format_name (TimecodeFormat const t);
std::string LIBTEMPORAL_API timecode_format_time (Timecode::Time const timecode);

bool LIBTEMPORAL_API parse_timecode_format (std::string tc, Timecode::Time& TC);

std::string LIBTEMPORAL_API
timecode_format_sampletime (
    int64_t sample,
    double  sample_sample_rate,
    double timecode_frames_per_second, bool timecode_drop_frames);

/** Convert timecode (frames per second) to audio sample time (samples per second)
 *
 * @param timecode Timecode to convert (also includes frame-rate)
 * @param sample returned corresponding audio sample time
 * @param use_offset apply offset as given by \p offset_is_negative and \p offset_samples
 * @param use_subframes use \p subframes_per_frame when converting
 * @param sample_sample_rate target sample-rate, may include pull up/down
 * @param subframes_per_frame sub-frames per frame -- must not be 0 if \p use_subframes \c == \c true
 * @param offset_is_negative true if offset_samples is to be subtracted
 * @param offset_samples sample offset to add or subtract
 */
void LIBTEMPORAL_API
timecode_to_sample (
    Timecode::Time const& timecode, int64_t& sample,
    bool use_offset, bool use_subframes,
    double   sample_sample_rate,
    uint32_t subframes_per_frame,
    bool offset_is_negative, int64_t offset_samples);

/** Convert audio sample time (samples per second) to timecode (frames per second)
 *
 * @param sample audio sample time to convert
 * @param timecode resulting Timecode
 * @param use_offset apply offset as given by \p offset_is_negative and \p offset_samples
 * @param use_subframes use \p subframes_per_frame when converting
 * @param timecode_frames_per_second target framerate
 * @param timecode_drop_frames true if fps uses drop-frame-counting. only valid for \c 29.97 \c = \c 30000/1001 fps
 * @param sample_sample_rate source sample-rate, may include pull up/down
 * @param subframes_per_frame sub-frames per frame -- must not be 0 if \p use_subframes \c == \c true
 * @param offset_is_negative true if offset_samples is to be subtracted
 * @param offset_samples sample offset to add or subtract
 */
void LIBTEMPORAL_API
sample_to_timecode (
    int64_t sample, Timecode::Time& timecode,
    bool use_offset, bool use_subframes,
    double   timecode_frames_per_second,
    bool     timecode_drop_frames,
    double   sample_sample_rate,
    uint32_t subframes_per_frame,
    bool offset_is_negative, int64_t offset_samples);

} // namespace Timecode

extern LIBTEMPORAL_API std::ostream& operator<< (std::ostream& ostr, const Timecode::Time& t);

#endif // __timecode_time_h__
