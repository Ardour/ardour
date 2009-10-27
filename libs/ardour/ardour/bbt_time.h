/*
    Copyright (C) 2002-2009 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_bbt_time_h__
#define __ardour_bbt_time_h__

#include <stdint.h>

namespace ARDOUR {

class TempoMetric;

struct BBT_Time {
    uint32_t bars;
    uint32_t beats;
    uint32_t ticks;
    
    BBT_Time() {
	    bars = 1;
	    beats = 1;
	    ticks = 0;
    }
    
    BBT_Time (uint32_t ba, uint32_t be, uint32_t t)
         : bars (ba), beats (be), ticks (t) {}

    bool operator< (const BBT_Time& other) const {
	    return bars < other.bars ||
		    (bars == other.bars && beats < other.beats) ||
		    (bars == other.bars && beats == other.beats && ticks < other.ticks);
    }
    
    bool operator== (const BBT_Time& other) const {
	    return bars == other.bars && beats == other.beats && ticks == other.ticks;
    }
    
    static bool add (BBT_Time& target, const BBT_Time& other, const TempoMetric& metric);
    static bool subtract (BBT_Time& target, const BBT_Time& other, const TempoMetric& metric);
};

}

#endif /* __ardour_bbt_time_h__ */
