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
	, average_midi_clock_frame_duration (0.0)
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

#ifdef DEBUG_MIDI_CLOCK		
	std::cerr << "MIDIClock_Slave: connecting to port " << port->name() << std::endl;
#endif

	connections.push_back (port->input()->timing.connect   (mem_fun (*this, &MIDIClock_Slave::update_midi_clock)));
	connections.push_back (port->input()->start.connect    (mem_fun (*this, &MIDIClock_Slave::start)));
	connections.push_back (port->input()->contineu.connect (mem_fun (*this, &MIDIClock_Slave::contineu)));
	connections.push_back (port->input()->stop.connect     (mem_fun (*this, &MIDIClock_Slave::stop)));
}

void 
MIDIClock_Slave::calculate_one_ppqn_in_frames_at(nframes_t time)
{
	const Tempo& current_tempo = session.tempo_map().tempo_at(time);
	const Meter& current_meter = session.tempo_map().meter_at(time);
	double frames_per_beat =
		current_tempo.frames_per_beat(session.nominal_frame_rate(),
		                              current_meter);

	double quarter_notes_per_beat = 4.0 / current_tempo.note_type();
	double frames_per_quarter_note = frames_per_beat / quarter_notes_per_beat;

	one_ppqn_in_frames = frames_per_quarter_note / double (ppqn);
}

void
MIDIClock_Slave::update_midi_clock (Parser& parser, nframes_t timestamp)
{	
	nframes_t now = timestamp;

	SafeTime last;
	read_current (&last);
	
	if (_starting) {
		assert(last.timestamp == 0);
		// let ardour go after first MIDI Clock Event
		_starting = false;
	}
		
	calculate_one_ppqn_in_frames_at(now);
	
	// for the first MIDI clock event we don't have any past
	// data, so we assume a sane tempo
	if(last.timestamp == 0) {
		current_midi_clock_frame_duration = one_ppqn_in_frames;
	} else {
		current_midi_clock_frame_duration = now - last.timestamp;
	}
		
	// moving average over incoming intervals
	accumulator[accumulator_index++] = current_midi_clock_frame_duration;
	if(accumulator_index == accumulator_size)
		accumulator_index = 0;
	
	average_midi_clock_frame_duration = 0.0;
	for(int i = 0; i < accumulator_size; i++)
		average_midi_clock_frame_duration += accumulator[i];
	average_midi_clock_frame_duration /= double(accumulator_size);
	
#ifdef DEBUG_MIDI_CLOCK		
#ifdef WITH_JACK_MIDI
	JACK_MidiPort* jack_port = dynamic_cast<JACK_MidiPort*>(port);
	pthread_t process_thread_id = 0;
	if(jack_port) {
		process_thread_id = jack_port->get_process_thread();
	}
	
	std::cerr 
	          << " got MIDI Clock message at time " << now  
	          << " session time: " << session.engine().frame_time() 
	          << " transport position: " << session.transport_frame()
	          << " real delta: " << current_midi_clock_frame_duration 
	          << " reference: " << one_ppqn_in_frames
	          << " average: " << average_midi_clock_frame_duration
	          << std::endl;
#endif // DEBUG_MIDI_CLOCK
#endif // WITH_JACK_MIDI
	
	current.guard1++;
	current.position += one_ppqn_in_frames;
	current_position += one_ppqn_in_frames;
	current.timestamp = now;
	current.guard2++;

	last_inbound_frame = now;
}

void
MIDIClock_Slave::start (Parser& parser, nframes_t timestamp)
{
	
	nframes_t now = timestamp;
	
#ifdef DEBUG_MIDI_CLOCK	
	cerr << "MIDIClock_Slave got start message at time "  <<  now << " session time: " << session.engine().frame_time() << endl;
#endif
	
	if(!locked()) {
		cerr << "Did not start because not locked!" << endl;
		return;
	}
	
	current_midi_clock_frame_duration = 0;
	
	current.guard1++;
	current.position = 0;
	current_position = 0;	
	current.timestamp = 0;
	current.guard2++;
	
	_started = true;
	_starting = true;
}

void
MIDIClock_Slave::contineu (Parser& parser, nframes_t timestamp)
{
#ifdef DEBUG_MIDI_CLOCK	
	std::cerr << "MIDIClock_Slave got continue message" << endl;
#endif
	start(parser, timestamp);
}


void
MIDIClock_Slave::stop (Parser& parser, nframes_t timestamp)
{
#ifdef DEBUG_MIDI_CLOCK	
	std::cerr << "MIDIClock_Slave got stop message" << endl;
#endif
	
	current_midi_clock_frame_duration = 0;

	current.guard1++;
	current.position = 0;
	current_position = 0;	
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
MIDIClock_Slave::starting() const
{
	return _starting;
}

bool
MIDIClock_Slave::stop_if_no_more_clock_events(nframes_t& pos, nframes_t now, SafeTime& last)
{
	/* no timecode for 1/4 second ? conclude that its stopped */
	if (last_inbound_frame && 
	    now > last_inbound_frame && 
	    now - last_inbound_frame > session.frame_rate() / 4) {
#ifdef DEBUG_MIDI_CLOCK			
		cerr << "No MIDI Clock frames received for some time, stopping!" << endl;
#endif		
		pos = last.position;
		session.request_locate (pos, false);
		session.request_transport_speed (0);
		this->stop(*port->input(), now);
		reset();
		return true;
	} else {
		return false;
	}
}

bool
MIDIClock_Slave::speed_and_position (float& speed, nframes_t& pos)
{
	if (!_started) {
		speed = 0.0;
		pos = 0;
		return true;
	}
		
	nframes_t now = session.engine().frame_time();
	SafeTime last;
	read_current (&last);

	if (stop_if_no_more_clock_events(pos, now, last)) {
		return false;
	}
	//cerr << " now: " << now << " last: " << last.timestamp;

	// calculate speed
	double speed_double = one_ppqn_in_frames / average_midi_clock_frame_duration;
	speed = float(speed_double);
	//cerr << " final speed: " << speed;
	
	// calculate position
	if (now > last.timestamp) {
		// we are in between MIDI clock messages
		// so we interpolate position according to speed
		nframes_t elapsed = now - last.timestamp;
		pos = nframes_t (current_position + double(elapsed) * speed_double);
	} else {
		// A new MIDI clock message has arrived this cycle
		pos = current_position;
	}
	
	/*
   cerr << " transport position now: " <<  session.transport_frame(); 
   cerr << " calculated position: " << pos; 
   cerr << endl;
   */
   
	return true;
}

ARDOUR::nframes_t
MIDIClock_Slave::resolution() const
{
	// one beat
	return (nframes_t) one_ppqn_in_frames * ppqn;
}

void
MIDIClock_Slave::reset ()
{

	last_inbound_frame = 0;
	current.guard1++;
	current.position = 0;
	current_position = 0;		
	current.timestamp = 0;
	current.guard2++;
	
	session.request_locate(0, false);
}
