/*
 * Copyright (C) 2008-2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 Michael Fisher <mfisher31@gmail.com>
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

#include "pbd/compose.h"

#include "evoral/midi_events.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/lmath.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_port.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/ticker.h"
#include "ardour/transport_fsm.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Temporal;

MidiClockTicker::MidiClockTicker (Session& s)
	: _session (s)
	, _midi_port (s.midi_clock_output_port ())
	, _rolling (false)
	, _next_tick (0)
	, _beat_pos (0)
	, _clock_cnt (0)
	, _transport_pos (-1)
{
	resync_latency (true);
	_session.LatencyUpdated.connect_same_thread (_latency_connection, boost::bind (&MidiClockTicker::resync_latency, this, _1));
}

MidiClockTicker::~MidiClockTicker ()
{
}

void
MidiClockTicker::reset ()
{
	DEBUG_TRACE (DEBUG::MidiClock, "reset!\n");
	_rolling       = false;
	_next_tick     = 0;
	_beat_pos      = 0;
	_clock_cnt     = 0;
	_transport_pos = -1;
}

void
MidiClockTicker::resync_latency (bool playback)
{
	if (_session.deletion_in_progress() || !playback) {
		return;
	}
	assert (_midi_port);
	_midi_port->get_connected_latency_range(_mclk_out_latency, true);
	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("resync latency: %1\n", _mclk_out_latency.max));
}

void
MidiClockTicker::tick (ProcessedRanges const & pr, pframes_t n_samples, samplecnt_t pre_roll)
{

	const pframes_t full_nsamples = n_samples;
	samplecnt_t total = pr.end[0] - pr.start[0];

	if (pr.cnt > 1) {
		total += pr.end[1] - pr.start[1];
	}

	const double speed = total / n_samples;
	pframes_t offset = 0;

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("tick for %1 ranges (%2) w/preroll %3 @ %4, currently rolling %5\n", pr.cnt, n_samples, pre_roll, speed, _rolling));

	_midi_port->cycle_start (n_samples);

	/* If told not to send, or we're not moving, or we're moving backwards,
	 * ensure we have sent a stop message, reset and be done.
	 */

	if (!Config->get_send_midi_clock () || (pr.start[0] == pr.end[0]) || (pr.end[0] < pr.start[0])) { /*|| !TransportMasterManager::instance().current()*/
		if (_rolling) {
			send_stop_event (0, n_samples);
			DEBUG_TRACE (DEBUG::MidiClock, "stopped sending!\n");
		}
		DEBUG_TRACE (DEBUG::MidiClock, "not sending!\n");
		reset ();
		goto out;
	}

	n_samples -= pr.end[0] - pr.start[0];

	/* Special case code for "pre-roll ends during this call, and position
	 * is zero
	 */

	if ((speed == 0) && (pr.start[0] == 0)) {

		/* test if pre-roll is active, special-case
		 * "start at zero"
		 */

		if (pre_roll > 0 && pre_roll >= _mclk_out_latency.max && pre_roll < _mclk_out_latency.max + n_samples) {
			assert (!_rolling);

			pframes_t pos = pre_roll - _mclk_out_latency.max;
			_next_tick    = one_ppqn_in_samples (0) - _mclk_out_latency.max;

			DEBUG_TRACE (DEBUG::MidiClock, string_compose (
			                                   "Preroll Start offset: %1 (port latency: %2, remaining preroll: %3) next %4\n",
			                                   pos, _mclk_out_latency.max, pre_roll, _next_tick));

			_rolling       = true;
			_beat_pos      = 0;
			_clock_cnt     = 1;
			_transport_pos = 0;

			send_start_event (pos, n_samples);
			send_midi_clock_event (pos, n_samples);
		}

		/* Handle case _mclk_out_latency.max > one_ppqn_in_samples (0)
		 * may need to send more than one clock */

		if (pre_roll > 0 && _next_tick < 0) {
			assert (_rolling);
			while (_next_tick < 0 && pre_roll >= -_next_tick && pre_roll < n_samples - _next_tick) {
				pframes_t pos = pre_roll + _next_tick;
				_next_tick   += one_ppqn_in_samples (llrint (0));

				DEBUG_TRACE (DEBUG::MidiClock, string_compose (
				                                   "Preroll Tick offset: %1 (port latency: %2, remaining preroll: %3) next %4\n",
				                                   pos, _mclk_out_latency.max, pre_roll, _next_tick));

				send_midi_clock_event (pos, n_samples);

				if (++_clock_cnt == 6) {
					_clock_cnt = 0;
					++_beat_pos;
				}
			}
		}

		if (pre_roll > 0) {
			goto out;
		}
	}

	if (speed == 0) {
		goto out;
	}

	sub_tick (pr.start[0], pr.end[0], n_samples, pre_roll, speed, offset);
	offset = pr.end[0] - pr.start[0];

	if (pr.cnt > 1) {

		/* we located for the end of a loop, so notify the receiver
		 * appropriately. If we located back to zero, treat that as a
		 * special case.
		 */

		if (pr.start[1] == 0) {
			send_start_event (0, n_samples);
		} else {

			uint32_t    beat_pos;
			samplepos_t clk_pos;

			Temporal::TempoMap::use()->midi_clock_beat_at_or_after (pr.start[1] + _mclk_out_latency.max, clk_pos, beat_pos);

			_beat_pos      = beat_pos;
			_next_tick     = clk_pos - _mclk_out_latency.max;

			send_position_event (_beat_pos, offset, n_samples); // consider sending this early
			send_continue_event (offset + (_next_tick - pr.start[1]), n_samples);
		}

		sub_tick (pr.start[1], pr.end[1], n_samples, pre_roll, speed, offset);
	}

	_transport_pos = pr.end[pr.cnt - 1];

out:
	_midi_port->flush_buffers (full_nsamples);
	_midi_port->cycle_end (full_nsamples);

}

/*
 * start_sanple .. end_sample is guaranteed to represent a single continuous,
 * advancing time range.
 *
 */

void
MidiClockTicker::sub_tick (samplepos_t start_sample, samplepos_t end_sample, pframes_t n_samples, samplecnt_t& pre_roll, double speed, pframes_t offset)
{
	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("sub-tick for %1 .. %2 (%3) w/preroll %4 offset = %5\n", start_sample, end_sample, n_samples, pre_roll, offset));

	/* silence buffer */

	if (!_rolling) {
		if (_transport_pos < 0 || _next_tick < start_sample) {
			/* get the next downbeat */
			uint32_t    beat_pos;
			samplepos_t clk_pos;

			Temporal::TempoMap::use()->midi_clock_beat_at_or_after (start_sample + _mclk_out_latency.max, clk_pos, beat_pos);

			_beat_pos      = beat_pos;
			_next_tick     = clk_pos - _mclk_out_latency.max;
		}

		if (_next_tick >= start_sample && _next_tick < end_sample) {
			DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Start rolling at %1 beat-pos: %2\n", _next_tick, _beat_pos));

			_rolling   = true;
			_clock_cnt = 0;

			if (_beat_pos == 0 && _next_tick == 0 && start_sample == 0) {
				send_start_event (offset, n_samples);
			} else {
				send_position_event (_beat_pos, offset, n_samples); // consider sending this early
				send_continue_event (offset + (_next_tick - start_sample), n_samples);
			}

			DEBUG_TRACE (DEBUG::MidiClock, string_compose ("next tick reset to %1 from %2 + %3\n", _next_tick, start_sample, _mclk_out_latency.max));

		} else {
			DEBUG_TRACE (DEBUG::MidiClock, string_compose ("not time for next tick at %1 from %2 + %3\n", _next_tick, start_sample, _mclk_out_latency.max));
			return;
		}
	}

	while (_next_tick >= start_sample && _next_tick < end_sample) {

		DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Tick @ %1 cycle: %2 .. %3 nsamples: %4, ticker-pos: %5\n",
		                                               _next_tick, start_sample, end_sample, n_samples, _transport_pos));
		send_midi_clock_event (offset + (_next_tick - start_sample), n_samples);

		if (++_clock_cnt == 6) {
			_clock_cnt = 0;
			++_beat_pos;
		}

		_next_tick += one_ppqn_in_samples (llrint (_next_tick));
	}

	pre_roll -= end_sample - start_sample;
}

double
MidiClockTicker::one_ppqn_in_samples (samplepos_t transport_position) const
{
	Tempo const & tempo (TempoMap::use()->metric_at (transport_position).tempo());
	const double samples_per_quarter_note = tempo.samples_per_quarter_note (_session.nominal_sample_rate());
	return samples_per_quarter_note / 24.0;
}

void
MidiClockTicker::send_midi_clock_event (pframes_t offset, pframes_t nframes)
{
	assert (_midi_port);

	static uint8_t msg = MIDI_CMD_COMMON_CLOCK;

	MidiBuffer& mb (_midi_port->get_midi_buffer (nframes));
	mb.push_back (offset, Evoral::MIDI_EVENT, 1, &msg);

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Tick with offset %1\n", offset));
}

void
MidiClockTicker::send_start_event (pframes_t offset, pframes_t nframes)
{
	assert (_midi_port);

	static uint8_t msg = { MIDI_CMD_COMMON_START };
	MidiBuffer&    mb (_midi_port->get_midi_buffer (nframes));
	mb.push_back (offset, Evoral::MIDI_EVENT, 1, &msg);

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Start %1\n", _next_tick));
}

void
MidiClockTicker::send_continue_event (pframes_t offset, pframes_t nframes)
{
	assert (_midi_port);

	static uint8_t msg = { MIDI_CMD_COMMON_CONTINUE };
	MidiBuffer&    mb (_midi_port->get_midi_buffer (nframes));
	mb.push_back (offset, Evoral::MIDI_EVENT, 1, &msg);

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Continue %1\n", _next_tick));
}

void
MidiClockTicker::send_stop_event (pframes_t offset, pframes_t nframes)
{
	assert (_midi_port);

	static uint8_t msg = MIDI_CMD_COMMON_STOP;
	MidiBuffer&    mb (_midi_port->get_midi_buffer (nframes));
	mb.push_back (offset, Evoral::MIDI_EVENT, 1, &msg);

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Stop %1\n", _next_tick));
}

void
MidiClockTicker::send_position_event (uint32_t midi_beats, pframes_t offset, pframes_t nframes)
{
	assert (_midi_port);

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
	mb.push_back (offset, Evoral::MIDI_EVENT, 3, &msg[0]);

	DEBUG_TRACE (DEBUG::MidiClock, string_compose ("Song Position Sent: %1 to %2 (events now %3, buf = %4)\n", midi_beats, _midi_port->name (), mb.size (), &mb));
}
