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

#define FLYWHEEL_TIMEOUT ( 3 * session.frame_rate() )

LTC_Slave::LTC_Slave (Session& s)
	: session (s)
{
	frames_per_ltc_frame = session.frames_per_timecode_frame(); // XXX at most 30fps ?
	timecode.rate = session.timecode_frames_per_second();
	timecode.drop  = session.timecode_drop_frames();

	ltc_transport_pos = 0;
	did_reset_tc_format = false;
	delayedlocked = 10;
	engine_dll_initstate = 0;
	monotonic_cnt = 0;

	memset(&prev_ltc_frame, 0, sizeof(LTCFrame));

	ltc_timecode = timecode_60; // track changes of LTC timecode
	a3e_timecode = timecode_60; // track canges of Ardour's timecode
	printed_timecode_warning = false;

	decoder = ltc_decoder_create((int) frames_per_ltc_frame, 128 /*queue size*/);
	reset();
	//session.engine().Xrun.connect_same_thread (*this, boost::bind (&LTC_Slave::reset, this));
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
	return (framecnt_t) (frames_per_ltc_frame);
}

ARDOUR::framecnt_t
LTC_Slave::seekahead_distance () const
{
	return 0;
}

bool
LTC_Slave::locked () const
{
	return (delayedlocked < 5);
}

bool
LTC_Slave::ok() const
{
	return true;
}

void
LTC_Slave::reset()
{
	DEBUG_TRACE (DEBUG::LTC, "LTC reset()\n");
	frames_in_sequence = 0;
	ltc_detect_fps_cnt = ltc_detect_fps_max = 0;
	last_timestamp = 0;
	current_delta = 0;
	transport_direction = 0;
	ltc_speed = 0;
	ltc_decoder_queue_flush(decoder);
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

bool
LTC_Slave::detect_ltc_fps(int frameno, bool df)
{
	double detected_fps = 0;
	if (frameno > ltc_detect_fps_max)
	{
		ltc_detect_fps_max = frameno;
	}
	ltc_detect_fps_cnt++;
	if (ltc_detect_fps_cnt > 60)
	{
		if (ltc_detect_fps_cnt > ltc_detect_fps_max
		    && (   ceil(timecode.rate) != (ltc_detect_fps_max + 1)
			|| timecode.drop != df
			)
		    )
		{
			DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC detected FPS %1%2",
					ltc_detect_fps_max + 1, timecode.drop ? "df" : ""));
			detected_fps = ltc_detect_fps_max + 1;
			if (df) {
				/* LTC df -> indicates fractional framerate */
				detected_fps = detected_fps * 1000.0 / 1001.0;
			}
			DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC detected FPS: %1%2\n", detected_fps, df?"df":"ndf"));
		}
		ltc_detect_fps_cnt = ltc_detect_fps_max = 0;
	}

	/* when changed */
	if (detected_fps != 0 && (detected_fps != timecode.rate || df != timecode.drop)) {
		timecode.rate = detected_fps;
		timecode.drop = df;
		frames_per_ltc_frame = double(session.frame_rate()) / timecode.rate;
		DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC reset to FPS: %1%2 ; audio-frames per LTC: %3\n",
				detected_fps, df?"df":"ndf", frames_per_ltc_frame));
		return true; // reset()
	}

	/* poll and check session TC */
	if (1) {
		TimecodeFormat tc_format = apparent_timecode_format();
		TimecodeFormat cur_timecode = session.config.get_timecode_format();
		if (Config->get_timecode_sync_frame_rate()) {
			/* enforce time-code */
			if (!did_reset_tc_format) {
				saved_tc_format = cur_timecode;
				did_reset_tc_format = true;
			}
			if (cur_timecode != tc_format) {
				warning << string_compose(_("Session framerate adjusted from %1 TO: LTC's %2."),
						Timecode::timecode_format_name(cur_timecode),
						Timecode::timecode_format_name(tc_format))
					<< endmsg;
			}
			session.config.set_timecode_format (tc_format);
		} else {
			/* only warn about TC mismatch */
			if (ltc_timecode != tc_format) printed_timecode_warning = false;
			if (a3e_timecode != cur_timecode) printed_timecode_warning = false;

			if (cur_timecode != tc_format && ! printed_timecode_warning) {
				warning << string_compose(_("Session and LTC framerate mismatch: LTC:%1 Session:%2."),
						Timecode::timecode_format_name(tc_format),
						Timecode::timecode_format_name(cur_timecode))
					<< endmsg;
				printed_timecode_warning = true;
			}
		}
		ltc_timecode = tc_format;
		a3e_timecode = cur_timecode;
	}
	return false;
}

bool
LTC_Slave::detect_ltc_discontinuity(LTCFrameExt *frame) {
	bool discontinuity_detected = false;
	/* detect discontinuities */
	if (frame->reverse) {
		ltc_frame_decrement(&prev_ltc_frame, ceil(timecode.rate), 0);
	} else {
		ltc_frame_increment(&prev_ltc_frame, ceil(timecode.rate), 0);
	}

	if (memcmp(&prev_ltc_frame, &frame->ltc, sizeof(LTCFrame))) {
		discontinuity_detected = true;
	}
	memcpy(&prev_ltc_frame, &frame->ltc, sizeof(LTCFrame));

	/* notfify about discontinuities */
	if (frames_in_sequence > 0 && discontinuity_detected) {
		DEBUG_TRACE (DEBUG::LTC, "# LTC DISCONTINUITY\n");
		frames_in_sequence=0;
		return true;
	}
	frames_in_sequence++;

	return false;
}

bool
LTC_Slave::process_ltc(framepos_t const now, framepos_t const sess_pos, framecnt_t const nframes)
{
	bool have_frame = false;
	DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC Process eng-tme: %1 eng-pos: %2\n", now, sess_pos));

	LTCFrameExt frame;
	while (ltc_decoder_read(decoder,&frame)) {
		bool reinitialize_ltc_dll = false;
		SMPTETimecode stime;

		ltc_frame_to_time(&stime, &frame.ltc, 0);
		timecode.negative  = false;
		timecode.subframes  = 0;
		/* set timecode.rate and timecode.drop: */
		if (detect_ltc_fps(stime.frame, (frame.ltc.dfbit)? true : false)) {
			reset();
			break;
		}
		if (detect_ltc_discontinuity(&frame)) {
			ltc_discontinuity = true;
		}

#if 0 // Devel/Debug
		fprintf(stdout, "LTC %02d:%02d:%02d%c%02d | %8lld %8lld%s\n",
			stime.hours,
			stime.mins,
			stime.secs,
			(frame.ltc.dfbit) ? '.' : ':',
			stime.frame,
			frame.off_start,
			frame.off_end,
			frame.reverse ? " R" : "  "
			);

		if (frames_in_sequence < 1) {
			fprintf(stdout, " ####### FIRST LTC FRAME in SEQ #######\n");
		}

		if (ltc_discontinuity) {
			fprintf(stdout, " ####### LTC DISCONTINUITY #######\n");
		}
#endif

		if (frames_in_sequence < 1) {
			continue;
		}

		/* when a full LTC frame is decoded, the timecode the LTC frame
		 * is referring has just passed.
		 * So we send the _next_ timecode which
		 * is expected to start at the end of the current frame
		 */
		int fps_i = ceil(timecode.rate);
		if (!frame.reverse) {
			ltc_frame_increment(&frame.ltc, fps_i , 0);
			ltc_frame_to_time(&stime, &frame.ltc, 0);
		} else {
			ltc_frame_decrement(&frame.ltc, fps_i , 0);
			int off = frame.off_end - frame.off_start;
			frame.off_start += off;
			frame.off_end += off;
		}

		timecode.hours   = stime.hours;
		timecode.minutes = stime.mins;
		timecode.seconds = stime.secs;
		timecode.frames  = stime.frame;

		/* map LTC timecode to session TC setting */
		framepos_t ltc_frame; ///< audio-frame corresponding to LTC frame
		Timecode::timecode_to_sample (timecode, ltc_frame, true, false,
			double(session.frame_rate()),
			session.config.get_subframes_per_frame(),
			session.config.get_timecode_offset_negative(), session.config.get_timecode_offset()
			);

		/* (frame.off_end + 1) = start of next LTC frame */
		double poff = (frame.off_end + 1 - now);
		ltc_transport_pos = ltc_frame - poff;

#if 0 // vari-speed LTC, no DLL
			frames_per_ltc_frame = 1 + frame.off_end - frame.off_start;
#else
			frames_per_ltc_frame = (double(session.frame_rate()) / timecode.rate);
#endif

		DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC frame: %1 poff: %2 pos: %3\n", ltc_frame, poff, ltc_transport_pos));

		if (ltc_discontinuity) {
			ltc_discontinuity=false;
			if (ltc_speed==0
					|| !locked()
					|| (ltc_transport_pos - sess_pos) > FLYWHEEL_TIMEOUT)
			{
				engine_dll_initstate = 0;
				reset();
			}
			reinitialize_ltc_dll = true;
		}

		if (last_timestamp == 0
			|| ((now - last_timestamp) > FLYWHEEL_TIMEOUT)
			|| (abs(current_delta) >  FLYWHEEL_TIMEOUT) // TODO LTC-delta not engine delta
			|| (frame.reverse && transport_direction != -1)
			|| (!frame.reverse && transport_direction != 1)
			) {
			reinitialize_ltc_dll = true;
			engine_dll_initstate = 0;
			reset();
		}

		if (reinitialize_ltc_dll) {
			init_ltc_dll(ltc_transport_pos, frames_per_ltc_frame);

			if (ltc_speed==0 || !locked()) {
				if (frame.reverse) {
					transport_direction = -1;
					ltc_transport_pos -= nframes * rint((2 * frames_per_ltc_frame + poff)/nframes);
				} else {
					transport_direction = 1;
					ltc_transport_pos += nframes * rint((2 * frames_per_ltc_frame + poff)/nframes);
				}
			}

		} else {
			// update DLL
			double e = (double(ltc_frame) - poff - double(sess_pos));
			t0 = t1;
			t1 += b * e + e2;
			e2 += c * e;

			ltc_speed = (t1 - t0) / frames_per_ltc_frame;
			current_delta = (ltc_transport_pos - sess_pos);
			DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC DLL t0:%1 t1:%2 err:%3 spd:%4 ddt:%5\n", t0, t1, e, ltc_speed, e2 - frames_per_ltc_frame));

		}

		/* the are equivalent
		 * TODO: choose the value with larger diff ->
		 * minimize roundig errors when extrapolating
		 */
#if 1
		last_timestamp = now;
		last_ltc_frame = ltc_transport_pos;
#else
		last_timestamp = frame.off_end + 1;
		last_ltc_frame = ltc_frame;
#endif

		have_frame = true;
	} /* end foreach decoded LTC frame */

	return have_frame;
}

void
LTC_Slave::init_ltc_dll(framepos_t const tme, double const dt)
{
	omega = 2.0 * M_PI * dt / double(session.frame_rate());
	b = 1.4142135623730950488 * omega;
	c = omega * omega;

	e2 = dt;
	t0 = double(tme);
	t1 = t0 + e2;
	DEBUG_TRACE (DEBUG::LTC, string_compose ("[re-]init LTC DLL %1 %2 %3\n", t0, t1, e2));
}

void
LTC_Slave::init_engine_dll (framepos_t pos, framepos_t inc)
{
	/* the bandwidth of the DLL is a trade-off,
	 * because the max-speed of the transport in ardour is
	 * limited to +-8.0, a larger bandwidth would cause oscillations
	 *
	 * But this is only really a problem if the user performs manual
	 * seeks while transport is running and slaved to LTC.
	 */
	oe = 2.0 * M_PI * double(inc/2.0) / double(session.frame_rate());
	be = 1.4142135623730950488 * oe;
	ce = oe * oe;

	ee2 = double(transport_direction * inc);
	te0 = double(pos);
	te1 = te0 + ee2;
	DEBUG_TRACE (DEBUG::LTC, string_compose ("[re-]init Engine DLL %1 %2 %3\n", te0, te1, ee2));
}

/* main entry point from session_process.cc
 * called from jack_process callback context
 * so it is OK to use jack_port_get_buffer() etc
 */
bool
LTC_Slave::speed_and_position (double& speed, framepos_t& pos)
{
	//framepos_t now = session.engine().frame_time_at_cycle_start();
	//framepos_t now = session.engine().processed_frames();
	framepos_t now = monotonic_cnt;
	framepos_t sess_pos = session.transport_frame(); // corresponds to now
	framecnt_t nframes = session.engine().frames_per_cycle();
	jack_default_audio_sample_t *in;
	jack_latency_range_t ltc_latency;

	monotonic_cnt += nframes;

	boost::shared_ptr<Port> ltcport = session.engine().ltc_input_port();
	ltcport->get_connected_latency_range(ltc_latency, false);
	in = (jack_default_audio_sample_t*) jack_port_get_buffer (ltcport->jack_port(), nframes);

	DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC_Slave::speed_and_position - TID:%1 | latency: %2\n", ::pthread_self(), ltc_latency.max));

	if (last_timestamp == 0) {
		engine_dll_initstate = 0;
		delayedlocked++;
	}
	else if (engine_dll_initstate != transport_direction) {
		engine_dll_initstate = transport_direction;
		init_engine_dll(last_ltc_frame, session.engine().frames_per_cycle());
	}

	if (in) {
		parse_ltc(nframes, in, now  + ltc_latency.max );
		if (!process_ltc(now, sess_pos, nframes) && ltc_speed != 0) {
		}
	}

	/* interpolate position according to speed and time since last LTC-frame*/
	double speed_flt = ltc_speed;
	framecnt_t elapsed;
	if (speed_flt == 0.0f) {
		elapsed = 0;
	} else {
		/* scale elapsed time by the current LTC speed */
		if (last_timestamp && (now > last_timestamp)) {
			elapsed = (now - last_timestamp) * speed_flt;
		} else {
			elapsed = 0;
		}
		/* update engine DLL and calculate speed */
		const double e = double (last_ltc_frame + elapsed - sess_pos);
		te0 = te1;
		te1 += be * e + ee2;
		ee2 += ce * e;
		speed_flt = (te1 - te0) / double(session.engine().frames_per_cycle());
		DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC engine DLL t0:%1 t1:%2 err:%3 spd:%4 ddt:%5\n", te0, te1, e, speed_flt, ee2 - session.engine().frames_per_cycle() ));
	}

	pos = last_ltc_frame + elapsed;
	speed = speed_flt;
	current_delta = (pos - sess_pos);

	DEBUG_TRACE (DEBUG::LTC, string_compose ("LTCsync spd: %1 pos: %2 | last-pos: %3 elapsed: %4 delta: %5\n",
						 speed, pos, last_ltc_frame, elapsed, current_delta));

	if (last_timestamp != 0 /* && frames_in_sequence > 8*/) {
		delayedlocked = 0;
	}

	if (last_timestamp == 0 || ((now - last_timestamp) > FLYWHEEL_TIMEOUT)) {
		DEBUG_TRACE (DEBUG::LTC, "LTC no-signal - reset\n");
		reset();
		engine_dll_initstate = 0;
		speed = 0;
		pos = session.transport_frame();
		return true;
	}

	if (last_timestamp != 0  && ltc_speed != 0 && ((ltc_transport_pos < 0) || (labs(current_delta) > 10 * session.frame_rate()))) {
		DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC large drift. %1\n", current_delta));
		// XXX only re-init engine DLL ?
		reset();
		engine_dll_initstate = 0;
		speed = 0;
		pos = session.transport_frame();
		return true;
	}

	return true;
}

Timecode::TimecodeFormat
LTC_Slave::apparent_timecode_format () const
{
	if      (timecode.rate == 24 && !timecode.drop)
		return timecode_24;
	else if (timecode.rate == 25 && !timecode.drop)
		return timecode_25;
	else if (rint(timecode.rate * 100) == 2997 && !timecode.drop)
		return timecode_2997;
	else if (rint(timecode.rate * 100) == 2997 &&  timecode.drop)
		return timecode_2997drop;
	else if (timecode.rate == 30 &&  timecode.drop)
		return timecode_2997drop; // timecode_30drop; // LTC counting to 30 frames w/DF *means* 29.97 df
	else if (timecode.rate == 30 && !timecode.drop)
		return timecode_30;

	/* XXX - unknown timecode format */
	return session.config.get_timecode_format();
}

std::string
LTC_Slave::approximate_current_position() const
{
	if (last_timestamp == 0) {
		return " --:--:--:--";
	}
	return Timecode::timecode_format_time(timecode);
}

std::string
LTC_Slave::approximate_current_delta() const
{
	char delta[24];
	if (last_timestamp == 0 || frames_in_sequence < 2) {
		snprintf(delta, sizeof(delta), "\u2012\u2012\u2012\u2012");
	} else if ((monotonic_cnt - last_timestamp) > 2 * frames_per_ltc_frame) {
		snprintf(delta, sizeof(delta), "flywheel");
	} else {
		// TODO if current_delta > 1 frame -> display timecode.
		// delta >0 if A3's transport is _behind_ LTC
		snprintf(delta, sizeof(delta), "%s%4" PRIi64 " sm",
				PLUSMINUS(-current_delta), abs(current_delta));
	}
	return std::string(delta);
}
