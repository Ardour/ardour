/*
    Copyright (C) 2008 Hans Baier 

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

    $Id$
*/

#include "ardour/ticker.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#define DEBUG_TICKER 0

namespace ARDOUR
{


void Ticker::set_session(Session& s) 
{
	 _session = &s;
	 
	 if(_session) {
		 _session->tick.connect(mem_fun (*this, &Ticker::tick));
		 _session->GoingAway.connect(mem_fun (*this, &Ticker::going_away));
	 }
}

void MidiClockTicker::set_session(Session& s) 
{
	 Ticker::set_session(s);
	 
	 if(_session) {
		 _session->MIDIClock_PortChanged.connect(mem_fun (*this, &MidiClockTicker::update_midi_clock_port));
		 _session->TransportStateChange .connect(mem_fun (*this, &MidiClockTicker::transport_state_changed));
		 _session->PositionChanged      .connect(mem_fun (*this, &MidiClockTicker::position_changed));		 
		 _session->TransportLooped      .connect(mem_fun (*this, &MidiClockTicker::transport_looped));		 
		 update_midi_clock_port();
	 }
}

void MidiClockTicker::update_midi_clock_port()
{
	 _jack_port = (MIDI::JACK_MidiPort*) _session->midi_clock_port();
}

void MidiClockTicker::transport_state_changed()
{
	float     speed     = _session->transport_speed();
	nframes_t position  = _session->transport_frame();
#if DEBUG_TICKER	
	cerr << "Transport state change, speed:" << speed << "position:" << position<< " play loop " << _session->get_play_loop() << endl;
#endif	
	if (speed == 1.0f) {
		_last_tick = position;
		
		if (!Config->get_send_midi_clock()) 
			return;
		
		if (_session->get_play_loop()) {
			assert(_session->locations()->auto_loop_location());
			if (position == _session->locations()->auto_loop_location()->start()) {
				send_start_event(0);
			} else {
				send_continue_event(0);
			}
		} else if (position == 0) {
			send_start_event(0);
		} else {
			send_continue_event(0);
		}
		
		send_midi_clock_event(0);
		
	} else if (speed == 0.0f) {
		if (!Config->get_send_midi_clock()) 
			return;
		
		send_stop_event(0);
	}
	
	tick(position, *((ARDOUR::BBT_Time *) 0), *((SMPTE::Time *)0));
}

void MidiClockTicker::position_changed(nframes_t position)
{
#if DEBUG_TICKER	
	cerr << "Position changed:" << position << endl;
#endif	
	_last_tick = position;
}

void MidiClockTicker::transport_looped()
{
	nframes_t position  = _session->transport_frame();
	
	Location* loop_location = _session->locations()->auto_loop_location();
	assert(loop_location);

#if DEBUG_TICKER	
	cerr << "Transport looped, position:" <<  position 
	     << " loop start " << loop_location->start( )
	     << " loop end " << loop_location->end( )
	     << " play loop " << _session->get_play_loop()
	     <<  endl;
#endif
	
	// adjust _last_tick, so that the next MIDI clock message is sent 
	// in due time (and the tick interval is still constant)
	nframes_t elapsed_since_last_tick = loop_location->end() - _last_tick;
	_last_tick = loop_location->start() - elapsed_since_last_tick;
}

void MidiClockTicker::tick(const nframes_t& transport_frames, const BBT_Time& transport_bbt, const SMPTE::Time& transport_smpt)
{	
	if (!Config->get_send_midi_clock() || _session == 0 || _session->transport_speed() != 1.0f)
		return;

	while (true) {
		double next_tick = _last_tick + one_ppqn_in_frames(transport_frames);
		nframes_t next_tick_offset = nframes_t(next_tick) - transport_frames;
		
#if DEBUG_TICKER	
		cerr << "Transport:" << transport_frames 
			 << ":Last tick time:" << _last_tick << ":" 
			 << ":Next tick time:" << next_tick << ":" 
			 << "Offset:" << next_tick_offset << ":"
			 << "cycle length:" << _jack_port->nframes_this_cycle() 
			 << endl; 
#endif	
		
		if (next_tick_offset >= _jack_port->nframes_this_cycle())
			return;
	
		send_midi_clock_event(next_tick_offset);
		
		_last_tick = next_tick;
	}
}

double MidiClockTicker::one_ppqn_in_frames(nframes_t transport_position)
{
	const Tempo& current_tempo = _session->tempo_map().tempo_at(transport_position);
	const Meter& current_meter = _session->tempo_map().meter_at(transport_position);
	double frames_per_beat =
		current_tempo.frames_per_beat(_session->nominal_frame_rate(),
		                              current_meter);

	double quarter_notes_per_beat = 4.0 / current_tempo.note_type();
	double frames_per_quarter_note = frames_per_beat / quarter_notes_per_beat;

	return frames_per_quarter_note / double (_ppqn);
}

void MidiClockTicker::send_midi_clock_event(nframes_t offset)
{
	assert(_jack_port->is_process_thread());
#if DEBUG_TICKER	
	cerr << "Tick with offset " << offset << endl;
#endif	
	static uint8_t _midi_clock_tick[1] = { MIDI_CMD_COMMON_CLOCK };
	_jack_port->write(_midi_clock_tick, 1, offset);
}

void MidiClockTicker::send_start_event(nframes_t offset)
{
	static uint8_t _midi_clock_tick[1] = { MIDI_CMD_COMMON_START };
	_jack_port->write(_midi_clock_tick, 1, offset);
}

void MidiClockTicker::send_continue_event(nframes_t offset)
{
	static uint8_t _midi_clock_tick[1] = { MIDI_CMD_COMMON_CONTINUE };
	_jack_port->write(_midi_clock_tick, 1, offset);
}

void MidiClockTicker::send_stop_event(nframes_t offset)
{
	static uint8_t _midi_clock_tick[1] = { MIDI_CMD_COMMON_STOP };
	_jack_port->write(_midi_clock_tick, 1, offset);
}

}

