/*
  Copyright (C) 2002-2010 Paul Davis

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

#ifndef __timecode_bbt_time_h__
#define __timecode_bbt_time_h__

#include <ostream>
#include <stdint.h>
#include <iomanip>

#include "timecode/visibility.h"

namespace Timecode {

/** Bar, Beat, Tick Time (i.e. Tempo-Based Time) */
struct LIBTIMECODE_API BBT_Time {
	static const double ticks_per_beat;

	uint32_t bars;
	uint32_t beats;
	uint32_t ticks;
	
	BBT_Time ()
		: bars (1), beats (1), ticks (0) {}
	
	BBT_Time (uint32_t ba, uint32_t be, uint32_t t)
		: bars (ba), beats (be), ticks (t) {}

        BBT_Time (double beats);
	
	bool operator< (const BBT_Time& other) const {
		return bars < other.bars ||
			(bars == other.bars && beats < other.beats) ||
			(bars == other.bars && beats == other.beats && ticks < other.ticks);
	}

	bool operator<= (const BBT_Time& other) const {
		return bars < other.bars ||
			(bars <= other.bars && beats <= other.beats) ||
			(bars <= other.bars && beats <= other.beats && ticks <= other.ticks);
	}

	bool operator> (const BBT_Time& other) const {
		return bars > other.bars ||
			(bars == other.bars && beats > other.beats) ||
			(bars == other.bars && beats == other.beats && ticks > other.ticks);
	}

	bool operator>= (const BBT_Time& other) const {
		return bars > other.bars ||
			(bars >= other.bars && beats >= other.beats) ||
			(bars >= other.bars && beats >= other.beats && ticks >= other.ticks);
	}
	
	bool operator== (const BBT_Time& other) const {
		return bars == other.bars && beats == other.beats && ticks == other.ticks;
	}

	bool operator!= (const BBT_Time& other) const {
		return bars != other.bars || beats != other.beats || ticks != other.ticks;
	}
};
	
}

inline std::ostream&
operator<< (std::ostream& o, const Timecode::BBT_Time& bbt)
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
