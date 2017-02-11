/*
    Copyright (C) 2002-4 Paul Davis
    Overhaul 2012 Robin Gareus <robin@gareus.org>

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
#include "ardour/slave.h"

#include <glibmm/timer.h>

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
const int MTC_Slave::frame_tolerance = 2;

MTC_Slave::MTC_Slave (Session& s, MidiPort& p)
	: session (s)
	, port (&p)
{
	can_notify_on_unknown_rate = true;
	did_reset_tc_format = false;
	reset_pending = 0;
	reset_position = false;
	mtc_frame = 0;
	mtc_frame_dll = 0;
	engine_dll_initstate = 0;
	busy_guard1 = busy_guard2 = 0;

	last_mtc_fps_byte = session.get_mtc_timecode_bits ();
	quarter_frame_duration = (double(session.samples_per_timecode_frame()) / 4.0);

	mtc_timecode = session.config.get_timecode_format();
	a3e_timecode = session.config.get_timecode_format();
	printed_timecode_warning = false;

	session.config.ParameterChanged.connect_same_thread (config_connection, boost::bind (&MTC_Slave::parameter_changed, this, _1));
	parse_timecode_offset();
	reset (true);

	port->self_parser().mtc_time.connect_same_thread (port_connections,  boost::bind (&MTC_Slave::update_mtc_time, this, _1, _2, _3));
	port->self_parser().mtc_qtr.connect_same_thread (port_connections, boost::bind (&MTC_Slave::update_mtc_qtr, this, _1, _2, _3));
	port->self_parser().mtc_status.connect_same_thread (port_connections, boost::bind (&MTC_Slave::update_mtc_status, this, _1));
}

MTC_Slave::~MTC_Slave()
{
	port_connections.drop_connections();
	config_connection.disconnect();

	while (busy_guard1 != busy_guard2) {
		/* make sure MIDI parser is not currently calling any callbacks in here,
		 * else there's a segfault ahead!
		 *
		 * XXX this is called from jack rt-context :(
		 * TODO fix libs/ardour/session_transport.cc:1321 (delete _slave;)
		 */
		sched_yield();
	}

	if (did_reset_tc_format) {
		session.config.set_timecode_format (saved_tc_format);
	}
}

void
MTC_Slave::rebind (MidiPort& p)
{
	port_connections.drop_connections ();

	port = &p;

}

void
MTC_Slave::parse_timecode_offset() {
	Timecode::Time offset_tc;
	Timecode::parse_timecode_format(session.config.get_slave_timecode_offset(), offset_tc);
	offset_tc.rate = session.timecode_frames_per_second();
	offset_tc.drop = session.timecode_drop_frames();
	session.timecode_to_sample(offset_tc, timecode_offset, false, false);
	timecode_negative_offset = offset_tc.negative;
}

void
MTC_Slave::parameter_changed (std::string const & p)
{
	if (p == "slave-timecode-offset"
			|| p == "timecode-format"
			) {
		parse_timecode_offset();
	}
}

bool
MTC_Slave::give_slave_full_control_over_transport_speed() const
{
	return true; // DLL align to engine transport
	// return false; // for Session-level computed varispeed
}

ARDOUR::framecnt_t
MTC_Slave::resolution () const
{
	return (framecnt_t) quarter_frame_duration * 4.0;
}

ARDOUR::framecnt_t
MTC_Slave::seekahead_distance () const
{
	return quarter_frame_duration * 8 * transport_direction;
}

bool
MTC_Slave::outside_window (framepos_t pos) const
{
	return ((pos < window_begin) || (pos > window_end));
}


bool
MTC_Slave::locked () const
{
	DEBUG_TRACE (DEBUG::MTC, string_compose ("locked ? %1 last %2 initstate %3\n", port->self_parser().mtc_locked(), last_inbound_frame, engine_dll_initstate));
	return port->self_parser().mtc_locked() && last_inbound_frame !=0 && engine_dll_initstate !=0;
}

bool
MTC_Slave::ok() const
{
	return true;
}

void
MTC_Slave::queue_reset (bool reset_pos)
{
	Glib::Threads::Mutex::Lock lm (reset_lock);
	reset_pending++;
	if (reset_pos) {
		reset_position = true;
	}
}

void
MTC_Slave::maybe_reset ()
{
	Glib::Threads::Mutex::Lock lm (reset_lock);

	if (reset_pending) {
		reset (reset_position);
		reset_pending = 0;
		reset_position = false;
	}
}

void
MTC_Slave::reset (bool with_position)
{
	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC_Slave reset %1\n", with_position?"with position":"without position"));
	if (with_position) {
		last_inbound_frame = 0;
		current.guard1++;
		current.position = 0;
		current.timestamp = 0;
		current.speed = 0;
		current.guard2++;
	} else {
		last_inbound_frame = 0;
		current.guard1++;
		current.timestamp = 0;
		current.speed = 0;
		current.guard2++;
	}
	first_mtc_timestamp = 0;
	window_begin = 0;
	window_end = 0;
	transport_direction = 1;
	current_delta = 0;
	ActiveChanged(false);
}

void
MTC_Slave::handle_locate (const MIDI::byte* mmc_tc)
{
	MIDI::byte mtc[5];
	DEBUG_TRACE (DEBUG::MTC, "MTC_Slave::handle_locate\n");

	mtc[4] = last_mtc_fps_byte;
	mtc[3] = mmc_tc[0] & 0xf; /* hrs only */
	mtc[2] = mmc_tc[1];
	mtc[1] = mmc_tc[2];
	mtc[0] = mmc_tc[3];

	update_mtc_time (mtc, true, 0);
}

void
MTC_Slave::read_current (SafeTime *st) const
{
	int tries = 0;

	do {
		if (tries == 10) {
			error << _("MTC Slave: atomic read of current time failed, sleeping!") << endmsg;
			Glib::usleep (20);
			tries = 0;
		}
		*st = current;
		tries++;

	} while (st->guard1 != st->guard2);
}

void
MTC_Slave::init_mtc_dll(framepos_t tme, double qtr)
{
	omega = 2.0 * M_PI * qtr / 2.0 / double(session.frame_rate());
	b = 1.4142135623730950488 * omega;
	c = omega * omega;

	e2 = qtr;
	t0 = double(tme);
	t1 = t0 + e2;
	DEBUG_TRACE (DEBUG::MTC, string_compose ("[re-]init MTC DLL %1 %2 %3\n", t0, t1, e2));
}

/* called from MIDI parser */
void
MTC_Slave::update_mtc_qtr (Parser& /*p*/, int which_qtr, framepos_t now)
{
	busy_guard1++;
	const double qtr_d = quarter_frame_duration;

	mtc_frame_dll += qtr_d * (double) transport_direction;
	mtc_frame = rint(mtc_frame_dll);

	DEBUG_TRACE (DEBUG::MTC, string_compose ("qtr frame %1 at %2 -> mtc_frame: %3\n", which_qtr, now, mtc_frame));

	double mtc_speed = 0;
	if (first_mtc_timestamp != 0) {
		/* update MTC DLL and calculate speed */
		const double e = mtc_frame_dll - (double)transport_direction * ((double)now - (double)current.timestamp + t0);
		t0 = t1;
		t1 += b * e + e2;
		e2 += c * e;

		mtc_speed = (t1 - t0) / qtr_d;
		DEBUG_TRACE (DEBUG::MTC, string_compose ("qtr frame DLL t0:%1 t1:%2 err:%3 spd:%4 ddt:%5\n", t0, t1, e, mtc_speed, e2 - qtr_d));

		current.guard1++;
		current.position = mtc_frame;
		current.timestamp = now;
		current.speed = mtc_speed;
		current.guard2++;

		last_inbound_frame = now;
	}

	maybe_reset ();

	busy_guard2++;
}

/* called from MIDI parser _after_ update_mtc_qtr()
 * when a full TC has been received
 * OR on locate */
void
MTC_Slave::update_mtc_time (const MIDI::byte *msg, bool was_full, framepos_t now)
{
	busy_guard1++;

	/* "now" can be zero if this is called from a context where we do not have or do not want
	   to use a timestamp indicating when this MTC time was received. example: when we received
	   a locate command via MMC.
	*/
	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC::update_mtc_time - TID:%1\n", pthread_name()));
	TimecodeFormat tc_format;
	bool reset_tc = true;

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
		if (Config->get_timecode_source_2997()) {
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
		timecode.rate = session.timecode_frames_per_second();
		timecode.drop = session.timecode_drop_frames();
		reset_tc = false;
	}

	if (reset_tc) {
		TimecodeFormat cur_timecode = session.config.get_timecode_format();
		if (Config->get_timecode_sync_frame_rate()) {
			/* enforce time-code */
			if (!did_reset_tc_format) {
				saved_tc_format = cur_timecode;
				did_reset_tc_format = true;
			}
			if (cur_timecode != tc_format) {
				if (ceil(Timecode::timecode_to_frames_per_second(cur_timecode)) != ceil(Timecode::timecode_to_frames_per_second(tc_format))) {
					warning << string_compose(_("Session framerate adjusted from %1 TO: MTC's %2."),
							Timecode::timecode_format_name(cur_timecode),
							Timecode::timecode_format_name(tc_format))
						<< endmsg;
				}
			}
			session.config.set_timecode_format (tc_format);
		} else {
			/* only warn about TC mismatch */
			if (mtc_timecode != tc_format) printed_timecode_warning = false;
			if (a3e_timecode != cur_timecode) printed_timecode_warning = false;

			if (cur_timecode != tc_format && ! printed_timecode_warning) {
				if (ceil(Timecode::timecode_to_frames_per_second(cur_timecode)) != ceil(Timecode::timecode_to_frames_per_second(tc_format))) {
					warning << string_compose(_("Session and MTC framerate mismatch: MTC:%1 %2:%3."),
								  Timecode::timecode_format_name(tc_format),
								  PROGRAM_NAME,
								  Timecode::timecode_format_name(cur_timecode))
						<< endmsg;
				}
				printed_timecode_warning = true;
			}
		}
		mtc_timecode = tc_format;
		a3e_timecode = cur_timecode;

		speedup_due_to_tc_mismatch = timecode.rate / Timecode::timecode_to_frames_per_second(a3e_timecode);
	}

	/* do a careful conversion of the timecode value to a position
	   so that we take drop/nondrop and all that nonsense into
	   consideration.
	*/

	quarter_frame_duration = (double(session.frame_rate()) / (double) timecode.rate / 4.0);

	Timecode::timecode_to_sample (timecode, mtc_frame, true, false,
		double(session.frame_rate()),
		session.config.get_subframes_per_frame(),
		timecode_negative_offset, timecode_offset
		);

	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC at %1 TC %2 = mtc_frame %3 (from full message ? %4) tc-ratio %5\n",
						 now, timecode, mtc_frame, was_full, speedup_due_to_tc_mismatch));

	if (was_full || outside_window (mtc_frame)) {
		DEBUG_TRACE (DEBUG::MTC, string_compose ("update_mtc_time: full TC %1 or outside window %2 MTC %3\n", was_full, outside_window (mtc_frame), mtc_frame));
		session.set_requested_return_frame (-1);
		session.request_transport_speed (0);
		session.request_locate (mtc_frame, false);
		update_mtc_status (MIDI::MTC_Stopped);
		reset (false);
		reset_window (mtc_frame);
	} else {

		/* we've had the first set of 8 qtr frame messages, determine position
		   and allow continuing qtr frame messages to provide position
		   and speed information.
		*/

		/* We received the last quarter frame 7 quarter frames (1.75 mtc
		   frames) after the instance when the contents of the mtc quarter
		   frames were decided. Add time to compensate for the elapsed 1.75
		   frames.
		*/
		double qtr = quarter_frame_duration;
		long int mtc_off = (long) rint(7.0 * qtr);

		DEBUG_TRACE (DEBUG::MTC, string_compose ("new mtc_frame: %1 | MTC-FpT: %2 A3-FpT:%3\n",
							 mtc_frame, (4.0*qtr), session.samples_per_timecode_frame()));

		switch (port->self_parser().mtc_running()) {
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
				mtc_frame_dll = mtc_frame;
				ActiveChanged (true); // emit signal
			}
			current.guard1++;
			current.position = mtc_frame;
			current.timestamp = now;
			current.guard2++;
			reset_window (mtc_frame);
		}
	}

	if (now) {
		last_inbound_frame = now;
	}
	busy_guard2++;
}

void
MTC_Slave::update_mtc_status (MIDI::MTC_Status status)
{
	/* XXX !!! thread safety ... called from MIDI I/O context
	 * on locate (via ::update_mtc_time())
	 */
	DEBUG_TRACE (DEBUG::MTC, string_compose("MTC_Slave::update_mtc_status - TID:%1 MTC:%2\n", pthread_name(), mtc_frame));
	return; // why was this fn needed anyway ? it just messes up things -> use reset.
	busy_guard1++;

	switch (status) {
	case MTC_Stopped:
		current.guard1++;
		current.position = mtc_frame;
		current.timestamp = 0;
		current.speed = 0;
		current.guard2++;

		break;

	case MTC_Forward:
		current.guard1++;
		current.position = mtc_frame;
		current.timestamp = 0;
		current.speed = 0;
		current.guard2++;
		break;

	case MTC_Backward:
		current.guard1++;
		current.position = mtc_frame;
		current.timestamp = 0;
		current.speed = 0;
		current.guard2++;
		break;
	}
	busy_guard2++;
}

void
MTC_Slave::reset_window (framepos_t root)
{
	/* if we're waiting for the master to catch us after seeking ahead, keep the window
	   of acceptable MTC frames wide open. otherwise, shrink it down to just 2 video frames
	   ahead of the window root (taking direction into account).
	*/

	framecnt_t const d = (quarter_frame_duration * 4 * frame_tolerance);

	switch (port->self_parser().mtc_running()) {
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

void
MTC_Slave::init_engine_dll (framepos_t pos, framepos_t inc)
{
	/* the bandwidth of the DLL is a trade-off,
	 * because the max-speed of the transport in ardour is
	 * limited to +-8.0, a larger bandwidth would cause oscillations
	 *
	 * But this is only really a problem if the user performs manual
	 * seeks while transport is running and slaved to MTC.
	 */
	oe = 2.0 * M_PI * double(inc) / 2.0 / double(session.frame_rate());
	be = 1.4142135623730950488 * oe;
	ce = oe * oe;

	ee2 = double(transport_direction * inc);
	te0 = double(pos);
	te1 = te0 + ee2;
	DEBUG_TRACE (DEBUG::MTC, string_compose ("[re-]init Engine DLL %1 %2 %3\n", te0, te1, ee2));
}

/* main entry point from session_process.cc
xo * in process callback context */
bool
MTC_Slave::speed_and_position (double& speed, framepos_t& pos)
{
	framepos_t now = session.engine().sample_time_at_cycle_start();
	framepos_t sess_pos = session.transport_frame(); // corresponds to now
	//sess_pos -= session.engine().frames_since_cycle_start();

	SafeTime last;
	frameoffset_t elapsed;
	bool engine_dll_reinitialized = false;

	read_current (&last);

	DEBUG_TRACE (DEBUG::MTC, string_compose ("speed&pos: timestamp %1 speed %2 initstate %3 dir %4 tpos %5 now %6 last-in %7\n",
						 last.timestamp,
						 last.speed,
						 engine_dll_initstate,
						 transport_direction,
						 sess_pos,
						 now,
						 last_inbound_frame));

	/* re-init engine DLL here when state changed (direction, first_mtc_timestamp) */
	if (last.timestamp == 0) {
		engine_dll_initstate = 0;
	} else if (engine_dll_initstate != transport_direction && last.speed != 0) {
		engine_dll_initstate = transport_direction;
		init_engine_dll(last.position, session.engine().samples_per_cycle());
		engine_dll_reinitialized = true;
	}

	if (last.timestamp == 0) {
		speed = 0;
		pos = session.transport_frame() ; // last.position;
		DEBUG_TRACE (DEBUG::MTC, string_compose ("first call to MTC_Slave::speed_and_position, pos = %1\n", pos));
		return true;
	}

	/* no timecode for two frames - conclude that it's stopped */
	if (last_inbound_frame && now > last_inbound_frame && now - last_inbound_frame > labs(seekahead_distance())) {
		speed = 0;
		pos = last.position;
		session.set_requested_return_frame (-1);
		session.request_locate (pos, false);
		session.request_transport_speed (0);
		engine_dll_initstate = 0;
		queue_reset (false);
		ActiveChanged (false);
		DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC not seen for 2 frames - reset pending, pos = %1\n", pos));
		return false;
	}


	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC::speed_and_position mtc-tme: %1 mtc-pos: %2 mtc-spd: %3\n", last.timestamp, last.position, last.speed));
	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC::speed_and_position eng-tme: %1 eng-pos: %2\n", now, sess_pos));

	double speed_flt = last.speed; ///< MTC speed from MTC-quarter-frame DLL

	/* interpolate position according to speed and time since last quarter-frame*/
	if (speed_flt == 0.0f) {
		elapsed = 0;
	} else {
		/* scale elapsed time by the current MTC speed */
		elapsed = (framecnt_t) rint (speed_flt * (now - last.timestamp));
		if (give_slave_full_control_over_transport_speed() && !engine_dll_reinitialized) {
			/* there is an engine vs MTC position frame-delta.
			 * This mostly due to quantization and rounding of (speed * nframes)
			 * but can also due to the session-process not calling
			 * speed_and_position() every cycle under some circumstances.
			 * Thus we use an other DLL to align the engine and the MTC
			 */

			/* update engine DLL and calculate speed */
			const double e = double (last.position + elapsed - sess_pos);
			te0 = te1;
			te1 += be * e + ee2;
			ee2 += ce * e;
			speed_flt = (te1 - te0) / double(session.engine().samples_per_cycle());
			DEBUG_TRACE (DEBUG::MTC, string_compose ("engine DLL t0:%1 t1:%2 err:%3 spd:%4 ddt:%5\n", te0, te1, e, speed_flt, ee2 - session.engine().samples_per_cycle() ));
		}
	}

	pos = last.position + elapsed;
	speed = speed_flt;

	/* may happen if the user performs a seek in the timeline while slaved to running MTC
	 * engine-DLL can oscillate back before 0.
	 * also see note in MTC_Slave::init_engine_dll
	 */
	if (!session.actively_recording()
	    && speed != 0
	    && ((pos < 0) || (labs(pos - sess_pos) > 3 * session.frame_rate()))) {
		engine_dll_initstate = 0;
		queue_reset (false);
	}

	/* provide a .1% deadzone to lock the speed */
	if (fabs (speed - 1.0) <= 0.001)
	        speed = 1.0;

	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTCsync spd: %1 pos: %2 | last-pos: %3 elapsed: %4 delta: %5\n",
						 speed, pos, last.position, elapsed,  pos - sess_pos));

	current_delta = (pos - sess_pos);

	return true;
}

Timecode::TimecodeFormat
MTC_Slave::apparent_timecode_format () const
{
	return mtc_timecode;
}

std::string
MTC_Slave::approximate_current_position() const
{
	SafeTime last;
	read_current (&last);
	if (last.timestamp == 0 || reset_pending) {
		return " --:--:--:--";
	}
	return Timecode::timecode_format_sampletime(
		last.position,
		double(session.frame_rate()),
		Timecode::timecode_to_frames_per_second(mtc_timecode),
		Timecode::timecode_has_drop_frames(mtc_timecode));
}

std::string
MTC_Slave::approximate_current_delta() const
{
	char delta[80];
	SafeTime last;
	read_current (&last);
	if (last.timestamp == 0 || reset_pending) {
		snprintf(delta, sizeof(delta), "\u2012\u2012\u2012\u2012");
	} else {
		snprintf(delta, sizeof(delta), "\u0394<span foreground=\"green\" face=\"monospace\" >%s%s%" PRIi64 "</span>sm",
				LEADINGZERO(abs(current_delta)), PLUSMINUS(-current_delta), abs(current_delta));
	}
	return std::string(delta);
}
