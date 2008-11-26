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
		 update_midi_clock_port();
	 }
}

void MidiClockTicker::update_midi_clock_port()
{
	 _jack_port = (MIDI::JACK_MidiPort*) _session->midi_clock_port();
}

void MidiClockTicker::tick(const nframes_t& transport_frames, const BBT_Time& transport_bbt, const SMPTE::Time& transport_smpt)
{	
	if (!Config->get_send_midi_clock() || _session == 0 || _session->transport_speed() != 1.0f)
		return;
	
	const Tempo& current_tempo = _session->tempo_map().tempo_at(transport_frames);
	const Meter& current_meter = _session->tempo_map().meter_at(transport_frames);
	double frames_per_beat =
		current_tempo.frames_per_beat(_session->nominal_frame_rate(),
		                              current_meter);

	double quarter_notes_per_beat = 4.0 / current_tempo.note_type();
	double frames_per_quarter_note = frames_per_beat / quarter_notes_per_beat;

	nframes_t one_ppqn_in_frames = frames_per_quarter_note / double (_ppqn);
	
	nframes_t next_tick = _last_tick + one_ppqn_in_frames;
	nframes_t next_tick_offset = next_tick - transport_frames;
	
	assert(_jack_port->is_process_thread());
	static uint8_t _midi_clock_tick[1] = { MIDI_CMD_COMMON_TICK };
	_jack_port->write(_midi_clock_tick, 1, next_tick_offset);
	
	_last_tick = next_tick;
}

}

