/*
    Copyright (C) 2012 Paul Davis
    Witten by 2012 Robin Gareus <robin@gareus.org>

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

#include "ardour/debug.h"
#include "ardour/slave.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/audio_port.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace MIDI;
using namespace PBD;
using namespace Timecode;

LTC_Slave::LTC_Slave (Session& s)
	: session (s)
{
	current_frames_per_ltc_frame = 1920; // samplerate / framerate
	ltc_transport_pos = 0;
	ltc_speed = 1.0;
	monotonic_fcnt = 0;

	decoder = ltc_decoder_create(current_frames_per_ltc_frame, 128 /*queue size*/);
}

LTC_Slave::~LTC_Slave()
{
	if (did_reset_tc_format) {
		session.config.set_timecode_format (saved_tc_format);
	}

	ltc_decoder_free(decoder);
}

bool
LTC_Slave::give_slave_full_control_over_transport_speed() const
{
	return true; // DLL align to engine transport
	// return false; // for Session-level computed varispeed
}

ARDOUR::framecnt_t
LTC_Slave::resolution () const
{
	return current_frames_per_ltc_frame;
}

ARDOUR::framecnt_t
LTC_Slave::seekahead_distance () const
{
	return current_frames_per_ltc_frame * 2;
}

bool
LTC_Slave::locked () const
{
	return true;
}

bool
LTC_Slave::ok() const
{
	return true;
}

int
LTC_Slave::parse_ltc(const jack_nframes_t nframes, const jack_default_audio_sample_t * const in, const framecnt_t posinfo)
{
	jack_nframes_t i;
	unsigned char sound[8192];
	if (nframes > 8192) return 1;

	for (i = 0; i < nframes; i++) {
		const int snd=(int)rint((127.0*in[i])+128.0);
		sound[i] = (unsigned char) (snd&0xff);
	}
	ltc_decoder_write(decoder, sound, nframes, posinfo);
	return 0;
}

void
LTC_Slave::process_ltc()
{
	LTCFrameExt frame;
	while (ltc_decoder_read(decoder,&frame)) {
		SMPTETimecode stime;
		ltc_frame_to_time(&stime, &frame.ltc, 0);

		fprintf(stdout, "%02d:%02d:%02d%c%02d | %8lld %8lld%s\n",
			stime.hours,
			stime.mins,
			stime.secs,
			(frame.ltc.dfbit) ? '.' : ':',
			stime.frame,
			frame.off_start,
			frame.off_end,
			frame.reverse ? " R" : "  "
			);

		/* when a full LTC frame is decoded, the timecode the LTC frame
		 * is referring has just passed.
		 * So we send the _next_ timecode which
		 * is expected to start at the end of the current frame
		 */

		int detected_fps = 25; // XXX
		if (!frame.reverse) {
			ltc_frame_increment(&frame.ltc, detected_fps , 0);
			ltc_frame_to_time(&stime, &frame.ltc, 0);
		} else {
			ltc_frame_decrement(&frame.ltc, detected_fps , 0);
			int off = frame.off_end - frame.off_start;
			frame.off_start += off;
			frame.off_end += off;
		}
	}
}

/* main entry point from session_process.cc
 * called from jack_process callback context
 */
bool
LTC_Slave::speed_and_position (double& speed, framepos_t& pos)
{
	DEBUG_TRACE (DEBUG::MTC, string_compose ("LTC_Slave::speed_and_position - TID:%1\n", ::pthread_self()));
	framepos_t now = session.engine().frame_time_at_cycle_start();
	framepos_t sess_pos = session.transport_frame(); // corresponds to now
	framecnt_t nframes = session.engine().frames_per_cycle();

	jack_port_t *ltc_port = session.engine().ltc_input_port()->jack_port();
	jack_default_audio_sample_t *in;
	in = (jack_default_audio_sample_t*) jack_port_get_buffer (ltc_port, nframes);

	//in = session.engine().ltc_input_port()->engine_get_whole_audio_buffer()
	// TODO: get capture latency for ltc_port.

	if (in) {
		parse_ltc(nframes, in, monotonic_fcnt /* + jltc_latency*/ );
		process_ltc();
	}

	/* fake for testing */
	ltc_transport_pos += nframes * ltc_speed;
	pos = ltc_transport_pos;
	speed = ltc_speed;

	monotonic_fcnt += nframes;
}
