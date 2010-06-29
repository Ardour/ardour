
/*
  Copyright (C) 1999-2002 Paul Davis

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

#include <string>
#include <cmath>
#include <cerrno>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <boost/shared_ptr.hpp>

#include <glibmm/main.h>

#include "midi++/mmc.h"
#include "midi++/port.h"
#include "midi++/manager.h"
#include "pbd/error.h"
#include "pbd/pthread_utils.h"

#include "ardour/configuration.h"
#include "ardour/debug.h"
#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/midi_ui.h"
#include "ardour/audio_diskstream.h"
#include "ardour/slave.h"
#include "ardour/cycles.h"
#include "ardour/timecode.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace MIDI;
using namespace Glib;

void
Session::midi_panic()
{
	{
		boost::shared_ptr<RouteList> r = routes.reader ();

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			MidiTrack *track = dynamic_cast<MidiTrack*>((*i).get());
			if (track != 0) {
				track->midi_panic();
			}
		}
	}
}

int
Session::use_config_midi_ports ()
{
	string port_name;

	if (default_mtc_port) {
		set_mtc_port (default_mtc_port->name());
	} else {
		set_mtc_port ("");
	}

	if (default_midi_port) {
		set_midi_port (default_midi_port->name());
	} else {
		set_midi_port ("");
	}

	if (default_midi_clock_port) {
		set_midi_clock_port (default_midi_clock_port->name());
	} else {
		set_midi_clock_port ("");
	}

	return 0;
}


/***********************************************************************
 MTC, MMC, etc.
**********************************************************************/

int
Session::set_mtc_port (string port_tag)
{
	MTC_Slave *ms;

	if (port_tag.length() == 0) {

		if (_slave && ((ms = dynamic_cast<MTC_Slave*> (_slave)) != 0)) {
			error << string_compose (_("%1 is slaved to MTC - port cannot be reset"), PROGRAM_NAME) << endmsg;
			return -1;
		}

		if (_mtc_port == 0) {
			return 0;
		}

		_mtc_port = 0;
		goto out;
	}

	MIDI::Port* port;

	if ((port = MIDI::Manager::instance()->port (port_tag)) == 0) {
		error << string_compose (_("unknown port %1 requested for MTC"), port_tag) << endl;
		return -1;
	}

	_mtc_port = port;

	if (_slave && ((ms = dynamic_cast<MTC_Slave*> (_slave)) != 0)) {
		ms->rebind (*port);
	}

	Config->set_mtc_port_name (port_tag);

  out:
	MTC_PortChanged(); /* EMIT SIGNAL */
	set_dirty();
	return 0;
}

int
Session::set_midi_port (string /*port_tag*/)
{
#if 0
	if (port_tag.length() == 0) {
		if (_midi_port == 0) {
			return 0;
		}
		_midi_port = 0;
		goto out;
	}

	MIDI::Port* port;

	if ((port = MIDI::Manager::instance()->port (port_tag)) == 0) {
		return -1;
	}

	_midi_port = port;

	/* XXX need something to forward this to control protocols ? or just
	   use the signal below
	*/

	Config->set_midi_port_name (port_tag);

  out:
#endif
	MIDI_PortChanged(); /* EMIT SIGNAL */
	set_dirty();
	return 0;
}

int
Session::set_midi_clock_port (string port_tag)
{
	MIDIClock_Slave *ms;

	if (port_tag.length() == 0) {

		if (_slave && ((ms = dynamic_cast<MIDIClock_Slave*> (_slave)) != 0)) {
			error << string_compose (_("%1 is slaved to MIDI Clock - port cannot be reset"), PROGRAM_NAME) << endmsg;
			return -1;
		}

		if (_midi_clock_port == 0) {
			return 0;
		}

		_midi_clock_port = 0;
		goto out;
	}

	MIDI::Port* port;

	if ((port = MIDI::Manager::instance()->port (port_tag)) == 0) {
		error << string_compose (_("unknown port %1 requested for MIDI Clock"), port_tag) << endl;
		return -1;
	}

	_midi_clock_port = port;

	if (_slave && ((ms = dynamic_cast<MIDIClock_Slave*> (_slave)) != 0)) {
		ms->rebind (*port);
	}

	Config->set_midi_clock_port_name (port_tag);

  out:
	MIDIClock_PortChanged(); /* EMIT SIGNAL */
	set_dirty();
	return 0;
}

void
Session::set_trace_midi_input (bool yn, MIDI::Port* port)
{
	MIDI::Parser* input_parser;

	cerr << "enabling tracing: " << yn << " for input port " << port->name() << endl;

	if (port) {
		if ((input_parser = port->input()) != 0) {
			input_parser->trace (yn, &cout, "input: ");
		}
	} else {

		if (_mmc->port()) {
			if ((input_parser = _mmc->port()->input()) != 0) {
				input_parser->trace (yn, &cout, "input: ");
			}
		}

		if (_mtc_port && _mtc_port != _mmc->port()) {
			if ((input_parser = _mtc_port->input()) != 0) {
				input_parser->trace (yn, &cout, "input: ");
			}
		}

		if (_midi_port && _midi_port != _mmc->port() && _midi_port != _mtc_port  ) {
			if ((input_parser = _midi_port->input()) != 0) {
				input_parser->trace (yn, &cout, "input: ");
			}
		}

		if (_midi_clock_port
		    && _midi_clock_port != _mmc->port()
			&& _midi_clock_port != _mtc_port
			&& _midi_clock_port != _midi_port) {
			if ((input_parser = _midi_clock_port->input()) != 0) {
				input_parser->trace (yn, &cout, "input: ");
			}
		}
	}

	Config->set_trace_midi_input (yn);
}

void
Session::set_trace_midi_output (bool yn, MIDI::Port* port)
{
	MIDI::Parser* output_parser;

	if (port) {
		if ((output_parser = port->output()) != 0) {
			output_parser->trace (yn, &cout, "output: ");
		}
	} else {
		if (_mmc->port()) {
			if ((output_parser = _mmc->port()->output()) != 0) {
				output_parser->trace (yn, &cout, "output: ");
			}
		}

		if (_mtc_port && _mtc_port != _mmc->port()) {
			if ((output_parser = _mtc_port->output()) != 0) {
				output_parser->trace (yn, &cout, "output: ");
			}
		}

		if (_midi_port && _midi_port != _mmc->port() && _midi_port != _mtc_port  ) {
			if ((output_parser = _midi_port->output()) != 0) {
				output_parser->trace (yn, &cout, "output: ");
			}
		}

	}

	Config->set_trace_midi_output (yn);
}

bool
Session::get_trace_midi_input(MIDI::Port *port)
{
	MIDI::Parser* input_parser;
	if (port) {
		if ((input_parser = port->input()) != 0) {
			return input_parser->tracing();
		}
	}
	else {
		if (_mmc->port()) {
			if ((input_parser = _mmc->port()->input()) != 0) {
				return input_parser->tracing();
			}
		}

		if (_mtc_port) {
			if ((input_parser = _mtc_port->input()) != 0) {
				return input_parser->tracing();
			}
		}

		if (_midi_port) {
			if ((input_parser = _midi_port->input()) != 0) {
				return input_parser->tracing();
			}
		}
	}

	return false;
}

bool
Session::get_trace_midi_output(MIDI::Port *port)
{
	MIDI::Parser* output_parser;
	if (port) {
		if ((output_parser = port->output()) != 0) {
			return output_parser->tracing();
		}
	}
	else {
		if (_mmc->port()) {
			if ((output_parser = _mmc->port()->output()) != 0) {
				return output_parser->tracing();
			}
		}

		if (_mtc_port) {
			if ((output_parser = _mtc_port->output()) != 0) {
				return output_parser->tracing();
			}
		}

		if (_midi_port) {
			if ((output_parser = _midi_port->output()) != 0) {
				return output_parser->tracing();
			}
		}
	}

	return false;

}

void
Session::setup_midi_control ()
{
	outbound_mtc_timecode_frame = 0;
	next_quarter_frame_to_send = 0;

	/* Set up the qtr frame message */

	mtc_msg[0] = 0xf1;
	mtc_msg[2] = 0xf1;
	mtc_msg[4] = 0xf1;
	mtc_msg[6] = 0xf1;
	mtc_msg[8] = 0xf1;
	mtc_msg[10] = 0xf1;
	mtc_msg[12] = 0xf1;
	mtc_msg[14] = 0xf1;
}

void
Session::spp_start (Parser &, nframes_t /*timestamp*/)
{
	if (Config->get_mmc_control() && (!config.get_external_sync() || config.get_sync_source() != JACK)) {
		request_transport_speed (1.0);
	}
}

void
Session::spp_continue (Parser& ignored, nframes_t timestamp)
{
	spp_start (ignored, timestamp);
}

void
Session::spp_stop (Parser&, nframes_t /*timestamp*/)
{
	if (Config->get_mmc_control()) {
		request_stop ();
	}
}

void
Session::mmc_deferred_play (MIDI::MachineControl &/*mmc*/)
{
	if (Config->get_mmc_control() && (!config.get_external_sync() || (config.get_sync_source() != JACK))) {
		request_transport_speed (1.0);
	}
}

void
Session::mmc_record_pause (MIDI::MachineControl &/*mmc*/)
{
	if (Config->get_mmc_control()) {
		maybe_enable_record();
	}
}

void
Session::mmc_record_strobe (MIDI::MachineControl &/*mmc*/)
{
	if (!Config->get_mmc_control())
		return;

	/* record strobe does an implicit "Play" command */

	if (_transport_speed != 1.0) {

		/* start_transport() will move from Enabled->Recording, so we
		   don't need to do anything here except enable recording.
		   its not the same as maybe_enable_record() though, because
		   that *can* switch to Recording, which we do not want.
		*/

		save_state ("", true);
		g_atomic_int_set (&_record_status, Enabled);
		RecordStateChanged (); /* EMIT SIGNAL */

		request_transport_speed (1.0);

	} else {

		enable_record ();
	}
}

void
Session::mmc_record_exit (MIDI::MachineControl &/*mmc*/)
{
	if (Config->get_mmc_control()) {
		disable_record (false);
	}
}

void
Session::mmc_stop (MIDI::MachineControl &/*mmc*/)
{
	if (Config->get_mmc_control()) {
		request_stop ();
	}
}

void
Session::mmc_pause (MIDI::MachineControl &/*mmc*/)
{
	if (Config->get_mmc_control()) {

		/* We support RECORD_PAUSE, so the spec says that
		   we must interpret PAUSE like RECORD_PAUSE if
		   recording.
		*/

		if (actively_recording()) {
			maybe_enable_record ();
		} else {
			request_stop ();
		}
	}
}

static bool step_queued = false;

void
Session::mmc_step (MIDI::MachineControl &/*mmc*/, int steps)
{
	if (!Config->get_mmc_control()) {
		return;
	}

	struct timeval now;
	struct timeval diff = { 0, 0 };

	gettimeofday (&now, 0);

	timersub (&now, &last_mmc_step, &diff);

	gettimeofday (&now, 0);
	timersub (&now, &last_mmc_step, &diff);

	if (last_mmc_step.tv_sec != 0 && (diff.tv_usec + (diff.tv_sec * 1000000)) < _engine.usecs_per_cycle()) {
		return;
	}

	double diff_secs = diff.tv_sec + (diff.tv_usec / 1000000.0);
	double cur_speed = (((steps * 0.5) * timecode_frames_per_second()) / diff_secs) / timecode_frames_per_second();

	if (_transport_speed == 0 || cur_speed * _transport_speed < 0) {
		/* change direction */
		step_speed = cur_speed;
	} else {
		step_speed = (0.6 * step_speed) + (0.4 * cur_speed);
	}

	step_speed *= 0.25;

#if 0
	cerr << "delta = " << diff_secs
	     << " ct = " << _transport_speed
	     << " steps = " << steps
	     << " new speed = " << cur_speed
	     << " speed = " << step_speed
	     << endl;
#endif

	request_transport_speed (step_speed);
	last_mmc_step = now;

	if (!step_queued) {
		if (midi_control_ui) {
			RefPtr<TimeoutSource> tsrc = TimeoutSource::create (100);
			tsrc->connect (sigc::mem_fun (*this, &Session::mmc_step_timeout));
			tsrc->attach (midi_control_ui->main_loop()->get_context());
			step_queued = true;
		}
	}
}

void
Session::mmc_rewind (MIDI::MachineControl &/*mmc*/)
{
	if (Config->get_mmc_control()) {
		request_transport_speed(-8.0f);
	}
}

void
Session::mmc_fast_forward (MIDI::MachineControl &/*mmc*/)
{
	if (Config->get_mmc_control()) {
		request_transport_speed(8.0f);
	}
}

void
Session::mmc_locate (MIDI::MachineControl &/*mmc*/, const MIDI::byte* mmc_tc)
{
	if (!Config->get_mmc_control()) {
		return;
	}

	nframes_t target_frame;
	Timecode::Time timecode;

	timecode.hours = mmc_tc[0] & 0xf;
	timecode.minutes = mmc_tc[1];
	timecode.seconds = mmc_tc[2];
	timecode.frames = mmc_tc[3];
	timecode.rate = timecode_frames_per_second();
	timecode.drop = timecode_drop_frames();

	// Also takes timecode offset into account:
	timecode_to_sample( timecode, target_frame, true /* use_offset */, false /* use_subframes */ );

	if (target_frame > max_frames) {
		target_frame = max_frames;
	}

	/* Some (all?) MTC/MMC devices do not send a full MTC frame
	   at the end of a locate, instead sending only an MMC
	   locate command. This causes the current position
	   of an MTC slave to become out of date. Catch this.
	*/

	MTC_Slave* mtcs = dynamic_cast<MTC_Slave*> (_slave);

	if (mtcs != 0) {
		// cerr << "Locate *with* MTC slave\n";
		mtcs->handle_locate (mmc_tc);
	} else {
		// cerr << "Locate without MTC slave\n";
		request_locate (target_frame, false);
	}
}

void
Session::mmc_shuttle (MIDI::MachineControl &/*mmc*/, float speed, bool forw)
{
	if (!Config->get_mmc_control()) {
		return;
	}

	if (Config->get_shuttle_speed_threshold() >= 0 && speed > Config->get_shuttle_speed_threshold()) {
		speed *= Config->get_shuttle_speed_factor();
	}

	if (forw) {
		request_transport_speed (speed);
	} else {
		request_transport_speed (-speed);
	}
}

void
Session::mmc_record_enable (MIDI::MachineControl &mmc, size_t trk, bool enabled)
{
	if (Config->get_mmc_control()) {

		RouteList::iterator i;
		boost::shared_ptr<RouteList> r = routes.reader();

		for (i = r->begin(); i != r->end(); ++i) {
			AudioTrack *at;

			if ((at = dynamic_cast<AudioTrack*>((*i).get())) != 0) {
				if (trk == at->remote_control_id()) {
					at->set_record_enable (enabled, &mmc);
					break;
				}
			}
		}
	}
}

/** Send MTC Full Frame message (complete Timecode time) for the start of this cycle.
 * This resets the MTC code, the next quarter frame message that is sent will be
 * the first one with the beginning of this cycle as the new start point.
 */
int
Session::send_full_time_code(nframes_t /*nframes*/)
{
	/* This function could easily send at a given frame offset, but would
	 * that be useful?  Does ardour do sub-block accurate locating? [DR] */

	MIDI::byte msg[10];
	Timecode::Time timecode;

	_send_timecode_update = false;

	if (_mtc_port == 0 || !session_send_mtc || _slave) {
		return 0;
	}

	// Get timecode time for this transport frame
	sample_to_timecode(_transport_frame, timecode, true /* use_offset */, false /* no subframes */);

	transmitting_timecode_time = timecode;
	outbound_mtc_timecode_frame = _transport_frame;

	// I don't understand this bit yet.. [DR]
	if (((mtc_timecode_bits >> 5) != MIDI::MTC_25_FPS) && (transmitting_timecode_time.frames % 2)) {
		// start MTC quarter frame transmission on an even frame
		Timecode::increment (transmitting_timecode_time, config.get_subframes_per_frame());
		outbound_mtc_timecode_frame += (nframes_t) _frames_per_timecode_frame;
	}

	// Compensate for audio latency
	outbound_mtc_timecode_frame += _worst_output_latency;
	next_quarter_frame_to_send = 0;

	// Sync slave to the same Timecode time as we are on
	msg[0] = 0xf0;
	msg[1] = 0x7f;
	msg[2] = 0x7f;
	msg[3] = 0x1;
	msg[4] = 0x1;
	msg[9] = 0xf7;

	msg[5] = mtc_timecode_bits | timecode.hours;
	msg[6] = timecode.minutes;
	msg[7] = timecode.seconds;
	msg[8] = timecode.frames;

	// Send message at offset 0, sent time is for the start of this cycle
	if (_mtc_port->midimsg (msg, sizeof (msg), 0)) {
		error << _("Session: could not send full MIDI time code") << endmsg;
		return -1;
	}

	return 0;
}

/** Send MTC (quarter-frame) messages for this cycle.
 * Must be called exactly once per cycle from the audio thread.  Realtime safe.
 * This function assumes the state of full Timecode is sane, eg. the slave is
 * expecting quarter frame messages and has the right frame of reference (any
 * full MTC Timecode time messages that needed to be sent should have been sent
 * earlier already this cycle by send_full_time_code)
 */
int
Session::send_midi_time_code_for_cycle(nframes_t nframes)
{
	if (_mtc_port == 0 || _slave || !session_send_mtc || transmitting_timecode_time.negative || (next_quarter_frame_to_send < 0)) {
		// cerr << "(MTC) Not sending MTC\n";
		return 0;
	}

	assert (next_quarter_frame_to_send >= 0);
	assert (next_quarter_frame_to_send <= 7);

	/* Duration of one quarter frame */
	nframes_t quarter_frame_duration = ((long) _frames_per_timecode_frame) >> 2;

	DEBUG_TRACE (DEBUG::MTC, string_compose ("TF %1 SF %2 NQ %3 FD %4\n",  _transport_frame, outbound_mtc_timecode_frame,
						 next_quarter_frame_to_send, quarter_frame_duration));

	assert((outbound_mtc_timecode_frame + (next_quarter_frame_to_send * quarter_frame_duration))
			>= _transport_frame);


	// Send quarter frames for this cycle
	while (_transport_frame + nframes > (outbound_mtc_timecode_frame +
				(next_quarter_frame_to_send * quarter_frame_duration))) {

		DEBUG_TRACE (DEBUG::MTC, string_compose ("next frame to send: %1\n", next_quarter_frame_to_send));

		switch (next_quarter_frame_to_send) {
			case 0:
				mtc_msg[1] =  0x00 | (transmitting_timecode_time.frames & 0xf);
				break;
			case 1:
				mtc_msg[1] =  0x10 | ((transmitting_timecode_time.frames & 0xf0) >> 4);
				break;
			case 2:
				mtc_msg[1] =  0x20 | (transmitting_timecode_time.seconds & 0xf);
				break;
			case 3:
				mtc_msg[1] =  0x30 | ((transmitting_timecode_time.seconds & 0xf0) >> 4);
				break;
			case 4:
				mtc_msg[1] =  0x40 | (transmitting_timecode_time.minutes & 0xf);
				break;
			case 5:
				mtc_msg[1] = 0x50 | ((transmitting_timecode_time.minutes & 0xf0) >> 4);
				break;
			case 6:
				mtc_msg[1] = 0x60 | ((mtc_timecode_bits|transmitting_timecode_time.hours) & 0xf);
				break;
			case 7:
				mtc_msg[1] = 0x70 | (((mtc_timecode_bits|transmitting_timecode_time.hours) & 0xf0) >> 4);
				break;
		}

		const nframes_t msg_time = (outbound_mtc_timecode_frame
			+ (quarter_frame_duration * next_quarter_frame_to_send));

		// This message must fall within this block or something is broken
		assert(msg_time >= _transport_frame);
		assert(msg_time < _transport_frame + nframes);

		nframes_t out_stamp = msg_time - _transport_frame;
		assert(out_stamp < nframes);

		if (_mtc_port->midimsg (mtc_msg, 2, out_stamp)) {
			error << string_compose(_("Session: cannot send quarter-frame MTC message (%1)"), strerror (errno))
			      << endmsg;
			return -1;
		}

#ifndef NDEBUG
		DEBUG_STR_DECL(foo)
		DEBUG_STR_APPEND(foo,"sending ");
		DEBUG_STR_APPEND(foo, transmitting_timecode_time);
		DEBUG_TRACE (DEBUG::MTC, string_compose ("%1 qfm = %2, stamp = %3\n", DEBUG_STR(foo).str(), next_quarter_frame_to_send,
							 out_stamp));
#endif

		// Increment quarter frame counter
		next_quarter_frame_to_send++;

		if (next_quarter_frame_to_send >= 8) {
			// Wrap quarter frame counter
			next_quarter_frame_to_send = 0;
			// Increment timecode time twice
			Timecode::increment( transmitting_timecode_time, config.get_subframes_per_frame() );
			Timecode::increment( transmitting_timecode_time, config.get_subframes_per_frame() );
			// Re-calculate timing of first quarter frame
			//timecode_to_sample( transmitting_timecode_time, outbound_mtc_timecode_frame, true /* use_offset */, false );
			outbound_mtc_timecode_frame += 8 * quarter_frame_duration;
		}
	}

	return 0;
}

/***********************************************************************
 OUTBOUND MMC STUFF
**********************************************************************/


bool
Session::mmc_step_timeout ()
{
	struct timeval now;
	struct timeval diff;
	double diff_usecs;
	gettimeofday (&now, 0);

	timersub (&now, &last_mmc_step, &diff);
	diff_usecs = diff.tv_sec * 1000000 + diff.tv_usec;

	if (diff_usecs > 1000000.0 || fabs (_transport_speed) < 0.0000001) {
		/* too long or too slow, stop transport */
		request_transport_speed (0.0);
		step_queued = false;
		return false;
	}

	if (diff_usecs < 250000.0) {
		/* too short, just keep going */
		return true;
	}

	/* slow it down */

	request_transport_speed (_transport_speed * 0.75);
	return true;
}

/*---------------------------------------------------------------------------
  MIDI THREAD
  ---------------------------------------------------------------------------*/

int
Session::start_midi_thread ()
{
	midi_control_ui = new MidiControlUI (*this);
	midi_control_ui->run ();
	return 0;
}

void
Session::terminate_midi_thread ()
{
	if (midi_control_ui) {
		midi_control_ui->quit ();
	}
}


