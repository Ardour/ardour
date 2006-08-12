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

#include <ardour/meter.h>
#include <algorithm>
#include <cmath>
#include <ardour/buffer_set.h>
#include <ardour/peak.h>
#include <ardour/dB.h>
#include <ardour/session.h>

namespace ARDOUR {


/** Get peaks from @a bufs
 * Input acceptance is lenient - the first n audio buffers from @a bufs will
 * be metered, where n was set by the last call to setup(), excess meters will
 * be set to 0.
 */
void
PeakMeter::run (BufferSet& bufs, jack_nframes_t nframes, jack_nframes_t offset)
{
	size_t meterable = std::min(bufs.count().get(DataType::AUDIO), _peak_power.size());

	// Meter what we have
	for (size_t n = 0; n < meterable; ++n) {
		_peak_power[n] = compute_peak (bufs.get_audio(n).data(nframes, offset), nframes, _peak_power[n]); 
	}

	// Zero any excess peaks
	for (size_t n = meterable; n < _peak_power.size(); ++n) {
		_peak_power[n] = 0;
	}
}

void
PeakMeter::reset ()
{
	for (size_t i = 0; i < _peak_power.size(); ++i) {
		_peak_power[i] = 0;
	}
}

void
PeakMeter::setup (const ChanCount& in)
{
	uint32_t limit = in.get(DataType::AUDIO);

	while (_peak_power.size() < limit) {
		_peak_power.push_back (0);
		_visible_peak_power.push_back (0);
	}

	assert(_peak_power.size() == limit);
	assert(_visible_peak_power.size() == limit);
}

/** To be driven by the Meter signal from IO.
 * Caller MUST hold io_lock!
 */
void
PeakMeter::meter ()
{
	assert(_visible_peak_power.size() == _peak_power.size());

	const size_t limit = _peak_power.size();

	for (size_t n = 0; n < limit; ++n) {

		/* XXX we should use atomic exchange here */

		/* grab peak since last read */

 		float new_peak = _peak_power[n];
		_peak_power[n] = 0;
		
		/* compute new visible value using falloff */

		if (new_peak > 0.0) {
			new_peak = coefficient_to_dB (new_peak);
		} else {
			new_peak = minus_infinity();
		}
		
		if (_session.meter_falloff() == 0.0f || new_peak > _visible_peak_power[n]) {
			_visible_peak_power[n] = new_peak;
		} else {
			// do falloff
			new_peak = _visible_peak_power[n] - _session.meter_falloff();
			_visible_peak_power[n] = std::max (new_peak, -INFINITY);
		}
	}
}

} // namespace ARDOUR
