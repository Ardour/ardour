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

#define FLYWHEEL_TIMEOUT ( 1 * session.frame_rate() )

LTC_Slave::LTC_Slave (Session& s)
	: session (s)
{
	frames_per_ltc_frame = session.frames_per_timecode_frame();
	timecode.rate = session.timecode_frames_per_second();
	timecode.drop  = session.timecode_drop_frames();

	did_reset_tc_format = false;
	delayedlocked = 10;
	monotonic_cnt = 0;
	fps_detected=false;

	ltc_timecode = timecode_60; // track changes of LTC fps
	a3e_timecode = timecode_60; // track changes of Ardour's fps
	printed_timecode_warning = false;
	ltc_detect_fps_cnt = ltc_detect_fps_max = 0;
	memset(&prev_frame, 0, sizeof(LTCFrameExt));

	decoder = ltc_decoder_create((int) frames_per_ltc_frame, 128 /*queue size*/);
	reset();
	resync_latency();
	session.Xrun.connect_same_thread (port_connections, boost::bind (&LTC_Slave::resync_xrun, this));
	session.engine().GraphReordered.connect_same_thread (port_connections, boost::bind (&LTC_Slave::resync_latency, this));
}

LTC_Slave::~LTC_Slave()
{
	port_connections.drop_connections();

	if (did_reset_tc_format) {
		session.config.set_timecode_format (saved_tc_format);
	}

	ltc_decoder_free(decoder);
}

ARDOUR::framecnt_t
LTC_Slave::resolution () const
{
	return (framecnt_t) (frames_per_ltc_frame);
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
LTC_Slave::resync_xrun()
{
	DEBUG_TRACE (DEBUG::LTC, "LTC resync_xrun()\n");
	engine_dll_initstate = 0;
}

void
LTC_Slave::resync_latency()
{
	DEBUG_TRACE (DEBUG::LTC, "LTC resync_latency()\n");
	engine_dll_initstate = 0;

	if (session.ltc_output_io()) { /* check if Port exits */
		boost::shared_ptr<Port> ltcport = session.ltc_input_port();
		ltcport->get_connected_latency_range(ltc_slave_latency, false);
	}
}

void
LTC_Slave::reset()
{
	DEBUG_TRACE (DEBUG::LTC, "LTC reset()\n");
	last_timestamp = 0;
	current_delta = 0;
	transport_direction = 0;
	ltc_speed = 0;
	engine_dll_initstate = 0;
}

void
LTC_Slave::parse_ltc(const jack_nframes_t nframes, const jack_default_audio_sample_t * const in, const framecnt_t posinfo)
{
	jack_nframes_t i;
	unsigned char sound[8192];
	if (nframes > 8192) {
		/* TODO warn once or wrap, loop conversion below
		 * does jack/A3 support > 8192 spp anyway?
		 */
		return;
	}

	for (i = 0; i < nframes; i++) {
		const int snd=(int)rint((127.0*in[i])+128.0);
		sound[i] = (unsigned char) (snd&0xff);
	}
	ltc_decoder_write(decoder, sound, nframes, posinfo);
	return;
}

bool
LTC_Slave::equal_ltc_frame_time(LTCFrame *a, LTCFrame *b) {
	if (       a->frame_units != b->frame_units
		|| a->frame_tens  != b->frame_tens
		|| a->dfbit       != b->dfbit
		|| a->secs_units  != b->secs_units
		|| a->secs_tens   != b->secs_tens
		|| a->mins_units  != b->mins_units
		|| a->mins_tens   != b->mins_tens
		|| a->hours_units != b->hours_units
		|| a->hours_tens  != b->hours_tens
	     ) {
		return false;
	}
	return true;
}

bool
LTC_Slave::detect_discontinuity(LTCFrameExt *frame, int fps, bool fuzzy) {
	bool discontinuity_detected = false;

	if (fuzzy && (
		  ( frame->reverse && prev_frame.ltc.frame_units == 0)
		||(!frame->reverse && frame->ltc.frame_units == 0)
		)) {
		memcpy(&prev_frame, frame, sizeof(LTCFrameExt));
		return false;
	}

	if (frame->reverse) {
		ltc_frame_decrement(&prev_frame.ltc, fps , 0);
	} else {
		ltc_frame_increment(&prev_frame.ltc, fps , 0);
	}
	if (!equal_ltc_frame_time(&prev_frame.ltc, &frame->ltc)) {
		discontinuity_detected = true;
	}

    memcpy(&prev_frame, frame, sizeof(LTCFrameExt));
    return discontinuity_detected;
}

bool
LTC_Slave::detect_ltc_fps(int frameno, bool df)
{
	bool fps_changed = false;
	double detected_fps = 0;
	if (frameno > ltc_detect_fps_max)
	{
		ltc_detect_fps_max = frameno;
	}
	ltc_detect_fps_cnt++;

	if (ltc_detect_fps_cnt > 40)
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
		fps_changed=true;
	}

	/* poll and check session TC */
	TimecodeFormat tc_format = apparent_timecode_format();
	TimecodeFormat cur_timecode = session.config.get_timecode_format();
	if (Config->get_timecode_sync_frame_rate()) {
		/* enforce time-code */
		if (!did_reset_tc_format) {
			saved_tc_format = cur_timecode;
			did_reset_tc_format = true;
		}
		if (cur_timecode != tc_format) {
			warning << string_compose(_("Session framerate adjusted from %1 to LTC's %2."),
					Timecode::timecode_format_name(cur_timecode),
					Timecode::timecode_format_name(tc_format))
				<< endmsg;
			session.config.set_timecode_format (tc_format);
		}
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

	return fps_changed;
}

void
LTC_Slave::process_ltc(framepos_t const now)
{
	LTCFrameExt frame;
	while (ltc_decoder_read(decoder, &frame)) {
		SMPTETimecode stime;

		ltc_frame_to_time(&stime, &frame.ltc, 0);
		timecode.negative  = false;
		timecode.subframes  = 0;

		/* set timecode.rate and timecode.drop: */
		bool ltc_is_static = equal_ltc_frame_time(&prev_frame.ltc, &frame.ltc);

		if (detect_discontinuity(&frame, ceil(timecode.rate), !fps_detected)) {
			if (fps_detected) { ltc_detect_fps_cnt = ltc_detect_fps_max = 0; }
			fps_detected=false;
		}

		if (!ltc_is_static && detect_ltc_fps(stime.frame, (frame.ltc.dfbit)? true : false)) {
			reset();
			fps_detected=true;
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
#endif

		/* when a full LTC frame is decoded, the timecode the LTC frame
		 * is referring has just passed.
		 * So we send the _next_ timecode which
		 * is expected to start at the end of the current frame
		 */
		int fps_i = ceil(timecode.rate);
		if (!frame.reverse) {
			ltc_frame_increment(&frame.ltc, fps_i , 0);
			ltc_frame_to_time(&stime, &frame.ltc, 0);
			transport_direction = 1;
		} else {
			ltc_frame_decrement(&frame.ltc, fps_i , 0);
			int off = frame.off_end - frame.off_start;
			frame.off_start += off;
			frame.off_end += off;
			transport_direction = -1;
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

		framepos_t cur_timestamp = frame.off_end + 1;
		DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC F: %1 LF: %2  N: %3 L: %4\n", ltc_frame, last_ltc_frame, cur_timestamp, last_timestamp));
		if (frame.off_end + 1 <= last_timestamp || last_timestamp == 0) {
			DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC speed: UNCHANGED: %1\n", ltc_speed));
		} else {
			ltc_speed = double(ltc_frame - last_ltc_frame) / double(cur_timestamp - last_timestamp);
			DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC speed: %1\n", ltc_speed));
		}

		if (fabs(ltc_speed) > 10.0) {
			ltc_speed = 0;
		}

		last_timestamp = frame.off_end + 1;
		last_ltc_frame = ltc_frame;
	} /* end foreach decoded LTC frame */
}

void
LTC_Slave::init_engine_dll (framepos_t pos, int32_t inc)
{
	double omega = 2.0 * M_PI * double(inc) / double(session.frame_rate());
	b = 1.4142135623730950488 * omega;
	c = omega * omega;

	e2 = double(ltc_speed * inc);
	t0 = double(pos);
	t1 = t0 + e2;
	DEBUG_TRACE (DEBUG::LTC, string_compose ("[re-]init Engine DLL %1 %2 %3\n", t0, t1, e2));
}

/* main entry point from session_process.cc
 * called from jack_process callback context
 * so it is OK to use jack_port_get_buffer()
 */
bool
LTC_Slave::speed_and_position (double& speed, framepos_t& pos)
{
	bool engine_init_called = false;
	framepos_t now = session.engine().frame_time_at_cycle_start();
	framepos_t sess_pos = session.transport_frame(); // corresponds to now
	framecnt_t nframes = session.engine().frames_per_cycle();

	jack_default_audio_sample_t *in;

	boost::shared_ptr<Port> ltcport = session.ltc_input_port();

	in = (jack_default_audio_sample_t*) jack_port_get_buffer (ltcport->jack_port(), nframes);

	frameoffset_t skip = now - (monotonic_cnt + nframes);
	monotonic_cnt = now;
	DEBUG_TRACE (DEBUG::LTC, string_compose ("speed_and_position - TID:%1 | latency: %2 | skip %3\n", ::pthread_self(), ltc_slave_latency.max, skip));

	if (last_timestamp == 0) {
		engine_dll_initstate = 0;
		delayedlocked++;
	}
	else if (engine_dll_initstate != transport_direction && ltc_speed != 0) {
		engine_dll_initstate = transport_direction;
		init_engine_dll(last_ltc_frame + rint(ltc_speed * double(2 * nframes + now - last_timestamp)),
				session.engine().frames_per_cycle());
		engine_init_called = true;
	}

	if (in) {
		DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC Process eng-tme: %1 eng-pos: %2\n", now, sess_pos));
		/* when the jack-graph changes and if ardour performs
		 * locates, the audioengine is stopped (skipping frames) while
		 * jack [time] moves along.
		 */
		if (skip > 0) {
			DEBUG_TRACE (DEBUG::LTC, string_compose("engine skipped %1 frames. Feeding silence to LTC parser.\n", skip));
			if (skip >= 8192) skip = 8192;
			unsigned char sound[8192];
			memset(sound, 0, sizeof(char) * skip);
			ltc_decoder_write(decoder, sound, nframes, now);
		} else if (skip != 0) {
			/* this should never happen. it may if monotonic_cnt, now overflow on 64bit */
			DEBUG_TRACE (DEBUG::LTC, string_compose("engine skipped %1 frames\n", skip));
			reset();
		}

		parse_ltc(nframes, in, now + ltc_slave_latency.max );
		process_ltc(now);
	}

	if (last_timestamp == 0) {
		DEBUG_TRACE (DEBUG::LTC, "last timestamp == 0\n");
		speed = 0;
		pos = session.transport_frame();
		return true;
	} else if (ltc_speed != 0) {
		delayedlocked = 0;
	}

	if (abs(now - last_timestamp) > FLYWHEEL_TIMEOUT) {
		DEBUG_TRACE (DEBUG::LTC, "flywheel timeout\n");
		reset();
		speed = 0;
		pos = session.transport_frame();
		return true;
	}

	/* it take 2 cycles from naught to rolling.
	 * during these to initial cycles the speed == 0
	 *
	 * the first cycle:
	 * DEBUG::Slave: slave stopped, move to NNN
	 * DEBUG::Transport: Request forced locate to NNN
	 * DEBUG::Slave: slave state 0 @ NNN speed 0 cur delta VERY-LARGE-DELTA avg delta 1800
	 * DEBUG::Slave: silent motion
	 * DEBUG::Transport: realtime stop @ NNN
	 * DEBUG::Transport: Butler transport work, todo = PostTransportStop,PostTransportLocate,PostTransportClearSubstate
	 *
	 * [engine skips frames to locate, jack time keeps rolling on]
	 *
	 * the second cycle:
	 *
	 * DEBUG::LTC: [re-]init Engine DLL
	 * DEBUG::Slave: slave stopped, move to NNN+
	 * ...
	 *
	 * we need to seek two cycles ahead: 2 * nframes
	 */
	if (engine_dll_initstate == 0) {
		DEBUG_TRACE (DEBUG::LTC, "engine DLL not initialized. ltc_speed\n");
		speed = 0;
		pos = last_ltc_frame + rint(ltc_speed * double(2 * nframes + now - last_timestamp));
		return true;
	}

	/* interpolate position according to speed and time since last LTC-frame*/
	double speed_flt = ltc_speed;
	double elapsed = (now - last_timestamp) * speed_flt;

	if (!engine_init_called) {
		const double e = elapsed + double (last_ltc_frame - sess_pos);
		t0 = t1;
		t1 += b * e + e2;
		e2 += c * e;
		speed_flt = (t1 - t0) / double(session.engine().frames_per_cycle());
		DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC engine DLL t0:%1 t1:%2 err:%3 spd:%4 ddt:%5\n", t0, t1, e, speed_flt, e2 - session.engine().frames_per_cycle() ));
	} else {
		DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC adjusting elapsed (no DLL) from %1 by %2\n", elapsed, (2 * nframes * ltc_speed)));
		speed_flt = 0;
		elapsed += 2.0 * nframes * ltc_speed; /* see note above */
	}

	pos = last_ltc_frame + rint(elapsed);
	speed = speed_flt;
	current_delta = (pos - sess_pos);

	if (((pos < 0) || (labs(current_delta) > 2 * session.frame_rate()))) {
		DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC large drift: %1\n", current_delta));
		reset();
		speed = 0;
		pos = session.transport_frame();
		return true;
	}

	DEBUG_TRACE (DEBUG::LTC, string_compose ("LTCsync spd: %1 pos: %2 | last-pos: %3 elapsed: %4 delta: %5\n",
						 speed, pos, last_ltc_frame, elapsed, current_delta));

#if 1 /* provide a .1% deadzone to lock the speed */
	if (fabs(speed - 1.0) <= 0.001)
	        speed = 1.0;
#endif

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
	if (last_timestamp == 0 || engine_dll_initstate == 0) {
		snprintf(delta, sizeof(delta), "\u2012\u2012\u2012\u2012");
	} else if ((monotonic_cnt - last_timestamp) > 2 * frames_per_ltc_frame) {
		snprintf(delta, sizeof(delta), "flywheel");
	} else {
		snprintf(delta, sizeof(delta), "%s%4" PRIi64 " sm",
				PLUSMINUS(-current_delta), abs(current_delta));
	}
	return std::string(delta);
}
