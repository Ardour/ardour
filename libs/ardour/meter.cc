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

#include "ardour/meter.h"
#include <algorithm>
#include <cmath>
#include "ardour/buffer_set.h"
#include "ardour/peak.h"
#include "ardour/dB.h"
#include "ardour/session.h"
#include "ardour/midi_buffer.h"
#include "ardour/audio_buffer.h"
#include "ardour/runtime_functions.h"

using namespace std;

using namespace ARDOUR;

sigc::signal<void> Metering::Meter;
Glib::StaticMutex  Metering::m_meter_signal_lock;

sigc::connection
Metering::connect (sigc::slot<void> the_slot)
{
	// SignalProcessor::Meter is emitted from another thread so the
	// Meter signal must be protected.
	Glib::Mutex::Lock guard (m_meter_signal_lock);
	return Meter.connect (the_slot);
}

void
Metering::disconnect (sigc::connection& c)
{
	Glib::Mutex::Lock guard (m_meter_signal_lock);
	c.disconnect ();
}

/**
    Update the meters.

    The meter signal lock is taken to prevent modification of the 
    Meter signal while updating the meters, taking the meter signal
    lock prior to taking the io_lock ensures that all IO will remain 
    valid while metering.
*/   
void
Metering::update_meters()
{
	Glib::Mutex::Lock guard (m_meter_signal_lock);
	Meter(); /* EMIT SIGNAL */
}

/** Get peaks from @a bufs
 * Input acceptance is lenient - the first n buffers from @a bufs will
 * be metered, where n was set by the last call to setup(), excess meters will
 * be set to 0.
 */
void
PeakMeter::run (BufferSet& bufs, sframes_t start_frame, sframes_t end_frame, nframes_t nframes)
{
	if (!_active) {
		return;
	}

	const uint32_t n_audio = min(_configured_input.n_audio(), bufs.count().n_audio());
	const uint32_t n_midi  = min(_configured_input.n_midi(), bufs.count().n_midi());
	
	uint32_t n = 0;
	
	// Meter MIDI in to the first n_midi peaks
	for (uint32_t i = 0; i < n_midi; ++i, ++n) {
		float val = 0.0f;
		for (MidiBuffer::iterator e = bufs.get_midi(i).begin(); e != bufs.get_midi(i).end(); ++e) {
			const Evoral::MIDIEvent<nframes_t> ev(*e, false);
			if (ev.is_note_on()) {
				const float this_vel = log(ev.buffer()[2] / 127.0 * (M_E*M_E-M_E) + M_E) - 1.0;
				if (this_vel > val) {
					val = this_vel;
				}
			} else {
				val += 1.0 / bufs.get_midi(n).capacity();
				if (val > 1.0) {
					val = 1.0;
				}
			}
		}
		_peak_power[n] = val;
	}

	// Meter audio in to the rest of the peaks
	for (uint32_t i = 0; i < n_audio; ++i, ++n) {
		_peak_power[n] = compute_peak (bufs.get_audio(i).data(), nframes, _peak_power[n]); 
	}

	// Zero any excess peaks
	for (uint32_t i = n; i < _peak_power.size(); ++i) {
		_peak_power[i] = 0.0f;
	}
}

PeakMeter::PeakMeter (Session& s, const XMLNode& node)
	: Processor (s, node)
{
}

void
PeakMeter::reset ()
{
	for (size_t i = 0; i < _peak_power.size(); ++i) {
		_peak_power[i] = 0.0f;
	}
}

void
PeakMeter::reset_max ()
{
	for (size_t i = 0; i < _max_peak_power.size(); ++i) {
		_max_peak_power[i] = -INFINITY;
	}
}

bool
PeakMeter::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
	out = in;
	return true;
}

bool
PeakMeter::configure_io (ChanCount in, ChanCount out)
{
	if (out != in) { // always 1:1
		return false;
	}
	
	uint32_t limit = in.n_total();
	
	while (_peak_power.size() > limit) {
		_peak_power.pop_back();
		_visible_peak_power.pop_back();
		_max_peak_power.pop_back();
	}

	while (_peak_power.size() < limit) {
		_peak_power.push_back(0);
		_visible_peak_power.push_back(minus_infinity());
		_max_peak_power.push_back(minus_infinity());
	}

	assert(_peak_power.size() == limit);
	assert(_visible_peak_power.size() == limit);
	assert(_max_peak_power.size() == limit);

	return Processor::configure_io (in, out);
}

/** To be driven by the Meter signal from IO.
 * Caller MUST hold its own processor_lock to prevent reconfiguration
 * of meter size during this call.
 */

void
PeakMeter::meter ()
{
	assert(_visible_peak_power.size() == _peak_power.size());

	const size_t limit = _peak_power.size();

	for (size_t n = 0; n < limit; ++n) {

		/* grab peak since last read */

 		float new_peak = _peak_power[n]; /* XXX we should use atomic exchange from here ... */
		_peak_power[n] = 0;              /* ... to here */
		
		/* compute new visible value using falloff */

		if (new_peak > 0.0) {
			new_peak = coefficient_to_dB (new_peak);
		} else {
			new_peak = minus_infinity();
		}
		
		/* update max peak */
		
		_max_peak_power[n] = std::max (new_peak, _max_peak_power[n]);
		
		if (Config->get_meter_falloff() == 0.0f || new_peak > _visible_peak_power[n]) {
			_visible_peak_power[n] = new_peak;
		} else {
			// do falloff
			new_peak = _visible_peak_power[n] - (Config->get_meter_falloff() * 0.01f);
			_visible_peak_power[n] = std::max (new_peak, -INFINITY);
		}
	}
}

XMLNode&
PeakMeter::state (bool full_state)
{
	XMLNode& node (Processor::state (full_state));
	node.add_property("type", "meter");
	return node;
}


bool
PeakMeter::visible() const
{
	return true;
}
