/*
    Copyright (C) 2006 Paul Davis 
    
    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
    
    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.
    
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_meter_h__
#define __ardour_meter_h__

#include <vector>
#include <ardour/types.h>
#include <pbd/fastlog.h>

namespace ARDOUR {

class BufferSet;
class ChanCount;
class Session;


/** Meters peaks on the input and stores them for access.
 */
class PeakMeter {
public:
	PeakMeter(Session& s) : _session(s) {}

	void setup (const ChanCount& in);
	void reset ();
	void reset_max ();

	/** Compute peaks */
	void run (BufferSet& bufs, nframes_t nframes, nframes_t offset=0);
	
	float peak_power (uint32_t n) { 
		if (n < _visible_peak_power.size()) {
			return _visible_peak_power[n];
		} else {
			return minus_infinity();
		}
	}
	
	float max_peak_power (uint32_t n) {
		if (n < _max_peak_power.size()) {
			return _max_peak_power[n];
		} else {
			return minus_infinity();
		}
	}

private:
	
	friend class IO;
	void meter();

	Session&           _session;
	std::vector<float> _peak_power;
	std::vector<float> _visible_peak_power;
	std::vector<float> _max_peak_power;
};


} // namespace ARDOUR

#endif // __ardour_meter_h__
