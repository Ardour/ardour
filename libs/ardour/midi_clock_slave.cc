/*
    Copyright (C) 2008 Paul Davis
    Author: Hans Baier

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

#include <cmath>
#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <unistd.h>
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"
#include "pbd/convert.h"

#include "midi++/port.h"

#include "ardour/debug.h"
#include "ardour/slave.h"
#include "ardour/tempo.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace MIDI;
using namespace PBD;

MIDIClock_Slave::MIDIClock_Slave (Session& s, MIDI::Port& p, int ppqn)
	: ppqn (ppqn)
	, bandwidth (1.0 / 60.0) // 1 BpM = 1 / 60 Hz
{
	session = (ISlaveSessionProxy *) new SlaveSessionProxy(s);
	rebind (p);
	reset ();
}

MIDIClock_Slave::MIDIClock_Slave (ISlaveSessionProxy* session_proxy, int ppqn)
	: session(session_proxy)
	, ppqn (ppqn)
	, bandwidth (1.0 / 60.0) // 1 BpM = 1 / 60 Hz
{
	reset ();
}

MIDIClock_Slave::~MIDIClock_Slave()
{
	delete session;
}

void
MIDIClock_Slave::rebind (MIDI::Port& p)
{
	port_connections.drop_connections();

	port = &p;

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("MIDIClock_Slave: connecting to port %1\n", port->name()));

	port->parser()->timing.connect_same_thread (port_connections, boost::bind (&MIDIClock_Slave::update_midi_clock, this, _1, _2));
	port->parser()->start.connect_same_thread (port_connections, boost::bind (&MIDIClock_Slave::start, this, _1, _2));
	port->parser()->contineu.connect_same_thread (port_connections, boost::bind (&MIDIClock_Slave::contineu, this, _1, _2));
	port->parser()->stop.connect_same_thread (port_connections, boost::bind (&MIDIClock_Slave::stop, this, _1, _2));
	port->parser()->position.connect_same_thread (port_connections, boost::bind (&MIDIClock_Slave::position, this, _1, _2, 3));
}

void
MIDIClock_Slave::calculate_one_ppqn_in_frames_at(framepos_t time)
{
	const Tempo& current_tempo = session->tempo_map().tempo_at(time);
	double frames_per_beat = current_tempo.frames_per_beat(session->frame_rate());

	double quarter_notes_per_beat = 4.0 / current_tempo.note_type();
	double frames_per_quarter_note = frames_per_beat / quarter_notes_per_beat;

	one_ppqn_in_frames = frames_per_quarter_note / double (ppqn);
	// DEBUG_TRACE (DEBUG::MidiClock, string_compose ("at %1, one ppqn = %2\n", time, one_ppqn_in_frames));
}

ARDOUR::framepos_t
MIDIClock_Slave::calculate_song_position(uint16_t song_position_in_sixteenth_notes)
{
	framepos_t song_position_frames = 0;
	for (uint16_t i = 1; i <= song_position_in_sixteenth_notes; ++i) {
		// one quarter note contains ppqn pulses, so a sixteenth note is ppqn / 4 pulses
		calculate_one_ppqn_in_frames_at(song_position_frames);
		song_position_frames += one_ppqn_in_frames * (framepos_t)(ppqn / 4);
	}

	return song_position_frames;
}

void
MIDIClock_Slave::calculate_filter_coefficients()
{
	// omega = 2 * PI * Bandwidth / MIDI clock frame frequency in Hz
	omega = 2.0 * M_PI * bandwidth * one_ppqn_in_frames / session->frame_rate();
	b = 1.4142135623730950488 * omega;
	c = omega * omega;
}

void
MIDIClock_Slave::update_midi_clock (Parser& /*parser*/, framepos_t timestamp)
{
	// some pieces of hardware send MIDI Clock all the time
	if ( (!_starting) && (!_started) ) {
		return;
	}

	calculate_one_ppqn_in_frames_at(should_be_position);

	framepos_t elapsed_since_start = timestamp - first_timestamp;
	double error = 0;

	if (_starting || last_timestamp == 0) {
		midi_clock_count = 0;

		first_timestamp = timestamp;
		elapsed_since_start = should_be_position;

		// calculate filter coefficients
		calculate_filter_coefficients();

		// initialize DLL
		e2 = double(one_ppqn_in_frames) / double(session->frame_rate());
		t0 = double(elapsed_since_start) / double(session->frame_rate());
		t1 = t0 + e2;

		// let ardour go after first MIDI Clock Event
		_starting = false;
	} else {
		midi_clock_count++;
		should_be_position  += one_ppqn_in_frames;
		calculate_filter_coefficients();

		// calculate loop error
		// we use session->audible_frame() instead of t1 here
		// because t1 is used to calculate the transport speed,
		// so the loop will compensate for accumulating rounding errors
		error = (double(should_be_position) - double(session->audible_frame()));
		e = error / double(session->frame_rate());

		// update DLL
		t0 = t1;
		t1 += b * e + e2;
		e2 += c * e;
	}

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("clock #%1 @ %2 arrived %3 (theoretical) audible %4 transport %5 error %6 "
						       "read delta %7 should-be delta %8 t1-t0 %9 t0 %10 t1 %11 framerate %12 appspeed %13\n",
						       midi_clock_count,
						       elapsed_since_start,
						       should_be_position,
						       session->audible_frame(),
						       session->transport_frame(),
						       error,
						       timestamp - last_timestamp,
						       one_ppqn_in_frames,
						       (t1 -t0) * session->frame_rate(),
						       t0 * session->frame_rate(),
						       t1 * session->frame_rate(),
						       session->frame_rate(),
						       ((t1 - t0) * session->frame_rate()) / one_ppqn_in_frames));

	last_timestamp = timestamp;
}

void
MIDIClock_Slave::start (Parser& /*parser*/, framepos_t timestamp)
{
	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("MIDIClock_Slave got start message at time %1 engine time %2\n", timestamp, session->frame_time()));

	if (!_started) {
		reset();

		_started = true;
		_starting = true;

		should_be_position = session->transport_frame();
	}
}

void
MIDIClock_Slave::reset ()
{
	should_be_position = session->transport_frame();
	last_timestamp = 0;

	_starting = true;
	_started  = true;

	// session->request_locate(0, false);
	current_delta = 0;
}

void
MIDIClock_Slave::contineu (Parser& /*parser*/, framepos_t /*timestamp*/)
{
	DEBUG_TRACE (DEBUG::MidiClock, "MIDIClock_Slave got continue message\n");

	if (!_started) {
		_starting = true;
		_started  = true;
	}
}


void
MIDIClock_Slave::stop (Parser& /*parser*/, framepos_t /*timestamp*/)
{
	DEBUG_TRACE (DEBUG::MidiClock, "MIDIClock_Slave got stop message\n");

	if (_started || _starting) {
		_starting = false;
		_started  = false;
		// locate to last MIDI clock position
		session->request_transport_speed(0.0);

		// we need to go back to the last MIDI beat (6 ppqn)
		// and lets hope the tempo didnt change in the meantime :)

		// begin at the should be position, because
		// that is the position of the last MIDI Clock
		// message and that is probably what the master
		// expects where we are right now
		framepos_t stop_position = should_be_position;

		// find out the last MIDI beat: go back #midi_clocks mod 6
		// and lets hope the tempo didnt change in those last 6 beats :)
		stop_position -= (midi_clock_count % 6) * one_ppqn_in_frames;

		session->request_locate(stop_position, false);
		should_be_position = stop_position;
		last_timestamp = 0;
	}
}

void
MIDIClock_Slave::position (Parser& /*parser*/, byte* message, size_t size)
{
	// we are note supposed to get position messages while we are running
	// so lets be robust and ignore those
	if (_started || _starting) {
		return;
	}

	assert(size == 3);
	byte lsb = message[1];
	byte msb = message[2];
	assert((lsb <= 0x7f) && (msb <= 0x7f));

	uint16_t position_in_sixteenth_notes = (uint16_t(msb) << 7) | uint16_t(lsb);
	framepos_t position_in_frames = calculate_song_position(position_in_sixteenth_notes);

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Song Position: %1 frames: %2\n", position_in_sixteenth_notes, position_in_frames));

	session->request_locate(position_in_frames, false);
	should_be_position  = position_in_frames;
	last_timestamp = 0;

}

bool
MIDIClock_Slave::locked () const
{
	return true;
}

bool
MIDIClock_Slave::ok() const
{
	return true;
}

bool
MIDIClock_Slave::starting() const
{
	return false;
}

bool
MIDIClock_Slave::stop_if_no_more_clock_events(framepos_t& pos, framepos_t now)
{
	/* no timecode for 1/4 second ? conclude that its stopped */
	if (last_timestamp &&
	    now > last_timestamp &&
	    now - last_timestamp > session->frame_rate() / 4) {
		DEBUG_TRACE (DEBUG::MidiClock, "No MIDI Clock frames received for some time, stopping!\n");
		pos = should_be_position;
		session->request_transport_speed (0);
		session->request_locate (should_be_position, false);
		return true;
	} else {
		return false;
	}
}

bool
MIDIClock_Slave::speed_and_position (double& speed, framepos_t& pos)
{
	if (!_started || _starting) {
		speed = 0.0;
		pos   = should_be_position;
		return true;
	}

	framepos_t engine_now = session->frame_time();

	if (stop_if_no_more_clock_events(pos, engine_now)) {
		return false;
	}

	// calculate speed
	speed = ((t1 - t0) * session->frame_rate()) / one_ppqn_in_frames;

	// provide a 3% deadzone to lock the speed
	if (fabs(speed - 1.0) <= 0.03)
	        speed = 1.0;

	// calculate position
	if (engine_now > last_timestamp) {
		// we are in between MIDI clock messages
		// so we interpolate position according to speed
		framecnt_t elapsed = engine_now - last_timestamp;
		pos = (framepos_t) (should_be_position + double(elapsed) * speed);
	} else {
		// A new MIDI clock message has arrived this cycle
		pos = should_be_position;
	}

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("speed_and_position: %1 & %2 <-> %3 (transport)\n", speed, pos, session->transport_frame()));
	current_delta = pos - session->transport_frame();

	return true;
}

ARDOUR::framecnt_t
MIDIClock_Slave::resolution() const
{
	// one beat
	return (framecnt_t) one_ppqn_in_frames * ppqn;
}

std::string
MIDIClock_Slave::approximate_current_delta() const
{
	char delta[24];
	if (last_timestamp == 0 || _starting) {
		snprintf(delta, sizeof(delta), "\u2012\u2012\u2012\u2012");
	} else {
		snprintf(delta, sizeof(delta), "\u0394 %s%4" PRIi64 " sm",
				PLUSMINUS(-current_delta), abs(current_delta));
	}
	return std::string(delta);
}

