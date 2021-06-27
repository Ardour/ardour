/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2018 John Emmas <john@creativepost.co.uk>
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
#include <iostream>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "pbd/error.h"
#include "pbd/pthread_utils.h"

#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_port.h"
#include "ardour/session.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace MIDI;
using namespace PBD;
using namespace Timecode;

/* length (in timecode frames) of the "window" that we consider legal given receipt of
   a given timecode position. Ardour will try to chase within this window, and will
   stop+locate+wait+chase if timecode arrives outside of it. The window extends entirely
   in the current direction of motion, so if any timecode arrives that is before the most
   recently received position (and without the direction of timecode reversing too), we
   will stop+locate+wait+chase.
*/
const int MTC_TransportMaster::sample_tolerance = 2;

MTC_TransportMaster::MTC_TransportMaster (std::string const & name)
	: TimecodeTransportMaster (name, MTC)
	, can_notify_on_unknown_rate (true)
	, mtc_frame (0)
	, mtc_frame_dll (0)
	, last_inbound_frame (0)
	, window_begin (0)
	, window_end (0)
	, first_mtc_timestamp (0)
	, reset_pending (0)
	, reset_position (false)
	, transport_direction (1)
	, busy_guard1 (0)
	, busy_guard2 (0)
{
	init ();
}

MTC_TransportMaster::~MTC_TransportMaster()
{
	port_connections.drop_connections();
}

void
MTC_TransportMaster::init ()
{
	reset (true);
	resync_latency (false);
}

void
MTC_TransportMaster::connection_handler (boost::weak_ptr<ARDOUR::Port> w0, std::string n0, boost::weak_ptr<ARDOUR::Port> w1, std::string n1, bool con) 
{
	TransportMaster::connection_handler(w0, n0, w1, n1, con);

	boost::shared_ptr<Port> p = w1.lock ();
	if (p == _port) {
		resync_latency (false);
	}
}

void
MTC_TransportMaster::create_port ()
{
	if ((_port = create_midi_port (string_compose ("%1 in", _name))) == 0) {
		throw failed_constructor();
	}
}

void
MTC_TransportMaster::set_session (Session* s)
{
	TransportMaster::set_session (s);
	TransportMasterViaMIDI::set_session (s);

	port_connections.drop_connections();

	if (_session) {

		last_mtc_fps_byte = _session->get_mtc_timecode_bits ();
		quarter_frame_duration = (double) (_session->samples_per_timecode_frame() / 4.0);
		mtc_timecode = _session->config.get_timecode_format();

		parse_timecode_offset ();
		reset (true);

		parser.mtc_time.connect_same_thread (port_connections,  boost::bind (&MTC_TransportMaster::update_mtc_time, this, _1, _2, _3));
		parser.mtc_qtr.connect_same_thread (port_connections, boost::bind (&MTC_TransportMaster::update_mtc_qtr, this, _1, _2, _3));
		parser.mtc_status.connect_same_thread (port_connections, boost::bind (&MTC_TransportMaster::update_mtc_status, this, _1));
	}
}

void
MTC_TransportMaster::pre_process (MIDI::pframes_t nframes, samplepos_t now, boost::optional<samplepos_t> session_pos)
{
	/* Read and parse incoming MIDI */

	maybe_reset ();

	if (!_midi_port) {
		_current_delta = 0;
		DEBUG_TRACE (DEBUG::MTC, "No MTC port registered");
		return;
	}

	_midi_port->read_and_parse_entire_midi_buffer_with_no_speed_adjustment (nframes, parser, now);

	if (session_pos) {
		const samplepos_t current_pos = current.position + ((now - current.timestamp) * current.speed);
		_current_delta = current_pos - *session_pos;
	} else {
		_current_delta = 0;
	}
}

void
MTC_TransportMaster::parse_timecode_offset() {
	Timecode::Time offset_tc;
	Timecode::parse_timecode_format (_session->config.get_slave_timecode_offset(), offset_tc);
	offset_tc.rate = _session->timecode_frames_per_second();
	offset_tc.drop = _session->timecode_drop_frames();
	_session->timecode_to_sample(offset_tc, timecode_offset, false, false);
	timecode_negative_offset = offset_tc.negative;
}

void
MTC_TransportMaster::parameter_changed (std::string const & p)
{
	if (p == "slave-timecode-offset"
			|| p == "timecode-format"
			) {
		parse_timecode_offset();
	}
}

ARDOUR::samplecnt_t
MTC_TransportMaster::update_interval() const
{
	if (timecode.rate) {
		return AudioEngine::instance()->sample_rate() / timecode.rate;
	}

	return AudioEngine::instance()->sample_rate(); /* useless but what other answer is there? */
}

ARDOUR::samplecnt_t
MTC_TransportMaster::resolution () const
{
	return (samplecnt_t) quarter_frame_duration * 4.0;
}

ARDOUR::samplecnt_t
MTC_TransportMaster::seekahead_distance () const
{
	return quarter_frame_duration * 8 * transport_direction;
}

bool
MTC_TransportMaster::outside_window (samplepos_t pos) const
{
	return ((pos < window_begin) || (pos > window_end));
}


bool
MTC_TransportMaster::locked () const
{
	DEBUG_TRACE (DEBUG::MTC, string_compose ("locked ? %1 last %2\n", parser.mtc_locked(), last_inbound_frame));
	return parser.mtc_locked() && last_inbound_frame !=0;
}

bool
MTC_TransportMaster::ok() const
{
	return true;
}

void
MTC_TransportMaster::queue_reset (bool reset_pos)
{
	Glib::Threads::Mutex::Lock lm (reset_lock);
	reset_pending++;
	if (reset_pos) {
		reset_position = true;
	}
}

void
MTC_TransportMaster::maybe_reset ()
{
	Glib::Threads::Mutex::Lock lm (reset_lock);

	if (reset_pending) {
		reset (reset_position);
		reset_pending = 0;
		reset_position = false;
	}
}

void
MTC_TransportMaster::reset (bool with_position)
{
	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC_TransportMaster reset %1\n", with_position?"with position":"without position"));

	if (with_position) {
		current.update (current.position, 0, 0);
	} else {
		current.reset ();
	}
	first_mtc_timestamp = 0;
	window_begin = 0;
	window_end = 0;
	transport_direction = 1;
	_current_delta = 0;
	timecode_format_valid = false;
}

void
MTC_TransportMaster::handle_locate (const MIDI::byte* mmc_tc)
{
	MIDI::byte mtc[5];
	DEBUG_TRACE (DEBUG::MTC, "MTC_TransportMaster::handle_locate\n");

	mtc[4] = last_mtc_fps_byte;
	mtc[3] = mmc_tc[0] & 0xf; /* hrs only */
	mtc[2] = mmc_tc[1];
	mtc[1] = mmc_tc[2];
	mtc[0] = mmc_tc[3];

	update_mtc_time (mtc, true, 0);
}

void
MTC_TransportMaster::init_mtc_dll(samplepos_t tme, double qtr)
{
	const double omega = 2.0 * M_PI * qtr / 2.0 / double(_session->sample_rate());
	b = 1.4142135623730950488 * omega;
	c = omega * omega;

	e2 = qtr;
	t0 = double(tme);
	t1 = t0 + e2;
	DEBUG_TRACE (DEBUG::MTC, string_compose ("[re-]init MTC DLL %1 %2 %3\n", t0, t1, e2));
}

/* called from MIDI parser */
void
MTC_TransportMaster::update_mtc_qtr (Parser& p, int which_qtr, samplepos_t now)
{
	busy_guard1++;
	const double qtr_d = quarter_frame_duration;

	mtc_frame_dll += qtr_d * (double) transport_direction;
	mtc_frame = rint(mtc_frame_dll);

	DEBUG_TRACE (DEBUG::MTC, string_compose ("qtr sample %1 at %2 -> mtc_frame: %3\n", which_qtr, now, mtc_frame));

	double mtc_speed = 0;
	if (first_mtc_timestamp != 0) {
		/* update MTC DLL and calculate speed */
		const double e = mtc_frame_dll - (double)transport_direction * ((double)now - (double)current.timestamp + t0);
		t0 = t1;
		t1 += b * e + e2;
		e2 += c * e;

		mtc_speed = (t1 - t0) / qtr_d;
		DEBUG_TRACE (DEBUG::MTC, string_compose ("qtr sample DLL t0:%1 t1:%2 err:%3 spd:%4 ddt:%5\n", t0, t1, e, mtc_speed, e2 - qtr_d));

		current.update (mtc_frame, now, mtc_speed);

		last_inbound_frame = now;
	}

	maybe_reset ();

	busy_guard2++;
}

/* called from MIDI parser _after_ update_mtc_qtr()
 * when a full TC has been received
 * OR on locate */
void
MTC_TransportMaster::update_mtc_time (const MIDI::byte *msg, bool was_full, samplepos_t now)
{
	busy_guard1++;

	/* "now" can be zero if this is called from a context where we do not have or do not want
	   to use a timestamp indicating when this MTC time was received. example: when we received
	   a locate command via MMC.
	*/
	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC::update_mtc_time - TID:%1\n", pthread_name()));
	TimecodeFormat tc_format;
	bool have_tc = true;

	timecode.hours = msg[3];
	timecode.minutes = msg[2];
	timecode.seconds = msg[1];
	timecode.frames = msg[0];

	last_mtc_fps_byte = msg[4];

	DEBUG_TRACE (DEBUG::MTC, string_compose ("full mtc time known at %1, full ? %2\n", now, was_full));

	if (now) {
		maybe_reset ();
	}

	switch (msg[4]) {
	case MTC_24_FPS:
		timecode.rate = 24;
		timecode.drop = false;
		tc_format = timecode_24;
		can_notify_on_unknown_rate = true;
		break;
	case MTC_25_FPS:
		timecode.rate = 25;
		timecode.drop = false;
		tc_format = timecode_25;
		can_notify_on_unknown_rate = true;
		break;
	case MTC_30_FPS_DROP:
		if (fr2997()) {
			tc_format = Timecode::timecode_2997000drop;
			timecode.rate = (29970.0/1000.0);
		} else {
			tc_format = timecode_2997drop;
			timecode.rate = (30000.0/1001.0);
		}
		timecode.drop = true;
		can_notify_on_unknown_rate = true;
		break;
	case MTC_30_FPS:
		timecode.rate = 30;
		timecode.drop = false;
		can_notify_on_unknown_rate = true;
		tc_format = timecode_30;
		break;
	default:
		/* throttle error messages about unknown MTC rates */
		if (can_notify_on_unknown_rate) {
			error << string_compose (_("Unknown rate/drop value %1 in incoming MTC stream, session values used instead"),
						 (int) msg[4])
			      << endmsg;
			can_notify_on_unknown_rate = false;
		}
		timecode.rate = _session->timecode_frames_per_second();
		timecode.drop = _session->timecode_drop_frames();
		have_tc = false;
	}

	if (have_tc) {
		mtc_timecode = tc_format;
		timecode_format_valid = true; /* SET FLAG */
	}

	/* do a careful conversion of the timecode value to a position
	   so that we take drop/nondrop and all that nonsense into
	   consideration.
	*/

	quarter_frame_duration = (double(_session->sample_rate()) / (double) timecode.rate / 4.0);

	Timecode::timecode_to_sample (timecode, mtc_frame, true, false,
		double(_session->sample_rate()),
		_session->config.get_subframes_per_frame(),
		timecode_negative_offset, timecode_offset
		);

	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC at %1 TC %2 = mtc_frame %3 (from full message ? %4)\n", now, timecode, mtc_frame, was_full));

	if (was_full || outside_window (mtc_frame)) {
		DEBUG_TRACE (DEBUG::MTC, string_compose ("update_mtc_time: full TC %1 or outside window %2 MTC %3\n", was_full, outside_window (mtc_frame), mtc_frame));
		boost::shared_ptr<TransportMaster> c = TransportMasterManager::instance().current();
		if (c && c.get() == this && _session->config.get_external_sync()) {
			_session->set_requested_return_sample (-1);
			_session->request_locate (mtc_frame, MustStop, TRS_MTC);
		}
		update_mtc_status (MIDI::MTC_Stopped);
		reset (false);
		reset_window (mtc_frame);
	} else {

		/* we've had the first set of 8 qtr sample messages, determine position
		   and allow continuing qtr sample messages to provide position
		   and speed information.
		*/

		/* We received the last quarter frame 7 quarter frames (1.75 mtc
		   samples) after the instance when the contents of the mtc quarter
		   samples were decided. Add time to compensate for the elapsed 1.75
		   samples.
		*/
		double qtr = quarter_frame_duration;
		long int mtc_off = (long) rint(7.0 * qtr);

		DEBUG_TRACE (DEBUG::MTC, string_compose ("new mtc_frame: %1 | MTC-FpT: %2 A3-FpT:%3\n",
							 mtc_frame, (4.0*qtr), _session->samples_per_timecode_frame()));

		switch (parser.mtc_running()) {
		case MTC_Backward:
			mtc_frame -= mtc_off;
			qtr *= -1.0;
			break;
		case MTC_Forward:
			mtc_frame += mtc_off;
			break;
		default:
			break;
		}

		DEBUG_TRACE (DEBUG::MTC, string_compose ("new mtc_frame (w/offset) = %1\n", mtc_frame));

		if (now) {
			if (first_mtc_timestamp == 0 || current.timestamp == 0) {
				first_mtc_timestamp = now;
				init_mtc_dll(mtc_frame, qtr);
				mtc_frame_dll = mtc_frame + midi_port_latency.max;
			}
			current.update (mtc_frame + midi_port_latency.max, now, current.speed);
			reset_window (mtc_frame);
		}
	}

	busy_guard2++;
}

void
MTC_TransportMaster::update_mtc_status (MIDI::MTC_Status status)
{
	/* XXX !!! thread safety ... called from MIDI I/O context
	 * on locate (via ::update_mtc_time())
	 */
	DEBUG_TRACE (DEBUG::MTC, string_compose("MTC_TransportMaster::update_mtc_status - TID:%1 MTC:%2\n", pthread_name(), mtc_frame));
	return; // why was this fn needed anyway ? it just messes up things -> use reset.
	busy_guard1++;

	switch (status) {
	case MTC_Stopped:
		current.update (mtc_frame, 0, 0);
		break;

	case MTC_Forward:
		current.update (mtc_frame, 0, 0);
		break;

	case MTC_Backward:
		current.update (mtc_frame, 0, 0);
		break;
	}
	busy_guard2++;
}

void
MTC_TransportMaster::reset_window (samplepos_t root)
{
	/* if we're waiting for the master to catch us after seeking ahead, keep the window
	   of acceptable MTC samples wide open. otherwise, shrink it down to just 2 video frames
	   ahead of the window root (taking direction into account).
	*/

	samplecnt_t const d = (quarter_frame_duration * 4 * sample_tolerance);

	switch (parser.mtc_running()) {
	case MTC_Forward:
		window_begin = root;
		transport_direction = 1;
		window_end = root + d;
		break;

	case MTC_Backward:
		transport_direction = -1;
		if (root > d) {
			window_begin = root - d;
			window_end = root;
		} else {
			window_begin = 0;
		}
		window_end = root;
		break;

	default:
		/* do nothing */
		break;
	}

	DEBUG_TRACE (DEBUG::MTC, string_compose ("reset MTC window @ %3, now %1 .. %2\n", window_begin, window_end, root));
}

Timecode::TimecodeFormat
MTC_TransportMaster::apparent_timecode_format () const
{
	return mtc_timecode;
}

std::string
MTC_TransportMaster::position_string() const
{
	SafeTime last;
	current.safe_read (last);
	if (last.timestamp == 0 || reset_pending) {
		return " --:--:--:--";
	}
	return Timecode::timecode_format_sampletime(
		last.position,
		double(_session->sample_rate()),
		Timecode::timecode_to_frames_per_second(mtc_timecode),
		Timecode::timecode_has_drop_frames(mtc_timecode));
}

std::string
MTC_TransportMaster::delta_string () const
{
	SafeTime last;
	current.safe_read (last);

	if (last.timestamp == 0 || reset_pending) {
		return X_("\u2012\u2012\u2012\u2012");
	} else {
		return format_delta_time (_current_delta);
	}
}

void
MTC_TransportMaster::unregister_port ()
{
	_midi_port.reset ();
	TransportMaster::unregister_port ();
}
