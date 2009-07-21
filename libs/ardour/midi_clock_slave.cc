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

#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <unistd.h>
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"

#include "midi++/port.h"
#include "midi++/jack.h"
#include "ardour/slave.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/cycles.h"
#include "ardour/tempo.h"


#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace sigc;
using namespace MIDI;
using namespace PBD;

MIDIClock_Slave::MIDIClock_Slave (Session& s, MIDI::Port& p, int ppqn)
	: ppqn (ppqn)
	, bandwidth (30.0 / 60.0) // 1 BpM = 1 / 60 Hz
{
	session = (ISlaveSessionProxy *) new SlaveSessionProxy(s);
	rebind (p);
	reset ();
}

MIDIClock_Slave::MIDIClock_Slave (ISlaveSessionProxy* session_proxy, int ppqn)
	: session(session_proxy)
	, ppqn (ppqn)
	, bandwidth (30.0 / 60.0) // 1 BpM = 1 / 60 Hz
{
	session = session_proxy;
	reset ();
}

MIDIClock_Slave::~MIDIClock_Slave()
{
  delete session;
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
	connections.push_back (port->input()->position.connect (mem_fun (*this, &MIDIClock_Slave::position)));
}

void 
MIDIClock_Slave::calculate_one_ppqn_in_frames_at(nframes_t time)
{
	const Tempo& current_tempo = session->tempo_map().tempo_at(time);
	const Meter& current_meter = session->tempo_map().meter_at(time);
	double frames_per_beat =
		current_tempo.frames_per_beat(session->frame_rate(),
		                              current_meter);

	double quarter_notes_per_beat = 4.0 / current_tempo.note_type();
	double frames_per_quarter_note = frames_per_beat / quarter_notes_per_beat;

	one_ppqn_in_frames = frames_per_quarter_note / double (ppqn);
}

ARDOUR::nframes_t 
MIDIClock_Slave::calculate_song_position(uint16_t song_position_in_sixteenth_notes)
{
	nframes_t song_position_frames = 0;
	for (uint16_t i = 1; i <= song_position_in_sixteenth_notes; ++i) {
		// one quarter note contains ppqn pulses, so a sixteenth note is ppqn / 4 pulses
		calculate_one_ppqn_in_frames_at(song_position_frames);
		song_position_frames += one_ppqn_in_frames * nframes_t(ppqn / 4);
	}
	
	return song_position_frames;
}

void 
MIDIClock_Slave::calculate_filter_coefficients()
{
	// omega = 2 * PI * Bandwidth / MIDI clock frame frequency in Hz
	omega = 2.0 * 3.14159265358979323846 * bandwidth * one_ppqn_in_frames / session->frame_rate();
	b = 1.4142135623730950488 * omega;
	c = omega * omega;	
}

void
MIDIClock_Slave::update_midi_clock (Parser& /*parser*/, nframes_t timestamp)
{	
	// some pieces of hardware send MIDI Clock all the time				
	if ( (!_starting) && (!_started) ) {
		return;
	}
		
	calculate_one_ppqn_in_frames_at(should_be_position);
	
	nframes_t elapsed_since_start = timestamp - first_timestamp;
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
		// we use session->transport_frame() instead of t1 here
		// because t1 is used to calculate the transport speed,
		// so the loop will compensate for accumulating rounding errors
		error = (double(should_be_position) - double(session->audible_frame())); 
		e = error / double(session->frame_rate());
		
		// update DLL
		t0 = t1;
		t1 += b * e + e2;
		e2 += c * e;			
	}	
	
	#ifdef DEBUG_MIDI_CLOCK		
		cerr 
				  << "MIDI Clock #" << midi_clock_count
				  //<< "@" << timestamp  
				  << " arrived at: " << elapsed_since_start << " (elapsed time) " 
				  << " should-be transport: " << should_be_position 
				  << " audible: " << session->audible_frame()
				  << " real transport: " << session->transport_frame()
				  << " error: " << error
				  //<< " engine: " << session->frame_time() 
				  << " real delta: " << timestamp - last_timestamp 
				  << " should-be delta: " << one_ppqn_in_frames
				  << " t1-t0: " << (t1 -t0) * session->frame_rate()
				  << " t0: " << t0 * session->frame_rate()
				  << " t1: " << t1 * session->frame_rate() 
				  << " frame-rate: " << session->frame_rate() 
				  << endl;
		
		cerr      << "frames since cycle start: " << session->frames_since_cycle_start() << endl;
	#endif // DEBUG_MIDI_CLOCK

	last_timestamp = timestamp;
}

void
MIDIClock_Slave::start (Parser& /*parser*/, nframes_t /*timestamp*/)
{	
	#ifdef DEBUG_MIDI_CLOCK	
		cerr << "MIDIClock_Slave got start message at time "  <<  timestamp << " engine time: " << session->frame_time() << endl;
	#endif
	
	if (!_started) {
		reset();
		
		_started = true;
		_starting = true;
	}
}

void
MIDIClock_Slave::reset ()
{

	should_be_position = 0;		
	last_timestamp = 0;
	
	_starting = false;
	_started  = false;
	
	session->request_locate(0, false);
}

void
MIDIClock_Slave::contineu (Parser& /*parser*/, nframes_t /*timestamp*/)
{
	#ifdef DEBUG_MIDI_CLOCK	
		std::cerr << "MIDIClock_Slave got continue message" << endl;
	#endif
	if (!_started) {
		_starting = true;
		_started  = true; 
	}
}


void
MIDIClock_Slave::stop (Parser& /*parser*/, nframes_t /*timestamp*/)
{
	#ifdef DEBUG_MIDI_CLOCK	
		std::cerr << "MIDIClock_Slave got stop message" << endl;
	#endif
	
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
		nframes_t stop_position = should_be_position;
		
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
	nframes_t position_in_frames = calculate_song_position(position_in_sixteenth_notes);
	
	#ifdef DEBUG_MIDI_CLOCK
	cerr << "Song Position: " << position_in_sixteenth_notes << " frames: " << position_in_frames << endl; 
	#endif
	
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
MIDIClock_Slave::stop_if_no_more_clock_events(nframes_t& pos, nframes_t now)
{
	/* no timecode for 1/4 second ? conclude that its stopped */
	if (last_timestamp && 
	    now > last_timestamp && 
	    now - last_timestamp > session->frame_rate() / 4) {
        #ifdef DEBUG_MIDI_CLOCK			
			cerr << "No MIDI Clock frames received for some time, stopping!" << endl;
        #endif		
		pos = should_be_position;
		session->request_transport_speed (0);
		session->request_locate (should_be_position, false);
		return true;
	} else {
		return false;
	}
}

bool
MIDIClock_Slave::speed_and_position (double& speed, nframes_t& pos)
{
	if (!_started || _starting) {
		speed = 0.0;
		pos   = should_be_position;
		return true;
	}
		
	nframes_t engine_now = session->frame_time();
	
	if (stop_if_no_more_clock_events(pos, engine_now)) {
		return false;
	}

	// calculate speed
	speed = ((t1 - t0) * session->frame_rate()) / one_ppqn_in_frames;
	
	// calculate position
	if (engine_now > last_timestamp) {
		// we are in between MIDI clock messages
		// so we interpolate position according to speed
		nframes_t elapsed = engine_now - last_timestamp;
		pos = nframes_t (should_be_position + double(elapsed) * speed);
	} else {
		// A new MIDI clock message has arrived this cycle
		pos = should_be_position;
	}
	
	#ifdef DEBUG_MIDI_CLOCK			
	cerr << "speed_and_position: " << speed << " & " << pos << " <-> " << session->transport_frame() << " (transport)" << endl;
	#endif	
	
	return true;
}

ARDOUR::nframes_t
MIDIClock_Slave::resolution() const
{
	// one beat
	return (nframes_t) one_ppqn_in_frames * ppqn;
}

