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
#include <midi++/jack.h>
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
	, accumulator_index (0)
	, average (0.0)
{
	rebind (p);
	reset ();

	for(int i = 0; i < accumulator_size; i++)
		accumulator[i]=0.0;
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
MIDIClock_Slave::update_midi_clock (Parser& parser, nframes_t timestamp)
{	
	nframes_t now = timestamp;

	SafeTime last;
	read_current (&last);
		
	const Tempo& current_tempo = session.tempo_map().tempo_at(now);
	const Meter& current_meter = session.tempo_map().meter_at(now);
	double frames_per_beat =
		current_tempo.frames_per_beat(session.frame_rate(),
		                              current_meter);

	double quarter_notes_per_beat = 4.0 / current_tempo.note_type();
	double frames_per_quarter_note = frames_per_beat / quarter_notes_per_beat;

	one_ppqn_in_frames = frames_per_quarter_note / ppqn;
	
	// for the first MIDI clock event we dont have any past
	// data, so we assume a sane tempo
	if(last.timestamp == 0) {
		midi_clock_frame = one_ppqn_in_frames;
	} else {
		midi_clock_frame = now - last.timestamp;
	}
	
	
	// moving average over incoming intervals
	accumulator[accumulator_index++] = (float) midi_clock_frame;
	if(accumulator_index == accumulator_size)
		accumulator_index = 0;
	
	average = 0.0;
	for(int i = 0; i < accumulator_size; i++)
		average += accumulator[i];
	average /= accumulator_size;
	
	
#if 1
	JACK_MidiPort *jack_port = dynamic_cast<JACK_MidiPort *>(port);
	pthread_t process_thread_id = 0;
	if(jack_port) {
		process_thread_id = jack_port->get_process_thread();
	}
	
	std::cerr 
	          << " got MIDI Clock message at time " << now  
	          << " session time: " << session.engine().frame_time() 
	          << " real delta: " << midi_clock_frame 
	          << " reference: " << one_ppqn_in_frames
	          << " average: " << average
	          << std::endl;
#endif
	
	current.guard1++;
	current.position += midi_clock_frame;
	current.timestamp = now;
	current.guard2++;

	last_inbound_frame = now;
}

void
MIDIClock_Slave::start (Parser& parser, nframes_t timestamp)
{
	
	nframes_t now = timestamp;
	cerr << "MIDIClock_Slave got start message at time "  <<  now 
	     << " session time: " << session.engine().frame_time() << endl;

	if(!locked()) {
		cerr << "Did not start because not locked!" << endl;
		return;
	}
	
	midi_clock_frame = 0;
	
	session.request_transport_speed(one_ppqn_in_frames / average);
	current.guard1++;
	current.position = 0;
	current.timestamp = now;
	current.guard2++;
	
	_started = true;
}

void
MIDIClock_Slave::stop (Parser& parser, nframes_t timestamp)
{
	std::cerr << "MIDIClock_Slave got stop message" << endl;

	midi_clock_frame = 0;

	current.guard1++;
	current.position = 0;
	current.timestamp = 0;
	current.guard2++;
	
	_started = false;
	reset();
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
	
	if(!_started) {
		speed = 0.0;
		pos = 0;
		return true;
	}
	
	nframes_t now = session.engine().frame_time();
	nframes_t frame_rate = session.frame_rate();

	SafeTime last;
	read_current (&last);

	/* no timecode for 1/4 second ? conclude that its stopped */

	if (last_inbound_frame && now > last_inbound_frame && now - last_inbound_frame > frame_rate / 4) {
		cerr << "No MIDI Clock frames received for some time, stopping!" << endl;
		pos = last.position;
		session.request_locate (pos, false);
		session.request_transport_speed (0);
		this->stop(*port->input(), now);
		reset();
		return false;
	}

	cerr << " now: " << now << " last: " << last.timestamp;

	speed = one_ppqn_in_frames / average;
	cerr << " final speed: " << speed;
	
	if (now > last.timestamp) {
		// we are in between MIDI clock messages
		// so we interpolate position according to speed
		nframes_t elapsed = now - last.timestamp;
		cerr << " elapsed: " << elapsed << " elapsed (scaled)  " << elapsed * speed;
		pos = last.position + elapsed * speed;
		last.position = pos;
	} else {
		// A new MIDI clock message has arrived this cycle		
		pos = last.position;
	}
	
   cerr << " position: " << pos; 
   cerr << endl;
	
	return true;
}

ARDOUR::nframes_t
MIDIClock_Slave::resolution() const
{
	return (nframes_t) one_ppqn_in_frames * 24;
}

void
MIDIClock_Slave::reset ()
{

	last_inbound_frame = 0;
	current.guard1++;
	current.position = 0;
	current.timestamp = 0;
	current.guard2++;
	
	session.request_locate(0, false);
}
