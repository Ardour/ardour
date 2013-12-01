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
*/

#include "pbd/compose.h"
#include "pbd/stacktrace.h"

#include "evoral/midi_events.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_port.h"
#include "ardour/ticker.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/debug.h"

using namespace ARDOUR;
using namespace PBD;

/** MIDI Clock Position tracking */
class MidiClockTicker::Position : public Timecode::BBT_Time
{
public:

	Position() : speed(0.0f), frame(0) { }
	~Position() { }

	/** Sync timing information taken from the given Session
		@return True if timings differed */
	bool sync (Session* s) {

		bool didit = false;

		double     sp = s->transport_speed();
		framecnt_t fr = s->transport_frame();

		if (speed != sp) {
			speed = sp;
			didit = true;
		}

		if (frame != fr) {
			frame = fr;
			didit = true;
		}

		/* Midi beats and clocks always gets updated for now */

		s->bbt_time (this->frame, *this);

		const TempoMap& tempo = s->tempo_map();

		const double divisions   = tempo.meter_at(frame).divisions_per_bar();
		const double divisor     = tempo.meter_at(frame).note_divisor();
		const double qnote_scale = divisor * 0.25f;

		/** Midi Beats in terms of Song Position Pointer is equivalent to total
			sixteenth notes at 'time' */

		midi_beats  = (((bars - 1) * divisions) + beats - 1);
		midi_beats += (double)ticks / (double)Position::ticks_per_beat * qnote_scale;
		midi_beats *= 16.0f / divisor;

		midi_clocks = midi_beats * 6.0f;

		return didit;
	}

	double      speed;
	framecnt_t  frame;
	double      midi_beats;
	double      midi_clocks;

	void print (std::ostream& s) {
		s << "frames: " << frame << " midi beats: " << midi_beats << " speed: " << speed;
	}
};


MidiClockTicker::MidiClockTicker ()
	: _ppqn (24)
	, _last_tick (0.0)
	, _send_pos (false)
	, _send_state (false)
{
	_pos.reset (new Position());
}

MidiClockTicker::~MidiClockTicker()
{
	_pos.reset (0);
}

void
MidiClockTicker::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);
	
	 if (_session) {
		 _session->TransportStateChange.connect_same_thread (_session_connections, boost::bind (&MidiClockTicker::transport_state_changed, this));
		 _session->TransportLooped.connect_same_thread (_session_connections, boost::bind (&MidiClockTicker::transport_looped, this));
		 _session->Located.connect_same_thread (_session_connections, boost::bind (&MidiClockTicker::session_located, this));

		 update_midi_clock_port();
		 _pos->sync (_session);
	 }
}

void
MidiClockTicker::session_located()
{
	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Session Located: %1, speed: %2\n", _session->transport_frame(), _session->transport_speed()));

	if (0 == _session || ! _pos->sync (_session)) {
		return;
	}

	_last_tick = _pos->frame;

	if (!Config->get_send_midi_clock()) {
		return;
	}

	_send_pos = true;
}

void
MidiClockTicker::session_going_away ()
{
	SessionHandlePtr::session_going_away();
	_midi_port.reset ();
}

void
MidiClockTicker::update_midi_clock_port()
{
	_midi_port = _session->midi_clock_output_port();
}

void
MidiClockTicker::transport_state_changed()
{
	if (_session->exporting()) {
		/* no midi clock during export, for now */
		return;
	}

	if (!_session->engine().running()) {
		/* Engine stopped, we can't do anything */
		return;
	}

	if (! _pos->sync (_session)) {
		return;
	}

	DEBUG_TRACE (DEBUG::MidiClock,
		 string_compose ("Transport state change @ %4, speed: %1 position: %2 play loop: %3\n",
							_pos->speed, _pos->frame, _session->get_play_loop(), _pos->frame)
	);

	_last_tick = _pos->frame;

	if (! Config->get_send_midi_clock()) {
		return;
	}

	_send_state = true;

	// tick (_pos->frame);
}

void
MidiClockTicker::transport_looped()
{
	Location* loop_location = _session->locations()->auto_loop_location();
	assert(loop_location);

	DEBUG_TRACE (DEBUG::MidiClock,
		     string_compose ("Transport looped, position: %1, loop start: %2, loop end: %3, play loop: %4\n",
				     _session->transport_frame(), loop_location->start(), loop_location->end(), _session->get_play_loop())
		);

	// adjust _last_tick, so that the next MIDI clock message is sent
	// in due time (and the tick interval is still constant)

	framecnt_t elapsed_since_last_tick = loop_location->end() - _last_tick;

	if (loop_location->start() > elapsed_since_last_tick) {
		_last_tick = loop_location->start() - elapsed_since_last_tick;
	} else {
		_last_tick = 0;
	}
}

void
MidiClockTicker::tick (const framepos_t& /* transport_frame */, pframes_t nframes)
{
	if (!Config->get_send_midi_clock() || _session == 0 || _session->transport_speed() != 1.0f || _midi_port == 0) {
		return;
	}

	if (_send_pos) {
		if (_pos->speed == 0.0f) {
			uint32_t where = llrint (_pos->midi_beats);
			send_position_event (where, 0, nframes);
		} else if (_pos->speed == 1.0f) {
#if 1
			/* Experimental.  To really do this and have accuracy, the
			   stop/locate/continue sequence would need queued to send immediately
			   before the next midi clock. */
			
			send_stop_event (0, nframes);
			
			if (_pos->frame == 0) {
				send_start_event (0, nframes);
			} else {
				uint32_t where = llrint (_pos->midi_beats);
				send_position_event (where, 0, nframes);
				send_continue_event (0, nframes);
			}
#endif
		} else {
			/* Varispeed not supported */
		}

		_send_pos = false;
	}

	if (_send_state) {
		if (_pos->speed == 1.0f) {
			if (_session->get_play_loop()) {
				assert(_session->locations()->auto_loop_location());
				
				if (_pos->frame == _session->locations()->auto_loop_location()->start()) {
					send_start_event (0, nframes);
				} else {
					send_continue_event (0, nframes);
				}
				
			} else if (_pos->frame == 0) {
				send_start_event (0, nframes);
			} else {
				send_continue_event (0, nframes);
			}
			
			// send_midi_clock_event (0);
			
		} else if (_pos->speed == 0.0f) {
			send_stop_event (0, nframes);
			send_position_event (llrint (_pos->midi_beats), 0, nframes);
		}

		_send_state = false;
	}


	const framepos_t end = _pos->frame + nframes;
	double iter = _last_tick;

	while (true) {
		double clock_delta = one_ppqn_in_frames (llrint (iter));
		double next_tick   = iter + clock_delta;
		frameoffset_t next_tick_offset = llrint (next_tick) - end;

		DEBUG_TRACE (DEBUG::MidiClock,
				 string_compose ("Tick: iter: %1, last tick time: %2, next tick time: %3, offset: %4, cycle length: %5\n",
						 iter, _last_tick, next_tick, next_tick_offset, nframes));

		if (next_tick_offset >= nframes) {
			break;
		}

		if (next_tick_offset >= 0) {
			send_midi_clock_event (next_tick_offset, nframes);
		}

		iter = next_tick;
	}

	_last_tick  = iter;
	_pos->frame = end;
}

double
MidiClockTicker::one_ppqn_in_frames (framepos_t transport_position)
{
	const Tempo& current_tempo = _session->tempo_map().tempo_at (transport_position);
	double frames_per_beat = current_tempo.frames_per_beat (_session->nominal_frame_rate());

	double quarter_notes_per_beat = 4.0 / current_tempo.note_type();
	double frames_per_quarter_note = frames_per_beat / quarter_notes_per_beat;

	return frames_per_quarter_note / double (_ppqn);
}

void
MidiClockTicker::send_midi_clock_event (pframes_t offset, pframes_t nframes)
{
	if (!_midi_port) {
		return;
	}

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Tick with offset %1\n", offset));

	static uint8_t tick_byte = { MIDI_CMD_COMMON_CLOCK };
	MidiBuffer& mb (_midi_port->get_midi_buffer (nframes));
	mb.push_back (offset, 1, &tick_byte);
}

void
MidiClockTicker::send_start_event (pframes_t offset, pframes_t nframes)
{
	if (!_midi_port) {
		return;
	}

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Start %1\n", _last_tick));

	static uint8_t tick_byte = { MIDI_CMD_COMMON_START };
	MidiBuffer& mb (_midi_port->get_midi_buffer (nframes));
	mb.push_back (offset, 1, &tick_byte);
}

void
MidiClockTicker::send_continue_event (pframes_t offset, pframes_t nframes)
{
	if (!_midi_port) {
		return;
	}

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Continue %1\n", _last_tick));

	static uint8_t tick_byte = { MIDI_CMD_COMMON_CONTINUE };
	MidiBuffer& mb (_midi_port->get_midi_buffer (nframes));
	mb.push_back (offset, 1, &tick_byte);
}

void
MidiClockTicker::send_stop_event (pframes_t offset, pframes_t nframes)
{
	if (!_midi_port) {
		return;
	}

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Stop %1\n", _last_tick));

	static uint8_t tick_byte = { MIDI_CMD_COMMON_STOP };
	MidiBuffer& mb (_midi_port->get_midi_buffer (nframes));
	mb.push_back (offset, 1, &tick_byte);
}

void
MidiClockTicker::send_position_event (uint32_t midi_beats, pframes_t offset, pframes_t nframes)
{
	if (!_midi_port) {
		return;
	}

	/* can only use 14bits worth */
	if (midi_beats > 0x3fff) {
		return;
	}

	/* split midi beats into a 14bit value */
	MIDI::byte msg[3];
	msg[0] = MIDI_CMD_COMMON_SONG_POS;
	msg[1] = midi_beats & 0x007f;
	msg[2] = midi_beats >> 7;

	MidiBuffer& mb (_midi_port->get_midi_buffer (nframes));
	mb.push_back (offset, 3, &msg[0]);

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Song Position Sent: %1\n", midi_beats));
}
