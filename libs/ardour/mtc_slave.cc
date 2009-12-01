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
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"

#include "midi++/port.h"
#include "ardour/debug.h"
#include "ardour/slave.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/cycles.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace sigc;
using namespace MIDI;
using namespace PBD;

MTC_Slave::MTC_Slave (Session& s, MIDI::Port& p)
	: session (s)
{
	can_notify_on_unknown_rate = true;

	last_mtc_fps_byte = session.get_mtc_timecode_bits ();

	rebind (p);
	reset ();
}

MTC_Slave::~MTC_Slave()
{
}

void
MTC_Slave::rebind (MIDI::Port& p)
{
	for (vector<sigc::connection>::iterator i = connections.begin(); i != connections.end(); ++i) {
		(*i).disconnect ();
	}

	port = &p;

	connections.push_back (port->input()->mtc_time.connect (mem_fun (*this, &MTC_Slave::update_mtc_time)));
	connections.push_back (port->input()->mtc_qtr.connect (mem_fun (*this, &MTC_Slave::update_mtc_qtr)));
	connections.push_back (port->input()->mtc_status.connect (mem_fun (*this, &MTC_Slave::update_mtc_status)));
}

void
MTC_Slave::update_mtc_qtr (Parser& /*p*/)
{
	nframes64_t now = session.engine().frame_time();
	nframes_t qtr;

	qtr = (long) (session.frames_per_timecode_frame() / 4);
	mtc_frame += qtr;
	
	double speed = compute_apparent_speed (now);

	current.guard1++;
	current.position = mtc_frame;
	current.timestamp = now;
	current.speed = speed;
	current.guard2++;

	last_inbound_frame = now;
}

void
MTC_Slave::update_mtc_time (const byte *msg, bool was_full)
{
	nframes64_t now = session.engine().frame_time();
	Timecode::Time timecode;

	timecode.hours = msg[3];
	timecode.minutes = msg[2];
	timecode.seconds = msg[1];
	timecode.frames = msg[0];

	last_mtc_fps_byte = msg[4];

	switch (msg[4]) {
	case MTC_24_FPS:
		timecode.rate = 24;
		timecode.drop = false;
		can_notify_on_unknown_rate = true;
		break;
	case MTC_25_FPS:
		timecode.rate = 25;
		timecode.drop = false;
		can_notify_on_unknown_rate = true;
		break;
	case MTC_30_FPS_DROP:
		timecode.rate = 30;
		timecode.drop = true;
		can_notify_on_unknown_rate = true;
		break;
	case MTC_30_FPS:
		timecode.rate = 30;
		timecode.drop = false;
		can_notify_on_unknown_rate = true;
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
	}

	session.timecode_to_sample (timecode, mtc_frame, true, false);

	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC time timestamp = %1 TC %2 = frame %3 (from full message ? %4)\n", 
						 now, timecode, mtc_frame, was_full));

	if (was_full) {

		session.request_locate (mtc_frame, false);
		session.request_transport_speed (0);
		update_mtc_status (MIDI::Parser::MTC_Stopped);
		reset ();

	} else {


		/* We received the last quarter frame 7 quarter frames (1.75 mtc
		   frames) after the instance when the contents of the mtc quarter
		   frames were decided. Add time to compensate for the elapsed 1.75
		   frames.
		   Also compensate for audio latency.
		*/
#if 0		
		mtc_frame += (long) (1.75 * session.frames_per_timecode_frame()) + session.worst_output_latency();

		/* leave speed alone here. compute it only as we receive qtr frame messages */
		
		current.guard1++;
		current.position = mtc_frame;
		current.timestamp = now;
		current.guard2++;

		DEBUG_TRACE (DEBUG::MTC, string_compose ("stored TC frame = %1 @ %2, sp = %3\n", mtc_frame, now, speed));
#endif
	}

	last_inbound_frame = now;
}

double
MTC_Slave::compute_apparent_speed (nframes64_t now)
{
	if (current.timestamp != 0) {
		
		double speed = (double) ((mtc_frame - current.position) / (double) (now - current.timestamp));
		DEBUG_TRACE (DEBUG::MTC, string_compose ("instantaneous speed = %1 from %2 - %3 / %4 - %5\n",
							 speed, mtc_frame, current.position, now, current.timestamp));
		
		accumulator[accumulator_index++] = speed;
		
		if (accumulator_index >= accumulator_size) {
			have_first_accumulated_speed = true;
			accumulator_index = 0;
		}
		
		if (have_first_accumulated_speed) {
			double total = 0;
			
			for (int32_t i = 0; i < accumulator_size; ++i) {
				total += accumulator[i];
			}
			
			speed = total / accumulator_size;
			DEBUG_TRACE (DEBUG::MTC, string_compose ("speed smoothed to %1\n", speed));
		} 

		return speed;
		
	} else {
		
		return 0;
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

	update_mtc_time (mtc, true);
}

void
MTC_Slave::update_mtc_status (MIDI::Parser::MTC_Status status)
{
	/* XXX !!! thread safety ... called from MIDI I/O context
	   and process() context (via ::speed_and_position())
	*/

	switch (status) {
	case MTC_Stopped:
		mtc_frame = 0;

		current.guard1++;
		current.position = mtc_frame;
		current.timestamp = 0;
		current.speed = 0;
		current.guard2++;

		break;

	case MTC_Forward:
		mtc_frame = 0;

		current.guard1++;
		current.position = mtc_frame;
		current.timestamp = 0;
		current.speed = 0;
		current.guard2++;

		break;

	case MTC_Backward:
		mtc_frame = 0;

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
	return port->input()->mtc_locked();
}

bool
MTC_Slave::ok() const
{
	return true;
}

bool
MTC_Slave::speed_and_position (double& speed, nframes64_t& pos)
{
	nframes64_t now = session.engine().frame_time();
	SafeTime last;
	nframes_t elapsed;

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
		update_mtc_status (MIDI::Parser::MTC_Stopped);
		reset();
		DEBUG_TRACE (DEBUG::MTC, "MTC not seen for 1/4 second - reset\n");
		return false;
	}

	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC::speed_and_position %1 %2\n", last.speed, last.position));

	if (last.speed == 0.0f) {

		elapsed = 0;

	} else {

		/* scale elapsed time by the current MTC speed */

		if (last.timestamp && (now > last.timestamp)) {
			elapsed = (nframes_t) floor (speed * (now - last.timestamp));
		} else {
			elapsed = 0; /* XXX is this right? */
		}
	}

	/* now add the most recent timecode value plus the estimated elapsed interval */

	pos =  elapsed + last.position;
	speed = last.speed;

	DEBUG_TRACE (DEBUG::MTC, string_compose ("MTC::speed_and_position FINAL %1 %2\n", last.speed, pos));

	return true;
}

ARDOUR::nframes_t
MTC_Slave::resolution() const
{
	return (nframes_t) session.frames_per_timecode_frame();
}

void
MTC_Slave::reset ()
{
	/* XXX massive thread safety issue here. MTC could
	   be being updated as we call this. but this
	   supposed to be a realtime-safe call.
	*/

	port->input()->reset_mtc_state ();

	last_inbound_frame = 0;
	current.guard1++;
	current.position = 0;
	current.timestamp = 0;
	current.speed = 0;
	current.guard2++;
	accumulator_index = 0;
	have_first_accumulated_speed = false;
}
