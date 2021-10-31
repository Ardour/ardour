/*
 * Copyright (C) 2005-2009 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013 Michael Fisher <mfisher31@gmail.com>
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

#include <string>
#include <cmath>
#include <cerrno>
#include <cassert>
#include <unistd.h>

#include <boost/shared_ptr.hpp>

#include <glibmm/main.h>

#include "midi++/mmc.h"
#include "midi++/port.h"

#include "pbd/error.h"
#include "pbd/pthread_utils.h"
#include "pbd/timersub.h"

#include "temporal/time.h"

#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/midi_port.h"
#include "ardour/midi_track.h"
#include "ardour/midi_ui.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/transport_master.h"
#include "ardour/transport_fsm.h"
#include "ardour/ticker.h"

#include "pbd/i18n.h"

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
Session::spp_start ()
{
	if (Config->get_mmc_control ()) {
		request_roll (TRS_MIDIClock);
	}
}

void
Session::spp_continue ()
{
	spp_start ();
}

void
Session::spp_stop ()
{
	if (Config->get_mmc_control ()) {
		request_stop ();
	}
}

void
Session::mmc_deferred_play (MIDI::MachineControl &/*mmc*/)
{
	if (Config->get_mmc_control ()) {
		request_roll (TRS_MMC);
	}
}

void
Session::mmc_record_pause (MIDI::MachineControl &/*mmc*/)
{
	if (Config->get_mmc_control ()) {
		maybe_enable_record();
	}
}

void
Session::mmc_record_strobe (MIDI::MachineControl &/*mmc*/)
{
	if (!Config->get_mmc_control() || (_step_editors > 0)) {
		return;
	}

	/* record strobe does an implicit "Play" command */

	if (_transport_fsm->transport_speed() != 1.0) {

		/* start_transport() will move from Enabled->Recording, so we
		   don't need to do anything here except enable recording.
		   its not the same as maybe_enable_record() though, because
		   that *can* switch to Recording, which we do not want.
		*/

		save_state ("", true);
		g_atomic_int_set (&_record_status, Enabled);
		RecordStateChanged (); /* EMIT SIGNAL */

		request_roll (TRS_MMC);

	} else {

		enable_record ();
	}
}

void
Session::mmc_record_exit (MIDI::MachineControl &/*mmc*/)
{
	if (Config->get_mmc_control ()) {
		disable_record (false);
	}
}

void
Session::mmc_stop (MIDI::MachineControl &/*mmc*/)
{
	if (Config->get_mmc_control ()) {
		request_stop ();
	}
}

void
Session::mmc_pause (MIDI::MachineControl &/*mmc*/)
{
	if (Config->get_mmc_control ()) {

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
	if (!Config->get_mmc_control ()) {
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

	if (_transport_fsm->transport_speed() == 0 || cur_speed * _transport_fsm->transport_speed() < 0) {
		/* change direction */
		step_speed = cur_speed;
	} else {
		step_speed = (0.6 * step_speed) + (0.4 * cur_speed);
	}

	step_speed *= 0.25;

#if 0
	cerr << "delta = " << diff_secs
	     << " ct = " << _transport_fsm->transport_speed()
	     << " steps = " << steps
	     << " new speed = " << cur_speed
	     << " speed = " << step_speed
	     << endl;
#endif

	request_transport_speed_nonzero (step_speed);
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
	if (Config->get_mmc_control ()) {
		request_transport_speed(-8.0f);
	}
}

void
Session::mmc_fast_forward (MIDI::MachineControl &/*mmc*/)
{
	if (Config->get_mmc_control ()) {
		request_transport_speed(8.0f);
	}
}

void
Session::mmc_locate (MIDI::MachineControl &/*mmc*/, const MIDI::byte* mmc_tc)
{
	if (!Config->get_mmc_control ()) {
		return;
	}

	samplepos_t target_sample;
	Timecode::Time timecode;

	timecode.hours = mmc_tc[0] & 0xf;
	timecode.minutes = mmc_tc[1];
	timecode.seconds = mmc_tc[2];
	timecode.frames = mmc_tc[3];
	timecode.rate = timecode_frames_per_second();
	timecode.drop = timecode_drop_frames();

	// Also takes timecode offset into account:
	timecode_to_sample( timecode, target_sample, true /* use_offset */, false /* use_subframes */ );

	if (target_sample > max_samplepos) {
		target_sample = max_samplepos;
	}

	/* Some (all?) MTC/MMC devices do not send a full MTC frame
	   at the end of a locate, instead sending only an MMC
	   locate command. This causes the current position
	   of an MTC slave to become out of date. Catch this.
	*/

	boost::shared_ptr<MTC_TransportMaster> mtcs = boost::dynamic_pointer_cast<MTC_TransportMaster> (transport_master());

	if (mtcs) {
		// cerr << "Locate *with* MTC slave\n";
		mtcs->handle_locate (mmc_tc);
	} else {
		// cerr << "Locate without MTC slave\n";
		request_locate (target_sample, MustStop);
	}
}

void
Session::mmc_shuttle (MIDI::MachineControl &/*mmc*/, float speed, bool forw)
{
	if (!Config->get_mmc_control ()) {
		return;
	}

	if (Config->get_shuttle_speed_threshold() >= 0 && speed > Config->get_shuttle_speed_threshold()) {
		speed *= Config->get_shuttle_speed_factor();
	}

	if (forw) {
		request_transport_speed_nonzero (speed);
	} else {
		request_transport_speed_nonzero (-speed);
	}
}

boost::shared_ptr<Route>
Session::get_midi_nth_route_by_id (PresentationInfo::order_t n) const
{
	PresentationInfo::Flag f;

	/* These numbers are defined by the MMC specification.
	 */

	if (n == 318) {
		f = PresentationInfo::MasterOut;
	} else if (n == 319) {
		f = PresentationInfo::MonitorOut;
	} else {
		f = PresentationInfo::Route;
	}

	boost::shared_ptr<RouteList> r = routes.reader ();
	PresentationInfo::order_t match_cnt = 0;

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->presentation_info().flag_match (f)) {
			if (match_cnt++ == n) {
				return *i;
			}
		}
	}

	return boost::shared_ptr<Route>();
}

void
Session::mmc_record_enable (MIDI::MachineControl &mmc, size_t trk, bool enabled)
{
	if (!Config->get_mmc_control ()) {
		return;
	}

	boost::shared_ptr<Route> r = get_midi_nth_route_by_id (trk);

	if (r) {
		boost::shared_ptr<AudioTrack> at;

		if ((at = boost::dynamic_pointer_cast<AudioTrack> (r))) {
			at->rec_enable_control()->set_value (enabled, Controllable::UseGroup);
		}
	}
}

void
Session::mtc_tx_resync_latency (bool playback)
{
	if (deletion_in_progress() || !playback) {
		return;
	}
	boost::shared_ptr<Port> mtxport = _midi_ports->mtc_output_port ();
	if (mtxport) {
		mtxport->get_connected_latency_range(mtc_out_latency, true);
		DEBUG_TRACE (DEBUG::MTC, string_compose ("resync latency: %1\n", mtc_out_latency.max));
	}
}

/** Send MTC Full Frame message (complete Timecode time) for the start of this cycle.
 * This resets the MTC code, the next quarter frame message that is sent will be
 * the first one with the beginning of this cycle as the new start point.
 * @param t time to send.
 */
int
Session::send_full_time_code (samplepos_t const t, MIDI::pframes_t nframes)
{
	/* This function could easily send at a given sample offset, but would
	 * that be useful?  Does ardour do sub-block accurate locating? [DR] */

	MIDI::byte msg[10];
	Timecode::Time timecode;

	_send_timecode_update = false;

	if (_engine.freewheeling() || !Config->get_send_mtc()) {
		return 0;
	}

	if (transport_master_is_external() && !transport_master()->locked()) {
		return 0;
	}

	// Get timecode time for the given time
	sample_to_timecode (t, timecode, true /* use_offset */, false /* no subframes */);

	// sample-align outbound to rounded (no subframes) timecode
	samplepos_t mtc_tc;
	timecode_to_sample(timecode, mtc_tc, true, false);
	outbound_mtc_timecode_frame = mtc_tc;
	transmitting_timecode_time = timecode;

	sampleoffset_t mtc_offset = mtc_out_latency.max;

	// only if rolling.. ?
	outbound_mtc_timecode_frame += mtc_offset;

	// outbound_mtc_timecode_frame needs to be >= _transport_sample
	// or a new full timecode will be queued next cycle.
	while (outbound_mtc_timecode_frame < t) {
		Timecode::increment (transmitting_timecode_time, config.get_subframes_per_frame());
		outbound_mtc_timecode_frame += _samples_per_timecode_frame;
	}

	double const quarter_frame_duration = ((samplecnt_t) _samples_per_timecode_frame) / 4.0;
	if (ceil((t - mtc_tc) / quarter_frame_duration) > 0) {
		Timecode::increment (transmitting_timecode_time, config.get_subframes_per_frame());
		outbound_mtc_timecode_frame += _samples_per_timecode_frame;
	}

	DEBUG_TRACE (DEBUG::MTC, string_compose ("Full MTC TC %1 (off %2)\n", outbound_mtc_timecode_frame, mtc_offset));

	/* according to MTC spec 24, 30 drop and 30 non-drop TC, the frame-number represented by 8 quarter frames must be even. */
	if (((mtc_timecode_bits >> 5) != MIDI::MTC_25_FPS) && (transmitting_timecode_time.frames % 2)) {
		/* start MTC quarter frame transmission on an even frame */
		Timecode::increment (transmitting_timecode_time, config.get_subframes_per_frame());
		outbound_mtc_timecode_frame += _samples_per_timecode_frame;
	}

	next_quarter_frame_to_send = 0;

	// Sync slave to the same Timecode time as we are on
	msg[0] = 0xf0;
	msg[1] = 0x7f;
	msg[2] = 0x7f;
	msg[3] = 0x1;
	msg[4] = 0x1;
	msg[9] = 0xf7;

	msg[5] = mtc_timecode_bits | (timecode.hours % 24);
	msg[6] = timecode.minutes;
	msg[7] = timecode.seconds;
	msg[8] = timecode.frames;

	// Send message at offset 0, sent time is for the start of this cycle

	MidiBuffer& mb (_midi_ports->mtc_output_port()->get_midi_buffer (nframes));
	mb.push_back (0, Evoral::MIDI_EVENT, sizeof (msg), msg);

	_pframes_since_last_mtc = 0;
	return 0;
}

/** Send MTC (quarter-frame) messages for this cycle.
 * Must be called exactly once per cycle from the process thread.  Realtime safe.
 * This function assumes the state of full Timecode is sane, eg. the slave is
 * expecting quarter frame messages and has the right frame of reference (any
 * full MTC Timecode time messages that needed to be sent should have been sent
 * earlier already this cycle by send_full_time_code)
 */
int
Session::send_midi_time_code_for_cycle (samplepos_t start_sample, samplepos_t end_sample, ARDOUR::pframes_t nframes)
{
	// start_sample == start_sample  for normal cycles
	// start_sample > _transport_sample  for split cycles
	if (_engine.freewheeling() || !_send_qf_mtc || transmitting_timecode_time.negative || (next_quarter_frame_to_send < 0)) {
		// cerr << "(MTC) Not sending MTC\n";
		return 0;
	}
	if (transport_master_is_external() && !transport_master()->locked()) {
		return 0;
	}

	if (_transport_fsm->transport_speed() < 0) {
		// we don't support rolling backwards
		return 0;
	}

	/* MTC is max. 30 fps - assert() below will fail
	 * TODO actually limit it to 24,25,29df,30fps
	 * talk to oofus, first.
	 */
	if (Timecode::timecode_to_frames_per_second(config.get_timecode_format()) > 30) {
		return 0;
	}

	assert (next_quarter_frame_to_send >= 0);
	assert (next_quarter_frame_to_send <= 7);

	/* Duration of one quarter frame */
	double const quarter_frame_duration = _samples_per_timecode_frame / 4.0;

	DEBUG_TRACE (DEBUG::MTC, string_compose ("TF %1 SF %2 MT %3 QF %4 QD %5\n",
	                                         _transport_sample, start_sample, outbound_mtc_timecode_frame,
	                                         next_quarter_frame_to_send, quarter_frame_duration));

	if (rint(outbound_mtc_timecode_frame + (next_quarter_frame_to_send * quarter_frame_duration)) < _transport_sample) {
		// send full timecode and set outbound_mtc_timecode_frame, next_quarter_frame_to_send
		send_full_time_code (_transport_sample, nframes);
	}

	if (rint(outbound_mtc_timecode_frame + (next_quarter_frame_to_send * quarter_frame_duration)) < start_sample) {
		// no QF for this cycle
		return 0;
	}

	/* Send quarter frames for this cycle */
	while (end_sample > rint(outbound_mtc_timecode_frame + (next_quarter_frame_to_send * quarter_frame_duration))) {

		DEBUG_TRACE (DEBUG::MTC, string_compose ("next sample to send: %1\n", next_quarter_frame_to_send));

		switch (next_quarter_frame_to_send) {
		case 0:
			mtc_msg[1] = 0x00 | (transmitting_timecode_time.frames & 0xf);
			break;
		case 1:
			mtc_msg[1] = 0x10 | ((transmitting_timecode_time.frames & 0xf0) >> 4);
			break;
		case 2:
			mtc_msg[1] = 0x20 | (transmitting_timecode_time.seconds & 0xf);
			break;
		case 3:
			mtc_msg[1] = 0x30 | ((transmitting_timecode_time.seconds & 0xf0) >> 4);
			break;
		case 4:
			mtc_msg[1] = 0x40 | (transmitting_timecode_time.minutes & 0xf);
			break;
		case 5:
			mtc_msg[1] = 0x50 | ((transmitting_timecode_time.minutes & 0xf0) >> 4);
			break;
		case 6:
			mtc_msg[1] = 0x60 | ((mtc_timecode_bits | transmitting_timecode_time.hours) & 0xf);
			break;
		case 7:
			mtc_msg[1] = 0x70 | (((mtc_timecode_bits | transmitting_timecode_time.hours) & 0xf0) >> 4);
			break;
		}

		const samplepos_t msg_time = rint (outbound_mtc_timecode_frame + (quarter_frame_duration * next_quarter_frame_to_send));

		// This message must fall within this block or something is broken
		assert (msg_time >= start_sample);
		assert (msg_time < end_sample);

		/* convert from session samples back to JACK samples using the transport speed */
		ARDOUR::pframes_t const out_stamp = (msg_time - start_sample) / _transport_fsm->transport_speed();
		assert (out_stamp < nframes);

		MidiBuffer& mb (_midi_ports->mtc_output_port()->get_midi_buffer(nframes));
		if (!mb.push_back (out_stamp, Evoral::MIDI_EVENT, 2, mtc_msg)) {
			error << string_compose(_("Session: cannot send quarter-frame MTC message (%1)"), strerror (errno))
			      << endmsg;
			return -1;
		}

#ifndef NDEBUG
		if (DEBUG_ENABLED(DEBUG::MTC)) {
			DEBUG_STR_DECL(foo);
			DEBUG_STR_APPEND(foo,"sending ");
			DEBUG_STR_APPEND(foo, transmitting_timecode_time);
			DEBUG_TRACE (DEBUG::MTC, string_compose ("%1 qfm = %2, stamp = %3\n", DEBUG_STR(foo).str(), next_quarter_frame_to_send,
			                                         out_stamp));
		}
#endif

		// Increment quarter frame counter
		next_quarter_frame_to_send++;

		if (next_quarter_frame_to_send >= 8) {
			// Wrap quarter frame counter
			next_quarter_frame_to_send = 0;
			// Increment timecode time twice
			Timecode::increment (transmitting_timecode_time, config.get_subframes_per_frame());
			Timecode::increment (transmitting_timecode_time, config.get_subframes_per_frame());
			// Increment timing of first quarter frame
			outbound_mtc_timecode_frame += 2.0 * _samples_per_timecode_frame;
		}
	}

	return 0;
}

/***********************************************************************
 OUTBOUND MMC STUFF
**********************************************************************/

void
Session::send_immediate_mmc (MachineControlCommand c)
{
	_mmc->send (c, 0);
}

bool
Session::mmc_step_timeout ()
{
	struct timeval now;
	struct timeval diff;
	double diff_usecs;
	gettimeofday (&now, 0);

	timersub (&now, &last_mmc_step, &diff);
	diff_usecs = diff.tv_sec * 1000000 + diff.tv_usec;

	if (diff_usecs > 1000000.0 || fabs (_transport_fsm->transport_speed()) < 0.0000001) {
		/* too long or too slow, stop transport */
		request_stop ();
		step_queued = false;
		return false;
	}

	if (diff_usecs < 250000.0) {
		/* too short, just keep going */
		return true;
	}

	/* slow it down */

	request_transport_speed_nonzero (actual_speed() * 0.75);
	return true;
}

/* *********************************************************************
 * OUTBOUND SYSTEM COMMON STUFF
 **********************************************************************/

void
Session::send_song_position_pointer (samplepos_t)
{
	if (midi_clock) {
		/* Do nothing for the moment */
	}
}

int
Session::start_midi_thread ()
{
	if (midi_control_ui) { return 0; }
	midi_control_ui = new MidiControlUI (*this);
	midi_control_ui->run ();
	return 0;
}

boost::shared_ptr<ARDOUR::Port>
Session::mmc_output_port () const
{
	return _midi_ports->mmc_output_port ();
}

boost::shared_ptr<ARDOUR::Port>
Session::mmc_input_port () const
{
	return _midi_ports->mmc_input_port ();
}

boost::shared_ptr<ARDOUR::Port>
Session::scene_output_port () const
{
	return _midi_ports->scene_output_port ();
}

boost::shared_ptr<ARDOUR::Port>
Session::scene_input_port () const
{
	return _midi_ports->scene_input_port ();
}

boost::shared_ptr<AsyncMIDIPort>
Session::vkbd_output_port () const
{
	return _midi_ports->vkbd_output_port ();
}

boost::shared_ptr<MidiPort>
Session::midi_clock_output_port () const
{
	return _midi_ports->midi_clock_output_port ();
}

boost::shared_ptr<MidiPort>
Session::mtc_output_port () const
{
	return _midi_ports->mtc_output_port ();
}

void
Session::midi_track_presentation_info_changed (PropertyChange const& what_changed, boost::weak_ptr<MidiTrack> mt)
{
	if (!Config->get_midi_input_follows_selection()) {
		return;
	}

	if (!what_changed.contains (Properties::selected)) {
		return;
	}

	boost::shared_ptr<MidiTrack> new_midi_target (mt.lock ());

	if (new_midi_target->is_selected()) {
		rewire_selected_midi (new_midi_target);
	}
}


void
Session::disconnect_port_for_rewire (std::string const& port) const
{
	MidiPortFlags mpf = AudioEngine::instance()->midi_port_metadata (port);

	/* if a port is marked for control data, do not
	 * disconnect it from everything since it may also be
	 * used via a control surface or some other
	 * functionality.
	 */
	bool keep_ctrl = mpf & MidiPortControl;

	vector<string> port_connections;
	AudioEngine::instance()->get_connections (port, port_connections);
	for (vector<string>::iterator i = port_connections.begin(); i != port_connections.end(); ++i) {

		/* test if (*i) is a control-surface input port */
		if (keep_ctrl && AudioEngine::instance()->port_is_control_only (*i)) {
			continue;
		}
		/* retain connection to "physical_midi_input_monitor_enable" */
		if (AudioEngine::instance()->port_is_physical_input_monitor_enable (*i)) {
			continue;
		}

		AudioEngine::instance()->disconnect (port, *i);
	}
}

void
Session::rewire_selected_midi (boost::shared_ptr<MidiTrack> new_midi_target)
{
	if (!new_midi_target) {
		return;
	}

	boost::shared_ptr<MidiTrack> old_midi_target = current_midi_target.lock ();

	if (new_midi_target == old_midi_target) {
		return;
	}

	vector<string> msp;
	AudioEngine::instance()->get_midi_selection_ports (msp);

	if (!msp.empty()) {
		for (vector<string>::const_iterator p = msp.begin(); p != msp.end(); ++p) {
			/* disconnect port */
			disconnect_port_for_rewire (*p);
			/* connect it to the new target */
			new_midi_target->input()->connect (new_midi_target->input()->nth(0), (*p), this);
		}
	}

	current_midi_target = new_midi_target;
}

void
Session::rewire_midi_selection_ports ()
{
	if (!Config->get_midi_input_follows_selection()) {
		return;
	}

	boost::shared_ptr<MidiTrack> target = current_midi_target.lock();

	if (!target) {
		return;
	}

	vector<string> msp;
	AudioEngine::instance()->get_midi_selection_ports (msp);

	if (msp.empty()) {
		return;
	}

	target->input()->disconnect (this);

	for (vector<string>::const_iterator p = msp.begin(); p != msp.end(); ++p) {
		disconnect_port_for_rewire (*p);
		target->input()->connect (target->input()->nth (0), (*p), this);
	}
}
