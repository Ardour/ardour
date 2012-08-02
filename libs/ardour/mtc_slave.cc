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
#include <iostream>
#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <unistd.h>

#include "pbd/error.h"

#include "midi++/port.h"
#include "ardour/debug.h"
#include "ardour/slave.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/pi_controller.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace MIDI;
using namespace PBD;

/* length (in timecode frames) of the "window" that we consider legal given receipt of
   a given timecode position. Ardour will try to chase within this window, and will
   stop+locate+wait+chase if timecode arrives outside of it. The window extends entirely
   in the current direction of motion, so if any timecode arrives that is before the most
   recently received position (and without the direction of timecode reversing too), we
   will stop+locate+wait+chase.
*/

const int MTC_Slave::frame_tolerance = 2;

MTC_Slave::MTC_Slave (Session& s, MIDI::Port& p)
	: session (s)
{
	can_notify_on_unknown_rate = true;
	did_reset_tc_format = false;
	reset_pending = 0;
	reset_position = false;

	pic = new PIChaser();

	last_mtc_fps_byte = session.get_mtc_timecode_bits ();
	mtc_frame = 0;

	speed_accumulator_size = 16;
	speed_accumulator = new double[speed_accumulator_size];

	rebind (p);
	reset (true);
}

MTC_Slave::~MTC_Slave()
{
	if (did_reset_tc_format) {
		session.config.set_timecode_format (saved_tc_format);
	}

	delete pic;
	delete [] speed_accumulator;
}

bool
MTC_Slave::give_slave_full_control_over_transport_speed() const
{
	return true; // for PiC control */
	// return false; // for Session-level computed varispeed
}

void
MTC_Slave::rebind (MIDI::Port& p)
{
	port_connections.drop_connections ();

	port = &p;

	port->parser()->mtc_time.connect_same_thread (port_connections,  boost::bind (&MTC_Slave::update_mtc_time, this, _1, _2, _3));
	port->parser()->mtc_qtr.connect_same_thread (port_connections, boost::bind (&MTC_Slave::update_mtc_qtr, this, _1, _2, _3));
	port->parser()->mtc_status.connect_same_thread (port_connections, boost::bind (&MTC_Slave::update_mtc_status, this, _1));
}

void
MTC_Slave::update_mtc_qtr (Parser& /*p*/, int which_qtr, framepos_t now)
{
	DEBUG_TRACE (DEBUG::MTC, string_compose ("qtr frame %1 at %2\n", which_qtr, now));
	maybe_reset ();
	last_inbound_frame = now;
}

void
MTC_Slave::update_mtc_time (const byte *msg, bool was_full, framepos_t now)
{
	/* "now" can be zero if this is called from a context where we do not have or do not want
	   to use a timestamp indicating when this MTC time was received. example: when we received
	   a locate command via MMC.
	*/

	if (now) {
		maybe_reset ();
	}

	Timecode::Time timecode;
	TimecodeFormat tc_format;
	bool reset_tc = true;
	framepos_t window_root = -1;

	DEBUG_TRACE (DEBUG::MTC, string_compose ("full mtc time known at %1, full ? %2\n", now, was_full));

	timecode.hours = msg[3];
	timecode.minutes = msg[2];
	timecode.seconds = msg[1];
	timecode.frames = msg[0];

	last_mtc_fps_byte = msg[4];

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
		timecode.rate = 30;
		timecode.drop = true;
		tc_format = timecode_30drop;
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
		if (!did_reset_tc_format) {
			saved_tc_format = session.config.get_timecode_format();
			did_reset_tc_format = true;
		}
		session.config.set_timecode_format (tc_format);
	}

	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC time timestamp = %1 TC %2 = frame %3 (from full message ? %4)\n",
						 now, timecode, mtc_frame, was_full));

	if (was_full || outside_window (mtc_frame)) {

		session.timecode_to_sample (timecode, mtc_frame, true, false);
		session.request_locate (mtc_frame, false);
		session.request_transport_speed (0);
		update_mtc_status (MIDI::MTC_Stopped);
		reset (false);
		reset_window (mtc_frame);

	} else {

		/* we've had the first set of 8 qtr frame messages, determine position
		   and allow continuing qtr frame messages to provide position
		   and speed information.
		*/

		/* do a careful conversion of the timecode value to a position
		   so that we take drop/nondrop and all that nonsense into
		   consideration.
		*/

		session.timecode_to_sample (timecode, mtc_frame, true, false);

		/* We received the last quarter frame 7 quarter frames (1.75 mtc
		   frames) after the instance when the contents of the mtc quarter
		   frames were decided. Add time to compensate for the elapsed 1.75
		   frames. Also compensate for audio latency.
		*/

		mtc_frame += (long) (1.75 * session.frames_per_timecode_frame()) + session.worst_playback_latency();


		if (now) {

			if (last_mtc_timestamp == 0) {

				last_mtc_timestamp = now;
				last_mtc_frame = mtc_frame;

			} else {

				if (give_slave_full_control_over_transport_speed()) {
					/* PIC
					 *
					 * its not the average, but we will assign it to current.speed below
					 */

				    static framepos_t last_seen_timestamp = 0;
				    static framepos_t last_seen_position = 0;

				    if ((now - last_seen_timestamp) < 300) {
					mtc_frame = (mtc_frame + last_seen_position)/2;
				    }

				    last_seen_timestamp = now;
				    last_seen_position = mtc_frame;



				} else {

					/* Non-PiC
					 */

					framepos_t time_delta = (now - last_mtc_timestamp);

					if (time_delta != 0) {
						double apparent_speed = (mtc_frame - last_mtc_frame) / (double) (time_delta);

						process_apparent_speed (apparent_speed);
						DEBUG_TRACE (DEBUG::Slave, string_compose ("apparent speed was %1 average is now %2\n", apparent_speed, average_speed));
					} else {
						DEBUG_TRACE (DEBUG::Slave, string_compose ("no apparent calc, average is %1\n", average_speed));
					}

					/* every second, recalibrate the starting point for the speed measurement */
					if (mtc_frame - last_mtc_frame > session.frame_rate()) {
						last_mtc_timestamp = now;
						last_mtc_frame = mtc_frame;
					}
				}
			}

			current.guard1++;
			current.position = mtc_frame;
			current.timestamp = now;
			current.speed = average_speed;
			current.guard2++;
			window_root = mtc_frame;
		}
	}

	if (now) {
		last_inbound_frame = now;
	}

	if (window_root >= 0) {
		reset_window (window_root);
	}
}

void
MTC_Slave::process_apparent_speed (double this_speed)
{
	DEBUG_TRACE (DEBUG::MTC, string_compose ("speed cnt %1 sz %2 have %3\n", speed_accumulator_cnt, speed_accumulator_size, have_first_speed_accumulator));

	/* clamp to an expected range */

	if (this_speed > 4.0 || this_speed < -4.0) {
		this_speed = average_speed;
	}

	if (speed_accumulator_cnt >= speed_accumulator_size) {
		have_first_speed_accumulator = true;
		speed_accumulator_cnt = 0;
	}

	speed_accumulator[speed_accumulator_cnt++] = this_speed;

	if (have_first_speed_accumulator) {
		average_speed = 0.0;
		for (size_t i = 0; i < speed_accumulator_size; ++i) {
			average_speed += speed_accumulator[i];
		}
		average_speed /= speed_accumulator_size;
	}
}

void
MTC_Slave::handle_locate (const MIDI::byte* mmc_tc)
{
	MIDI::byte mtc[5];

	mtc[4] = last_mtc_fps_byte;
	mtc[3] = mmc_tc[0] & 0xf; /* hrs only */
	mtc[2] = mmc_tc[1];
	mtc[1] = mmc_tc[2];
	mtc[0] = mmc_tc[3];

	update_mtc_time (mtc, true, 0);
}

void
MTC_Slave::update_mtc_status (MIDI::MTC_Status status)
{
	/* XXX !!! thread safety ... called from MIDI I/O context
	   and process() context (via ::speed_and_position())
	*/

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

}

void
MTC_Slave::read_current (SafeTime *st) const
{
	int tries = 0;

	do {
		if (tries == 10) {
			error << _("MTC Slave: atomic read of current time failed, sleeping!") << endmsg;
			usleep (20);
			tries = 0;
		}
		*st = current;
		tries++;

	} while (st->guard1 != st->guard2);
}

bool
MTC_Slave::locked () const
{
	return port->parser()->mtc_locked();
}

bool
MTC_Slave::ok() const
{
	return true;
}

bool
MTC_Slave::speed_and_position (double& speed, framepos_t& pos)
{
	framepos_t now = session.engine().frame_time();
	SafeTime last;
	framecnt_t elapsed;
	bool in_control = false;

	read_current (&last);

	if (last.timestamp == 0) {
		speed = 0;
		pos = last.position;
		DEBUG_TRACE (DEBUG::MTC, string_compose ("first call to MTC_Slave::speed_and_position, pos = %1\n", last.position));
		return true;
	}

	/* no timecode for 1/4 second ? conclude that its stopped */

	if (last_inbound_frame && now > last_inbound_frame && now - last_inbound_frame > session.frame_rate() / 4) {
		speed = 0;
		pos = last.position;
		session.request_locate (pos, false);
		session.request_transport_speed (0);
		queue_reset (false);
		DEBUG_TRACE (DEBUG::MTC, "MTC not seen for 1/4 second - reset pending\n");
		return false;
	}

	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC::speed_and_position %1 %2\n", last.speed, last.position));

	if (give_slave_full_control_over_transport_speed()) {
		in_control = (session.slave_state() == Session::Running);
		framepos_t pic_want_locate = 0;
		//framepos_t slave_pos = session.audible_frame();
		framepos_t slave_pos = session.transport_frame();
		static double average_speed = 0;

		framepos_t ref_now = session.engine().frame_time_at_cycle_start();
		average_speed = pic->get_ratio (last.timestamp, last.position, ref_now, slave_pos, in_control, session.engine().frames_per_cycle());

		pic_want_locate = pic->want_locate();

		if (in_control && pic_want_locate) {
			last.speed = average_speed + (double) (pic_want_locate - session.transport_frame()) / (double)session.get_block_size();
			std::cout << "locate req " << pic_want_locate << " speed: " << average_speed << "\n";
		} else {
			last.speed = average_speed;
		}
	}

	if (last.speed == 0.0f) {

		elapsed = 0;

	} else {

		/* scale elapsed time by the current MTC speed */

		if (last.timestamp && (now > last.timestamp)) {
			elapsed = (framecnt_t) floor (last.speed * (now - last.timestamp));
			DEBUG_TRACE (DEBUG::MTC, string_compose ("last timecode received @ %1, now = %2, elapsed frames = %3 w/speed= %4\n",
								 last.timestamp, now, elapsed, last.speed));
		} else {
			elapsed = 0; /* XXX is this right? */
		}
	}

	/* now add the most recent timecode value plus the estimated elapsed interval */

	if (in_control) {
		pos = session.transport_frame();
	} else {
		pos = last.position + elapsed;
	}

	speed = last.speed;

	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC::speed_and_position FINAL %1 %2\n", last.speed, pos));


	DEBUG_TRACE (DEBUG::MTC, string_compose ("last = %1 elapsed = %2 pos = %3 speed = %4\n", last.position, elapsed, pos, speed));

	return true;
}

ARDOUR::framecnt_t
MTC_Slave::resolution () const
{
	return (framecnt_t) session.frames_per_timecode_frame();
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

	window_begin = 0;
	window_end = 0;
	last_mtc_frame = 0;
	last_mtc_timestamp = 0;

	average_speed = 0;
	have_first_speed_accumulator = false;
	speed_accumulator_cnt = 0;

	pic->reset();
}

void
MTC_Slave::reset_window (framepos_t root)
{

	/* if we're waiting for the master to catch us after seeking ahead, keep the window
	   of acceptable MTC frames wide open. otherwise, shrink it down to just 2 video frames
	   ahead of the window root (taking direction into account).
	*/

	switch (port->parser()->mtc_running()) {
	case MTC_Forward:
		window_begin = root;
		if (session.slave_state() == Session::Running) {
			window_end = root + (session.frames_per_timecode_frame() * frame_tolerance);
		} else {
			window_end = root + seekahead_distance ();
		}
		break;

	case MTC_Backward:
		if (session.slave_state() == Session::Running) {
			framecnt_t const d = session.frames_per_timecode_frame() * frame_tolerance;
			if (root > d) {
				window_begin = root - d;
				window_end = root;
			} else {
				window_begin = 0;
			}
		} else {
			framecnt_t const d = seekahead_distance ();
			if (root > d) {
				window_begin = root - d;
			} else {
				window_begin = 0;
			}
		}
		window_end = root;
		break;

	default:
		/* do nothing */
		break;
	}

	DEBUG_TRACE (DEBUG::MTC, string_compose ("legal MTC window now %1 .. %2\n", window_begin, window_end));
}

ARDOUR::framecnt_t
MTC_Slave::seekahead_distance () const
{
	/* 1 second */
	return session.frame_rate();
}

bool
MTC_Slave::outside_window (framepos_t pos) const
{
	return ((pos < window_begin) || (pos > window_end));
}
