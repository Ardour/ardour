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

#include <algorithm>
#include <cmath>

#include "pbd/compose.h"

#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/dB.h"
#include "ardour/meter.h"
#include "ardour/midi_buffer.h"
#include "ardour/session.h"
#include "ardour/rc_configuration.h"
#include "ardour/runtime_functions.h"

using namespace std;

using namespace ARDOUR;

PBD::Signal0<void> Metering::Meter;

PeakMeter::PeakMeter (Session& s, const std::string& name)
    : Processor (s, string_compose ("meter-%1", name))
{
	Kmeterdsp::init(s.nominal_frame_rate());
}

PeakMeter::~PeakMeter ()
{
	while (_kmeter.size() > 0) {
		delete (_kmeter.back());
		_kmeter.pop_back();
	}
}


/** Get peaks from @a bufs
 * Input acceptance is lenient - the first n buffers from @a bufs will
 * be metered, where n was set by the last call to setup(), excess meters will
 * be set to 0.
 */
void
PeakMeter::run (BufferSet& bufs, framepos_t /*start_frame*/, framepos_t /*end_frame*/, pframes_t nframes, bool)
{
	if (!_active && !_pending_active) {
		return;
	}

	// cerr << "meter " << name() << " runs with " << bufs.available() << " inputs\n";

	const uint32_t n_audio = min (current_meters.n_audio(), bufs.count().n_audio());
	const uint32_t n_midi  = min (current_meters.n_midi(), bufs.count().n_midi());

	uint32_t n = 0;

	// Meter MIDI in to the first n_midi peaks
	for (uint32_t i = 0; i < n_midi; ++i, ++n) {
		float val = 0.0f;
		MidiBuffer& buf (bufs.get_midi(i));
		
		for (MidiBuffer::iterator e = buf.begin(); e != buf.end(); ++e) {
			const Evoral::MIDIEvent<framepos_t> ev(*e, false);
			if (ev.is_note_on()) {
				const float this_vel = ev.buffer()[2] / 127.0;
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
		_peak_signal[n] = max (val, _peak_signal[n]);
	}

	// Meter audio in to the rest of the peaks
	for (uint32_t i = 0; i < n_audio; ++i, ++n) {
		_peak_signal[n] = compute_peak (bufs.get_audio(i).data(), nframes, _peak_signal[n]);
		if (/* TODO use separate bit-flags for mixer,meterbridge,.. */ /* 1 || */  _meter_type & MeterKrms) {
			_kmeter[i]->process(bufs.get_audio(i).data(), nframes);
		}
	}

	// Zero any excess peaks
	for (uint32_t i = n; i < _peak_signal.size(); ++i) {
		_peak_signal[i] = 0.0f;
	}

	_active = _pending_active;
}

void
PeakMeter::reset ()
{
	for (size_t i = 0; i < _peak_signal.size(); ++i) {
		_peak_signal[i] = 0.0f;
	}
}

void
PeakMeter::reset_max ()
{
	for (size_t i = 0; i < _max_peak_power.size(); ++i) {
		_max_peak_power[i] = -INFINITY;
		_max_peak_signal[i] = 0;
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

	current_meters = in;

	reset_max_channels (in);

	return Processor::configure_io (in, out);
}

void
PeakMeter::reflect_inputs (const ChanCount& in)
{
	current_meters = in;

	const size_t limit = min (_peak_signal.size(), (size_t) current_meters.n_total ());
	const size_t n_midi  = min (_peak_signal.size(), (size_t) current_meters.n_midi());
	const size_t n_audio = current_meters.n_audio();

	for (size_t n = 0; n < limit; ++n) {
		if (n < n_midi) {
			_visible_peak_power[n] = 0;
		} else {
			_visible_peak_power[n] = -INFINITY;
		}
	}

	for (size_t n = 0; n < n_audio; ++n) {
		_kmeter[n]->reset();
	}

	reset_max();

	ConfigurationChanged (in, in); /* EMIT SIGNAL */
}

void
PeakMeter::reset_max_channels (const ChanCount& chn)
{
	uint32_t const limit = chn.n_total();
	const size_t n_audio = chn.n_audio();

	while (_peak_signal.size() > limit) {
		_peak_signal.pop_back();
		_visible_peak_power.pop_back();
		_max_peak_signal.pop_back();
		_max_peak_power.pop_back();
	}

	while (_peak_signal.size() < limit) {
		_peak_signal.push_back(0);
		_visible_peak_power.push_back(minus_infinity());
		_max_peak_signal.push_back(0);
		_max_peak_power.push_back(minus_infinity());
	}

	assert(_peak_signal.size() == limit);
	assert(_visible_peak_power.size() == limit);
	assert(_max_peak_signal.size() == limit);
	assert(_max_peak_power.size() == limit);

	/* alloc/free other audio-only meter types. */
	while (_kmeter.size() > n_audio) {
		delete (_kmeter.back());
		_kmeter.pop_back();
	}
	while (_kmeter.size() < n_audio) {
		_kmeter.push_back(new Kmeterdsp());
	}
	assert(_kmeter.size() == n_audio);
}

/** To be driven by the Meter signal from IO.
 * Caller MUST hold its own processor_lock to prevent reconfiguration
 * of meter size during this call.
 */

void
PeakMeter::meter ()
{
	if (!_active) {
		return;
	}

	assert(_visible_peak_power.size() == _peak_signal.size());

	const size_t limit = min (_peak_signal.size(), (size_t) current_meters.n_total ());
	const size_t n_midi  = min (_peak_signal.size(), (size_t) current_meters.n_midi());

	for (size_t n = 0; n < limit; ++n) {

		/* grab peak since last read */

		float new_peak = _peak_signal[n]; /* XXX we should use atomic exchange from here ... */
		_peak_signal[n] = 0;              /* ... to here */

		if (n < n_midi) {
			_max_peak_power[n] = -INFINITY; // std::max (new_peak, _max_peak_power[n]); // XXX
			_max_peak_signal[n] = 0;
			if (Config->get_meter_falloff() == 0.0f || new_peak > _visible_peak_power[n]) {
				;
			} else {
				/* empirical WRT to falloff times , 0.01f ^= 100 Hz update rate */
#if 1
				new_peak = _visible_peak_power[n] - _visible_peak_power[n] * Config->get_meter_falloff() * 0.01f * 0.05f;
#else
				new_peak = _visible_peak_power[n] - sqrt(_visible_peak_power[n] * Config->get_meter_falloff() * 0.01f * 0.0002f);
#endif
				if (new_peak < (1.0 / 512.0)) new_peak = 0;
			}
			_visible_peak_power[n] = new_peak;
			continue;
		}

		/* AUDIO */

		/* compute new visible value using falloff */

		_max_peak_signal[n] = std::max(new_peak, _max_peak_signal[n]);

		if (new_peak > 0.0) {
			new_peak = fast_coefficient_to_dB (new_peak);
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

float
PeakMeter::meter_level(uint32_t n, MeterType type) {
	switch (type) {
		case MeterKrms:
			{
				const uint32_t n_midi  = current_meters.n_midi();
				if ((n - n_midi) < _kmeter.size()) {
					return fast_coefficient_to_dB(_kmeter[n]->read());
				}
				return minus_infinity();
			}
		case MeterPeak:
			return peak_power(n);
		case MeterMaxSignal:
			if (n < _max_peak_signal.size()) {
				return _max_peak_signal[n];
			} else {
				return minus_infinity();
			}
		default:
		case MeterMaxPeak:
			if (n < _max_peak_power.size()) {
				return _max_peak_power[n];
			} else {
				return minus_infinity();
			}
	}
}
void
PeakMeter::set_type(MeterType t)
{
	if (t == _meter_type) {
		return;
	}

	_meter_type = t;

	if (t & MeterKrms) {
		const size_t n_audio = current_meters.n_audio();
		for (size_t n = 0; n < n_audio; ++n) {
			_kmeter[n]->reset();
		}
	}
	TypeChanged(t);
}

XMLNode&
PeakMeter::state (bool full_state)
{
	XMLNode& node (Processor::state (full_state));
	node.add_property("type", "meter");
	return node;
}


