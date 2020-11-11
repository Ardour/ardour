/*
 * Copyright (C) 2008-2013 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2013 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2018 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2016 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include <cmath>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"
#include "pbd/convert.h"

#include "midi++/port.h"

#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_port.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace MIDI;
using namespace PBD;

#define ENGINE AudioEngine::instance()

MIDIClock_TransportMaster::MIDIClock_TransportMaster (std::string const & name, int ppqn)
	: TransportMaster (MIDIClock, name)
	, ppqn (ppqn)
	, midi_clock_count (0)
	, _running (false)
	, _bpm (0)
{
}

MIDIClock_TransportMaster::~MIDIClock_TransportMaster()
{
	port_connections.drop_connections ();
}

void
MIDIClock_TransportMaster::init ()
{
	midi_clock_count = 0;
	current.reset ();
	resync_latency (false);
}

void
MIDIClock_TransportMaster::connection_handler (boost::weak_ptr<ARDOUR::Port> w0, std::string n0, boost::weak_ptr<ARDOUR::Port> w1, std::string n1, bool con) 
{
	TransportMaster::connection_handler(w0, n0, w1, n1, con);

	boost::shared_ptr<Port> p = w1.lock ();
	if (p == _port) {
		resync_latency (false);
	}
}

void
MIDIClock_TransportMaster::create_port ()
{
	if ((_port = create_midi_port (string_compose ("%1 in", _name))) == 0) {
		throw failed_constructor();
	}
}

void
MIDIClock_TransportMaster::set_session (Session* s)
{
	TransportMaster::set_session (s);
	TransportMasterViaMIDI::set_session (s);

	port_connections.drop_connections();

	/* only connect to signals if we have a proxy, because otherwise we
	 * cannot interpet incoming data (no tempo map etc.)
	 */

	if (_session) {
		parser.timing.connect_same_thread (port_connections, boost::bind (&MIDIClock_TransportMaster::update_midi_clock, this, _1, _2));
		parser.start.connect_same_thread (port_connections, boost::bind (&MIDIClock_TransportMaster::start, this, _1, _2));
		parser.contineu.connect_same_thread (port_connections, boost::bind (&MIDIClock_TransportMaster::contineu, this, _1, _2));
		parser.stop.connect_same_thread (port_connections, boost::bind (&MIDIClock_TransportMaster::stop, this, _1, _2));
		parser.position.connect_same_thread (port_connections, boost::bind (&MIDIClock_TransportMaster::position, this, _1, _2, _3, _4));

		reset (true);
	}
}

void
MIDIClock_TransportMaster::pre_process (MIDI::pframes_t nframes, samplepos_t now, boost::optional<samplepos_t> session_pos)
{
	/* Read and parse incoming MIDI */
	if (!_midi_port) {
		_bpm = 0.0;
		_running = false;
		_current_delta = 0;
		midi_clock_count = 0;
		DEBUG_TRACE (DEBUG::MidiClock, "No MIDI Clock port registered");
		return;
	}

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("preprocess with lt = %1 @ %2, running ? %3\n", current.timestamp, now, _running));

	_midi_port->read_and_parse_entire_midi_buffer_with_no_speed_adjustment (nframes, parser, now);

	/* no clock messages ever, or no clock messages for 1/4 second ? conclude that its stopped */

	if (!current.timestamp || one_ppqn_in_samples == 0 || (now > current.timestamp && ((now - current.timestamp) > (ENGINE->sample_rate() / 4)))) {
		_bpm = 0.0;
		_running = false;
		_current_delta = 0;
		midi_clock_count = 0;

		DEBUG_TRACE (DEBUG::MidiClock, string_compose ("No MIDI Clock messages received for some time, stopping! ts = %1 @ %2 ppqn = %3\n", current.timestamp, now, one_ppqn_in_samples));
		return;
	}

	if (session_pos) {
		const samplepos_t current_pos = current.position + ((now - current.timestamp) * current.speed);
		_current_delta = current_pos - *session_pos;
	} else {
		_current_delta = 0;
	}

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("speed_and_position: speed %1 should-be %2 transport %3 \n", current.speed, current.position, _session->transport_sample()));
}

void
MIDIClock_TransportMaster::calculate_one_ppqn_in_samples_at(samplepos_t time)
{
	const Temporal::TempoMetric& metric = _session->tempo_map().metric_at (time);
	const double samples_per_quarter_note = metric.tempo().samples_per_quarter_note (ENGINE->sample_rate());

	one_ppqn_in_samples = samples_per_quarter_note / double (ppqn);
	// DEBUG_TRACE (DEBUG::MidiClock, string_compose ("at %1, one ppqn = %2 [spl] spqn = %3, ppqn = %4\n", time, one_ppqn_in_samples, samples_per_quarter_note, ppqn));
}

ARDOUR::samplepos_t
MIDIClock_TransportMaster::calculate_song_position(uint16_t song_position_in_sixteenth_notes)
{
	samplepos_t song_position_samples = 0;
	for (uint16_t i = 1; i <= song_position_in_sixteenth_notes; ++i) {
		// one quarter note contains ppqn pulses, so a sixteenth note is ppqn / 4 pulses
		calculate_one_ppqn_in_samples_at(song_position_samples);
		song_position_samples += one_ppqn_in_samples * (samplepos_t)(ppqn / 4);
	}

	return song_position_samples;
}

void
MIDIClock_TransportMaster::calculate_filter_coefficients (double qpm)
{
	/* Paul says: I don't understand this computation of bandwidth
	*/

	const double bandwidth = 2.0 / qpm;

	/* Frequency of the clock messages is ENGINE->sample_rate() / * one_ppqn_in_samples, per second or in Hz */
	const double freq = (double) ENGINE->sample_rate() / one_ppqn_in_samples;

	const double omega = 2.0 * M_PI * bandwidth / freq;
	b = 1.4142135623730950488 * omega; // sqrt (2.0) * omega
	c = omega * omega;

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("DLL coefficients: bw:%1 omega:%2 b:%3 c:%4\n", bandwidth, omega, b, c));
}

void
MIDIClock_TransportMaster::update_midi_clock (Parser& /*parser*/, samplepos_t timestamp)
{
	samplepos_t elapsed_since_start = timestamp - first_timestamp;

	calculate_one_ppqn_in_samples_at (current.position);

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("clock count %1, sbp %2\n", midi_clock_count, current.position));

	if (midi_clock_count == 0) {
		/* second 0xf8 message after start/reset has arrived */

		first_timestamp = timestamp;
		current.update (0, timestamp, 0);

		DEBUG_TRACE (DEBUG::MidiClock, string_compose ("first clock message after start received @ %1\n", timestamp));

		midi_clock_count++;

	} else if (midi_clock_count == 1) {

		/* second 0xf8 message has arrived. we can now estimate QPM
		 * (quarters per minute, and fully initialize the DLL
		 */

		e2 = timestamp - current.timestamp;

		const samplecnt_t samples_per_quarter = e2 * 24;
		double bpm = (ENGINE->sample_rate() * 60.0) / samples_per_quarter;

		if (bpm < 1 || bpm > 999) {
			current.update (0, timestamp, 0);
			midi_clock_count = 1; /* start over */
			DEBUG_TRACE (DEBUG::MidiClock, string_compose ("BPM is out of bounds (%1)\n", timestamp, current.timestamp));
		} else {
			_bpm = bpm;

			calculate_filter_coefficients (_bpm);

			/* finish DLL initialization */

			t0 = timestamp;
			t1 = t0 + e2; /* timestamp we predict for the next 0xf8 clock message */

			midi_clock_count++;
			current.update (one_ppqn_in_samples + midi_port_latency.max, timestamp, 0);
		}

	} else {

		/* 3rd or later MIDI clock message. We can now compute actual
		 * speed (and tempo) with the DLL
		 */

		double e = timestamp - t1; // error between actual time of arrival of clock message and our predicted time
		t0 = t1;
		t1 += b * e + e2;
		e2 += c * e;

		const double samples_per_quarter = (timestamp - current.timestamp) * 24.0;
		const double instantaneous_bpm = (ENGINE->sample_rate() * 60.0) / samples_per_quarter;

		const double predicted_clock_interval_in_samples = (t1 - t0);

		/* _speed is relative to session tempo map */

		double speed = predicted_clock_interval_in_samples / one_ppqn_in_samples;

		/* _bpm (really, _qpm) is absolute */

		/* detect substantial changes in apparent tempo (defined as a
		 * change of more than 20% of the current tempo.
		 */

		const double lpf_coeff = 0.063;

		if (fabs (instantaneous_bpm - _bpm) > (0.20 * _bpm)) {
			_bpm = instantaneous_bpm;
		} else {
			_bpm += lpf_coeff * (instantaneous_bpm - _bpm);
		}

		calculate_filter_coefficients (_bpm);

		// need at least two clock events to compute speed

		if (!_running) {
			DEBUG_TRACE (DEBUG::MidiClock, string_compose ("start mclock running with speed = %1\n", (t1 - t0) / one_ppqn_in_samples));
			_running = true;
		}

		midi_clock_count++;
		current.update (current.position + one_ppqn_in_samples, timestamp, speed);

		if (TransportMasterManager::instance().current().get() == this) {
			_session->maybe_update_tempo_from_midiclock_tempo (_bpm);
		}
	}

	DEBUG_TRACE (DEBUG::MidiClock, string_compose (
	             "clock #%1 @ %2 should-be %3 transport %4 appspeed %6 "
	             "read-delta %7 should-be-delta %8 t1-t0 %9 t0 %10 t1 %11 sample-rate %12 engine %13 running %14\n",
	             midi_clock_count,                 // #
	             elapsed_since_start,              // @
	             current.position,                 // should-be
	             _session->transport_sample(),     // transport
	             (t1 - t0) / one_ppqn_in_samples,  // appspeed
	             timestamp - current.timestamp,    // read delta
	             one_ppqn_in_samples,              // should-be delta
	             (t1 - t0),                        // t1-t0
	             t0,                               // t0 (current position)
	             t1,                               // t1 (expected next pos)
	             ENGINE->sample_rate(),            // framerate
	             ENGINE->sample_time(),
	             _running

	));
}

void
MIDIClock_TransportMaster::start (Parser& /*parser*/, samplepos_t timestamp)
{
	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("MIDIClock_TransportMaster got start message at time %1 engine time %2 transport_sample %3\n", timestamp, ENGINE->sample_time(), _session->transport_sample()));

	if (!_running) {
		reset(true);
		_running = true;
		current.update (_session->transport_sample(), timestamp, 0);
	}
}

void
MIDIClock_TransportMaster::reset (bool with_position)
{
	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("MidiClock Master reset(): calculated filter for period size %2\n", ENGINE->samples_per_cycle()));

	if (with_position) {
		current.update (_session->transport_sample(), 0, 0);
	} else {
		current.reset ();
	}

	_running = false;
	_current_delta = 0;
}

void
MIDIClock_TransportMaster::contineu (Parser& /*parser*/, samplepos_t /*timestamp*/)
{
	DEBUG_TRACE (DEBUG::MidiClock, "MIDIClock_TransportMaster got continue message\n");

	_running = true;
}

void
MIDIClock_TransportMaster::stop (Parser& /*parser*/, samplepos_t timestamp)
{
	DEBUG_TRACE (DEBUG::MidiClock, "MIDIClock_TransportMaster got stop message\n");

	if (_running) {
		_running = false;

		// we need to go back to the last MIDI beat (6 ppqn)
		// and lets hope the tempo didnt change in the meantime :)

		// begin at the should be position, because
		// that is the position of the last MIDI Clock
		// message and that is probably what the master
		// expects where we are right now
		//
		// find out the last MIDI beat: go back #midi_clocks mod 6
		// and lets hope the tempo didnt change in those last 6 beats :)
		current.update (current.position - (midi_clock_count % 6) * one_ppqn_in_samples, 0, 0);
	}
}

void
MIDIClock_TransportMaster::position (Parser& /*parser*/, MIDI::byte* message, size_t size, samplepos_t timestamp)
{
	// we are not supposed to get position messages while we are running
	// so lets be robust and ignore those
	if (_running) {
		return;
	}

	assert(size == 3);
	MIDI::byte lsb = message[1];
	MIDI::byte msb = message[2];
	assert((lsb <= 0x7f) && (msb <= 0x7f));

	uint16_t position_in_sixteenth_notes = (uint16_t(msb) << 7) | uint16_t(lsb);
	samplepos_t position_in_samples = calculate_song_position(position_in_sixteenth_notes);

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Song Position: %1 samples: %2\n", position_in_sixteenth_notes, position_in_samples));

	current.update (position_in_samples + midi_port_latency.max, timestamp, current.speed);
}

bool
MIDIClock_TransportMaster::locked () const
{
	return true;
}

bool
MIDIClock_TransportMaster::ok() const
{
	return true;
}

ARDOUR::samplecnt_t
MIDIClock_TransportMaster::update_interval() const
{
	if (one_ppqn_in_samples) {
		return resolution ();
	}

	return AudioEngine::instance()->sample_rate() / 120 / 4; /* pure guesswork */
}

ARDOUR::samplecnt_t
MIDIClock_TransportMaster::resolution() const
{
	// one beat
	return (samplecnt_t) one_ppqn_in_samples * ppqn;
}

std::string
MIDIClock_TransportMaster::position_string () const
{
	return std::string();
}

std::string
MIDIClock_TransportMaster::delta_string() const
{
	SafeTime last;
	current.safe_read (last);

	if (last.timestamp == 0 || starting()) {
		return X_("\u2012\u2012\u2012\u2012");
	} else {
		return format_delta_time (_current_delta);
	}
}

void
MIDIClock_TransportMaster::unregister_port ()
{
	_midi_port.reset ();
	TransportMaster::unregister_port ();
}

