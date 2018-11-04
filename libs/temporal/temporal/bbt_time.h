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

#ifndef __timecode_bbt_time_h__
#define __timecode_bbt_time_h__

#include <ostream>
#include <stdint.h>
#include <iomanip>
#include <exception>

#include "temporal/visibility.h"

namespace Timecode {

/** Bar, Beat, Tick Time (i.e. Tempo-Based Time) */
struct LIBTEMPORAL_API BBT_Time
{
	static const double ticks_per_beat;

	/* note that it is illegal for BBT_Time to have bars==0 or
	 * beats==0. The "neutral" or "default" value is 1|1|0
	 */

	int32_t bars;
	int32_t beats;
	int32_t ticks;

	struct IllegalBBTTimeException : public std::exception {
		virtual const char* what() const throw() { return "illegal BBT time (bars or beats were zero)"; }
	};

	BBT_Time () : bars (1), beats (1), ticks (0) {}
	BBT_Time (int32_t ba, uint32_t be, uint32_t t) : bars (ba), beats (be), ticks (t) { if (!bars || !beats) { throw IllegalBBTTimeException(); } }

	bool operator< (const BBT_Time& other) const {
		return bars < other.bars ||
			(bars == other.bars && beats < other.beats) ||
			(bars == other.bars && beats == other.beats && ticks < other.ticks);
	}

	bool operator<= (const BBT_Time& other) const {
		return bars < other.bars ||
			(bars <= other.bars && beats < other.beats) ||
			(bars <= other.bars && beats <= other.beats && ticks <= other.ticks);
	}

	bool operator> (const BBT_Time& other) const {
		return bars > other.bars ||
			(bars == other.bars && beats > other.beats) ||
			(bars == other.bars && beats == other.beats && ticks > other.ticks);
	}

	bool operator>= (const BBT_Time& other) const {
		return bars > other.bars ||
			(bars >= other.bars && beats > other.beats) ||
			(bars >= other.bars && beats >= other.beats && ticks >= other.ticks);
	}

	bool operator== (const BBT_Time& other) const {
		return bars == other.bars && beats == other.beats && ticks == other.ticks;
	}

	bool operator!= (const BBT_Time& other) const {
		return bars != other.bars || beats != other.beats || ticks != other.ticks;
	}

	/* it would be nice to provide operator+(BBT_Time const&) and
	 * operator-(BBT_Time const&) but this math requires knowledge of the
	 * meter (time signature) used to define 1 bar, and so cannot be
	 * carried out with only two BBT_Time values.
	 */

	BBT_Time round_to_beat () const { return ticks >= (ticks_per_beat/2) ? BBT_Time (bars, beats+1, 0) : BBT_Time (bars, beats, 0); }
	BBT_Time round_down_to_beat () const { return BBT_Time (bars, beats, 0); }
	BBT_Time round_up_to_beat () const { return ticks ? BBT_Time (bars, beats+1, 0) : *this; }

	/* cannot implement round_to_bar() without knowing meter (time
	 * signature) information.
	 */
};

struct LIBTEMPORAL_API BBT_Offset
{
	int32_t bars;
	int32_t beats;
	int32_t ticks;

	/* this is a variant for which bars==0 and/or beats==0 is legal. It
	 * represents an offset from a given BBT_Time and is used when doing
	 * add/subtract operations on a BBT_Time.
	 */

	BBT_Offset () : bars (0), beats (0), ticks (0) {}
	BBT_Offset (int32_t ba, uint32_t be, uint32_t t) : bars (ba), beats (be), ticks (t) {}
	BBT_Offset (BBT_Time const & bbt) : bars (bbt.bars), beats (bbt.beats), ticks (bbt.ticks) {}
	BBT_Offset (double beats);
};

}

inline std::ostream&
operator<< (std::ostream& o, const Timecode::BBT_Time& bbt)
{
	o << bbt.bars << '|' << bbt.beats << '|' << bbt.ticks;
	return o;
}

inline std::ostream&
operator<< (std::ostream& o, const Timecode::BBT_Offset& bbt)
{
	o << bbt.bars << '|' << bbt.beats << '|' << bbt.ticks;
	return o;
}

inline std::ostream&
print_padded (std::ostream& o, const Timecode::BBT_Time& bbt)
{
	o << std::setfill ('0') << std::right
	  << std::setw (3) << bbt.bars << "|"
	  << std::setw (2) << bbt.beats << "|"
	  << std::setw (4) << bbt.ticks;

	return o;
}

#endif /* __timecode_bbt_time_h__ */
