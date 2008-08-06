/*
    Copyright (C) 2002-4 Paul Davis

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

#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <unistd.h>
#include <pbd/error.h>
#include <pbd/failed_constructor.h>
#include <pbd/pthread_utils.h>

#include <midi++/port.h>
#include <ardour/slave.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/cycles.h>
#include <ardour/tempo.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace sigc;
using namespace MIDI;
using namespace PBD;

MIDIClock_Slave::MIDIClock_Slave (Session& s, MIDI::Port& p, int ppqn)
	: session (s)
	, ppqn (ppqn)
{
	rebind (p);
	reset ();
}

MIDIClock_Slave::~MIDIClock_Slave()
{
}

void
MIDIClock_Slave::rebind (MIDI::Port& p)
{
	for (vector<sigc::connection>::iterator i = connections.begin(); i != connections.end(); ++i) {
		(*i).disconnect ();
	}

	port = &p;

	std::cerr << "MIDIClock_Slave: connecting to port " << port->name() << std::endl;

	connections.push_back (port->input()->timing.connect (mem_fun (*this, &MIDIClock_Slave::update_midi_clock)));
	connections.push_back (port->input()->start.connect  (mem_fun (*this, &MIDIClock_Slave::start)));
	connections.push_back (port->input()->stop.connect   (mem_fun (*this, &MIDIClock_Slave::stop)));
}

void
MIDIClock_Slave::update_midi_clock (Parser& parser)
{
	// ignore clock events if no start event received
	if(!_started)
		return;

	nframes_t now = session.engine().frame_time();

	SafeTime last;
	read_current (&last);

	const Tempo& current_tempo = session.tempo_map().tempo_at(now);
	const Meter& current_meter = session.tempo_map().meter_at(now);
	double frames_per_beat =
		current_tempo.frames_per_beat(session.nominal_frame_rate(),
		                              current_meter);

	double quarter_notes_per_beat = 4.0 / current_tempo.note_type();
	double frames_per_quarter_note = frames_per_beat / quarter_notes_per_beat;

	one_ppqn_in_frames = frames_per_quarter_note / ppqn;

	/*
	   Also compensate for audio latency.
	*/

	midi_clock_frame += (long) (one_ppqn_in_frames)
	                    + session.worst_output_latency();

	/*
	std::cerr << "got MIDI Clock message at time " << now  
	          << " result: " << midi_clock_frame 
	          << " open_ppqn_in_frames: " << one_ppqn_in_frames << std::endl;
	 */
	
	if (first_midi_clock_frame == 0) {
		first_midi_clock_frame = midi_clock_frame;
		first_midi_clock_time = now;
	}

	current.guard1++;
	current.position = midi_clock_frame;
	current.timestamp = now;
	current.guard2++;

	last_inbound_frame = now;
}

void
MIDIClock_Slave::start (Parser& parser)
{
	std::cerr << "MIDIClock_Slave got start message" << endl;

	midi_clock_speed = 1.0f;
	_started = true;
}

void
MIDIClock_Slave::stop (Parser& parser)
{
	std::cerr << "MIDIClock_Slave got stop message" << endl;

	midi_clock_speed = 0.0f;
	midi_clock_frame = 0;
	_started = false;

	current.guard1++;
	current.position = midi_clock_frame;
	current.timestamp = 0;
	current.guard2++;
}

void
MIDIClock_Slave::read_current (SafeTime *st) const
{
	int tries = 0;
	do {
		if (tries == 10) {
			error << _("MIDI Clock Slave: atomic read of current time failed, sleeping!") << endmsg;
			usleep (20);
			tries = 0;
		}

		*st = current;
		tries++;

	} while (st->guard1 != st->guard2);
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
MIDIClock_Slave::speed_and_position (float& speed, nframes_t& pos)
{
	//std::cerr << "MIDIClock_Slave speed and position() called" << endl;
	nframes_t now = session.engine().frame_time();
	nframes_t frame_rate = session.frame_rate();
	nframes_t elapsed;
	float speed_now;

	SafeTime last;
	read_current (&last);

	if (first_midi_clock_time == 0) {
		speed = 0;
		pos = last.position;
		return true;
	}

	/* no timecode for 1/4 second ? conclude that its stopped */

	if (last_inbound_frame && now > last_inbound_frame && now - last_inbound_frame > frame_rate / 4) {
		midi_clock_speed = 0;
		pos = last.position;
		session.request_locate (pos, false);
		session.request_transport_speed (0);
		this->stop(*port->input());
		reset();
		return false;
	}

	speed_now = (float) ((last.position - first_midi_clock_frame) / (double) (now - first_midi_clock_time));

	cerr << "speed_and_position: speed_now: " << speed_now ;
	
	midi_clock_speed = speed_now;

	if (midi_clock_speed == 0.0f) {

		elapsed = 0;

	} else {

		/* scale elapsed time by the current MIDI Clock speed */

		if (last.timestamp && (now > last.timestamp)) {
			elapsed = (nframes_t) floor (midi_clock_speed * (now - last.timestamp));
		} else {
			elapsed = 0; /* XXX is this right? */
		}
	}

	/* now add the most recent timecode value plus the estimated elapsed interval */

	pos =  elapsed + last.position;

	speed = midi_clock_speed;
	
	cerr << " final speed: " << speed << " position: " << pos << endl;
	return true;
}

ARDOUR::nframes_t
MIDIClock_Slave::resolution() const
{
	return (nframes_t) one_ppqn_in_frames;
}

void
MIDIClock_Slave::reset ()
{
	/* XXX massive thread safety issue here. MTC could
	   be being updated as we call this. but this
	   supposed to be a realtime-safe call.
	*/

	last_inbound_frame = 0;
	current.guard1++;
	current.position = 0;
	current.timestamp = 0;
	current.guard2++;
	first_midi_clock_frame = 0;
	first_midi_clock_time = 0;

	midi_clock_speed = 0;
}
