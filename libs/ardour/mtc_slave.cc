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

    $Id$
*/

#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <unistd.h>
#include <pbd/error.h>
#include <pbd/failed_constructor.h>
#include <pbd/pthread_utils.h>

#include <midi++/port.h>
#include <ardour/slave.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/cycles.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace sigc;
using namespace MIDI;
using namespace PBD;

MTC_Slave::MTC_Slave (Session& s, MIDI::Port& p) 
	: session (s)
{
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
MTC_Slave::update_mtc_qtr (Parser& p)
{
	cycles_t cnow = get_cycles ();
	nframes_t now = session.engine().frame_time();
	nframes_t qtr;
	static cycles_t last_qtr = 0;

	qtr = (long) (session.frames_per_smpte_frame() / 4);
	mtc_frame += qtr;
	last_qtr = cnow;

	current.guard1++;
	current.position = mtc_frame;
	current.timestamp = now;
	current.guard2++;

	last_inbound_frame = now;
}

void
MTC_Slave::update_mtc_time (const byte *msg, bool was_full)
{
	nframes_t now = session.engine().frame_time();
	SMPTE::Time smpte;
	
	smpte.hours = msg[3];
	smpte.minutes = msg[2];
	smpte.seconds = msg[1];
	smpte.frames = msg[0];
	
	session.smpte_to_sample( smpte, mtc_frame, true, false );
	
	if (was_full) {
		
		current.guard1++; 	 
		current.position = mtc_frame; 	 
		current.timestamp = 0; 	 
		current.guard2++; 	 
		
		session.request_locate (mtc_frame, false); 	 
		update_mtc_status (MIDI::Parser::MTC_Stopped); 	 

 		reset ();
		
	} else {
		
		/* We received the last quarter frame 7 quarter frames (1.75 mtc
		   frames) after the instance when the contents of the mtc quarter
		   frames were decided. Add time to compensate for the elapsed 1.75
		   frames.
		   Also compensate for audio latency. 
		*/

		mtc_frame += (long) (1.75 * session.frames_per_smpte_frame()) + session.worst_output_latency();
		
		if (first_mtc_frame == 0) {
			first_mtc_frame = mtc_frame;
			first_mtc_time = now;
		} 
		
		current.guard1++;
		current.position = mtc_frame;
		current.timestamp = now;
		current.guard2++;
	}
	
	last_inbound_frame = now;
}

void
MTC_Slave::handle_locate (const MIDI::byte* mmc_tc)
{
	MIDI::byte mtc[4];
	
	mtc[3] = mmc_tc[0] & 0xf; /* hrs only */
	mtc[2] = mmc_tc[1];
	mtc[1] = mmc_tc[2];
	mtc[0] = mmc_tc[3];

	update_mtc_time (mtc, true);
}

void
MTC_Slave::update_mtc_status (MIDI::Parser::MTC_Status status)
{

	switch (status) {
	case MTC_Stopped:
		mtc_speed = 0.0f;
		mtc_frame = 0;

		current.guard1++;
		current.position = mtc_frame;
		current.timestamp = 0;
		current.guard2++;

		break;

	case MTC_Forward:
		mtc_speed = 0.0f;
		mtc_frame = 0;

		current.guard1++;
		current.position = mtc_frame;
		current.timestamp = 0;
		current.guard2++;

		break;

	case MTC_Backward:
		mtc_speed = 0.0f;
		mtc_frame = 0;

		current.guard1++;
		current.position = mtc_frame;
		current.timestamp = 0;
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
MTC_Slave::speed_and_position (float& speed, nframes_t& pos)
{
	nframes_t now = session.engine().frame_time();
	SafeTime last;
	nframes_t frame_rate;
	nframes_t elapsed;
	float speed_now;

	read_current (&last);

	if (first_mtc_time == 0) {
		speed = 0;
		pos = last.position;
		return true;
	}
	
	/* no timecode for 1/4 second ? conclude that its stopped */

	if (last_inbound_frame && now > last_inbound_frame && now - last_inbound_frame > session.frame_rate() / 4) {
		mtc_speed = 0;
		pos = last.position;
		session.request_locate (pos, false);
		update_mtc_status (MIDI::Parser::MTC_Stopped);
		reset();
		return false;
	}

	frame_rate = session.frame_rate();

	speed_now = (float) ((last.position - first_mtc_frame) / (double) (now - first_mtc_time));

	accumulator[accumulator_index++] = speed_now;

	if (accumulator_index >= accumulator_size) {
		have_first_accumulated_speed = true;
		accumulator_index = 0;
	}

	if (have_first_accumulated_speed) {
		float total = 0;

		for (int32_t i = 0; i < accumulator_size; ++i) {
			total += accumulator[i];
		}

		mtc_speed = total / accumulator_size;

	} else {

		mtc_speed = speed_now;

	}

	if (mtc_speed == 0.0f) {

		elapsed = 0;

	} else {
	
		/* scale elapsed time by the current MTC speed */
		
		if (last.timestamp && (now > last.timestamp)) {
			elapsed = (nframes_t) floor (mtc_speed * (now - last.timestamp));
		} else {
			elapsed = 0; /* XXX is this right? */
		}
	}

	/* now add the most recent timecode value plus the estimated elapsed interval */

	pos =  elapsed + last.position;

	speed = mtc_speed;
	return true;
}

ARDOUR::nframes_t
MTC_Slave::resolution() const
{
	return (nframes_t) session.frames_per_smpte_frame();
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
	current.guard2++;
	first_mtc_frame = 0;
	first_mtc_time = 0;

	accumulator_index = 0;
	have_first_accumulated_speed = false;
	mtc_speed = 0;
}
