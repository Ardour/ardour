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
		current_tempo.frames_per_beat(session.frame_rate(),
		                              current_meter);

	double quarter_notes_per_beat = 4.0 / current_tempo.note_type();
	double frames_per_quarter_note = frames_per_beat / quarter_notes_per_beat;

	one_ppqn_in_frames = frames_per_quarter_note / double (ppqn);
}

void
MIDIClock_Slave::update_midi_clock (Parser& parser, nframes_t timestamp)
{			
	calculate_one_ppqn_in_frames_at(last_position);
	
	// for the first MIDI clock event we don't have any past
	// data, so we assume a sane tempo
	if(_starting) {
		current_midi_clock_frame_duration = one_ppqn_in_frames;
	} else {
		current_midi_clock_frame_duration = timestamp - last_timestamp;
	}
		
	// moving average over incoming intervals
	accumulator[accumulator_index++] = current_midi_clock_frame_duration;
	if(accumulator_index == accumulator_size) {
		accumulator_index = 0;
	}
	average_midi_clock_frame_duration = 0.0;
	for(int i = 0; i < accumulator_size; i++) {
		average_midi_clock_frame_duration += accumulator[i];
	}
	average_midi_clock_frame_duration /= double(accumulator_size);
	
	#ifdef DEBUG_MIDI_CLOCK		
		std::cerr 
				  << " got MIDI Clock message at time " << timestamp  
				  << " engine time: " << session.engine().frame_time() 
				  << " transport position: " << session.transport_frame()
				  << " real delta: " << current_midi_clock_frame_duration 
				  << " reference: " << one_ppqn_in_frames
				  << " average: " << average_midi_clock_frame_duration
				  << std::endl;
	#endif // DEBUG_MIDI_CLOCK
	
	if (_starting) {
		assert(last_timestamp == 0);
		assert(last_position == 0);
		
		last_position = 0;
		last_timestamp = timestamp;
		
		// let ardour go after first MIDI Clock Event
		_starting = false;
		session.request_transport_speed (1.0);
	} else {;
		last_position  += double(one_ppqn_in_frames);
		last_timestamp = timestamp;
	}

}

void
MIDIClock_Slave::start (Parser& parser, nframes_t timestamp)
{	
	#ifdef DEBUG_MIDI_CLOCK	
		cerr << "MIDIClock_Slave got start message at time "  <<  timestamp << " session time: " << session.engine().frame_time() << endl;
	#endif
	
	if(!locked()) {
		cerr << "Did not start because not locked!" << endl;
		return;
	}
	
	// initialize accumulator to sane values
	calculate_one_ppqn_in_frames_at(0);
	
	for(int i = 0; i < accumulator_size; i++) {
		accumulator[i] = one_ppqn_in_frames;
	}
	
	last_position = 0;
	last_timestamp = 0;
	
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

	last_position = 0;
	last_timestamp = 0;
	
	_started = false;
	reset();
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
MIDIClock_Slave::stop_if_no_more_clock_events(nframes_t& pos, nframes_t now)
{
	/* no timecode for 1/4 second ? conclude that its stopped */
	if (last_timestamp && 
	    now > last_timestamp && 
	    now - last_timestamp > session.frame_rate() / 4) {
        #ifdef DEBUG_MIDI_CLOCK			
			cerr << "No MIDI Clock frames received for some time, stopping!" << endl;
        #endif		
		pos = last_position;
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
	if (!_started || _starting) {
		speed = 0.0;
		pos = 0;
		return true;
	}
		
	nframes_t engine_now = session.engine().frame_time();

	if (stop_if_no_more_clock_events(pos, engine_now)) {
		return false;
	}
	
	#ifdef DEBUG_MIDI_CLOCK	
		cerr << "speed_and_position: engine time: " << engine_now << " last message timestamp: " << last_timestamp;
	#endif
	
	// calculate speed
	double speed_double = one_ppqn_in_frames / average_midi_clock_frame_duration;
	speed = float(speed_double);
	
    #ifdef DEBUG_MIDI_CLOCK	
		cerr << " final speed: " << speed;
    #endif
	
	// calculate position
	if (engine_now > last_timestamp) {
		// we are in between MIDI clock messages
		// so we interpolate position according to speed
		nframes_t elapsed = engine_now - last_timestamp;
		pos = nframes_t (last_position + double(elapsed) * speed_double);
	} else {
		// A new MIDI clock message has arrived this cycle
		pos = last_position;
	}
	
   #ifdef DEBUG_MIDI_CLOCK	
	   cerr << " transport position engine_now: " <<  session.transport_frame(); 
	   cerr << " calculated position: " << pos; 
	   cerr << endl;
   #endif
   
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

	last_position = 0;		
	last_timestamp = 0;
	
	session.request_locate(0, false);
}
