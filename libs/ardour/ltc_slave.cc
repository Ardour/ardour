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
#include <sys/types.h>
#include <unistd.h>

#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"

#include "ardour/debug.h"
#include "ardour/profile.h"
#include "ardour/transport_master.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/audio_port.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace MIDI;
using namespace PBD;
using namespace Timecode;

#define ENGINE AudioEngine::instance()
#define FLYWHEEL_TIMEOUT ( 1 * ENGINE->sample_rate() )

/* XXX USE Config->get_ltc_input */

LTC_TransportMaster::LTC_TransportMaster (std::string const & name)
	: TimecodeTransportMaster (name, LTC)
	, did_reset_tc_format (false)
	, decoder (0)
	, samples_per_ltc_frame (0)
	, fps_detected (false)
	, monotonic_cnt (0)
	, last_timestamp (0)
	, last_ltc_sample (0)
	, delayedlocked (10)
	, ltc_detect_fps_cnt (0)
	, ltc_detect_fps_max (0)
	, sync_lock_broken (false)
{
	if ((_port = AudioEngine::instance()->register_input_port (DataType::AUDIO, string_compose ("%1 in", _name))) == 0) {
		throw failed_constructor();
	}

	DEBUG_TRACE (DEBUG::Slave, string_compose ("LTC registered %1\n", _port->name()));

	memset(&prev_sample, 0, sizeof(LTCFrameExt));

	resync_latency();

	AudioEngine::instance()->Xrun.connect_same_thread (port_connections, boost::bind (&LTC_TransportMaster::resync_xrun, this));
	AudioEngine::instance()->GraphReordered.connect_same_thread (port_connections, boost::bind (&LTC_TransportMaster::resync_latency, this));
}

void
LTC_TransportMaster::init ()
{
	reset (true);
}

void
LTC_TransportMaster::set_session (Session *s)
{
	config_connection.disconnect ();
	_session = s;

	if (_session) {

		samples_per_ltc_frame = _session->samples_per_timecode_frame();
		timecode.rate = _session->timecode_frames_per_second();
		timecode.drop  = _session->timecode_drop_frames();
		printed_timecode_warning = false;
		ltc_timecode = _session->config.get_timecode_format();
		a3e_timecode = _session->config.get_timecode_format();

		if (Config->get_use_session_timecode_format() && _session) {
			samples_per_timecode_frame = _session->samples_per_timecode_frame();
		}

		if (decoder) {
			ltc_decoder_free (decoder);
		}

		decoder = ltc_decoder_create((int) samples_per_ltc_frame, 128 /*queue size*/);

		parse_timecode_offset();
		reset();

		_session->config.ParameterChanged.connect_same_thread (config_connection, boost::bind (&LTC_TransportMaster::parameter_changed, this, _1));
	}
}

LTC_TransportMaster::~LTC_TransportMaster()
{
	port_connections.drop_connections();
	config_connection.disconnect();

	if (did_reset_tc_format) {
		_session->config.set_timecode_format (saved_tc_format);
	}

	ltc_decoder_free(decoder);
}

void
LTC_TransportMaster::parse_timecode_offset() {
	Timecode::Time offset_tc;
	Timecode::parse_timecode_format(_session->config.get_slave_timecode_offset(), offset_tc);
	offset_tc.rate = _session->timecode_frames_per_second();
	offset_tc.drop = _session->timecode_drop_frames();
	_session->timecode_to_sample(offset_tc, timecode_offset, false, false);
	timecode_negative_offset = offset_tc.negative;
}

void
LTC_TransportMaster::parameter_changed (std::string const & p)
{
	if (p == "slave-timecode-offset"
			|| p == "timecode-format"
			) {
		parse_timecode_offset();
	}
}

ARDOUR::samplecnt_t
LTC_TransportMaster::resolution () const
{
	return (samplecnt_t) (ENGINE->sample_rate() / 1000);
}

bool
LTC_TransportMaster::locked () const
{
	return (delayedlocked < 5);
}

bool
LTC_TransportMaster::ok() const
{
	return true;
}

void
LTC_TransportMaster::resync_xrun()
{
	DEBUG_TRACE (DEBUG::LTC, "LTC resync_xrun()\n");
	sync_lock_broken = false;
}

void
LTC_TransportMaster::resync_latency()
{
	DEBUG_TRACE (DEBUG::LTC, "LTC resync_latency()\n");
	sync_lock_broken = false;

	if (!_port) {
		_port->get_connected_latency_range (ltc_slave_latency, false);
	}
}

void
LTC_TransportMaster::reset (bool with_ts)
{
	DEBUG_TRACE (DEBUG::LTC, "LTC reset()\n");
	if (with_ts) {
		last_timestamp = 0;
		_current_delta = 0;
	}
	transport_direction = 0;
	ltc_speed = 0;
	sync_lock_broken = false;
	monotonic_cnt = 0;
}

void
LTC_TransportMaster::parse_ltc (const ARDOUR::pframes_t nframes, const Sample* const in, const ARDOUR::samplecnt_t posinfo)
{
	pframes_t i;
	unsigned char sound[8192];

	if (nframes > 8192) {
		/* TODO warn once or wrap, loop conversion below
		 * does jack/A3 support > 8192 spp anyway?
		 */
		return;
	}

	for (i = 0; i < nframes; i++) {
		const int snd=(int) rint ((127.0*in[i])+128.0);
		sound[i] = (unsigned char) (snd&0xff);
	}

	ltc_decoder_write (decoder, sound, nframes, posinfo);

	return;
}

bool
LTC_TransportMaster::equal_ltc_sample_time(LTCFrame *a, LTCFrame *b) {
	if (a->frame_units != b->frame_units ||
	    a->frame_tens  != b->frame_tens ||
	    a->dfbit       != b->dfbit ||
	    a->secs_units  != b->secs_units ||
	    a->secs_tens   != b->secs_tens ||
	    a->mins_units  != b->mins_units ||
	    a->mins_tens   != b->mins_tens ||
	    a->hours_units != b->hours_units ||
	    a->hours_tens  != b->hours_tens) {
		return false;
	}
	return true;
}

bool
LTC_TransportMaster::detect_discontinuity(LTCFrameExt *sample, int fps, bool fuzzy) {
	bool discontinuity_detected = false;

	if (fuzzy && (
		  ( sample->reverse && prev_sample.ltc.frame_units == 0)
		||(!sample->reverse && sample->ltc.frame_units == 0)
		)) {
		memcpy(&prev_sample, sample, sizeof(LTCFrameExt));
		return false;
	}

	if (sample->reverse) {
		ltc_frame_decrement(&prev_sample.ltc, fps, LTC_TV_525_60, 0);
	} else {
		ltc_frame_increment(&prev_sample.ltc, fps, LTC_TV_525_60, 0);
	}
	if (!equal_ltc_sample_time(&prev_sample.ltc, &sample->ltc)) {
		discontinuity_detected = true;
	}

    memcpy(&prev_sample, sample, sizeof(LTCFrameExt));
    return discontinuity_detected;
}

bool
LTC_TransportMaster::detect_ltc_fps(int frameno, bool df)
{
	bool fps_changed = false;
	double detected_fps = 0;
	if (frameno > ltc_detect_fps_max)
	{
		ltc_detect_fps_max = frameno;
	}
	ltc_detect_fps_cnt++;

	if (ltc_detect_fps_cnt > 40) {
		if (ltc_detect_fps_cnt > ltc_detect_fps_max) {
			detected_fps = ltc_detect_fps_max + 1;
			if (df) {
				/* LTC df -> indicates fractional framerate */
				if (fr2997()) {
					detected_fps = detected_fps * 999.0 / 1000.0;
				} else {
					detected_fps = detected_fps * 1000.0 / 1001.0;
				}
			}

			if (timecode.rate != detected_fps || timecode.drop != df) {
				DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC detected FPS: %1%2\n", detected_fps, df?"df":"ndf"));
			} else {
				detected_fps = 0; /* no cange */
			}
		}
		ltc_detect_fps_cnt = ltc_detect_fps_max = 0;
	}

	/* when changed */
	if (detected_fps != 0 && (detected_fps != timecode.rate || df != timecode.drop)) {
		timecode.rate = detected_fps;
		timecode.drop = df;
		samples_per_ltc_frame = double(_session->sample_rate()) / timecode.rate;
		DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC reset to FPS: %1%2 ; audio-samples per LTC: %3\n",
				detected_fps, df?"df":"ndf", samples_per_ltc_frame));
		fps_changed=true;
	}

	/* poll and check session TC */
	TimecodeFormat tc_format = apparent_timecode_format();
	TimecodeFormat cur_timecode = _session->config.get_timecode_format();

	if (Config->get_timecode_sync_frame_rate()) {
		/* enforce time-code */
		if (!did_reset_tc_format) {
			saved_tc_format = cur_timecode;
			did_reset_tc_format = true;
		}
		if (cur_timecode != tc_format) {
			if (ceil(Timecode::timecode_to_frames_per_second(cur_timecode)) != ceil(Timecode::timecode_to_frames_per_second(tc_format))) {
				warning << string_compose(_("Session framerate adjusted from %1 to LTC's %2."),
						Timecode::timecode_format_name(cur_timecode),
						Timecode::timecode_format_name(tc_format))
					<< endmsg;
			}
			_session->config.set_timecode_format (tc_format);
		}
	} else {
		/* only warn about TC mismatch */
		if (ltc_timecode != tc_format) printed_timecode_warning = false;
		if (a3e_timecode != cur_timecode) printed_timecode_warning = false;

		if (cur_timecode != tc_format && ! printed_timecode_warning) {
			if (ceil(Timecode::timecode_to_frames_per_second(cur_timecode)) != ceil(Timecode::timecode_to_frames_per_second(tc_format))) {
				warning << string_compose(_("Session and LTC framerate mismatch: LTC:%1 Session:%2."),
						Timecode::timecode_format_name(tc_format),
						Timecode::timecode_format_name(cur_timecode))
					<< endmsg;
			}
			printed_timecode_warning = true;
		}
	}
	ltc_timecode = tc_format;
	a3e_timecode = cur_timecode;

	if (Config->get_use_session_timecode_format() && _session) {
		samples_per_timecode_frame = _session->samples_per_timecode_frame();
	} else {
		samples_per_timecode_frame = ENGINE->sample_rate() / Timecode::timecode_to_frames_per_second (ltc_timecode);
	}

	return fps_changed;
}

void
LTC_TransportMaster::process_ltc(samplepos_t const now)
{
	LTCFrameExt sample;
	LTC_TV_STANDARD tv_standard = LTC_TV_625_50;

	while (ltc_decoder_read (decoder, &sample)) {

		SMPTETimecode stime;

		ltc_frame_to_time (&stime, &sample.ltc, 0);
		timecode.negative  = false;
		timecode.subframes  = 0;

		/* set timecode.rate and timecode.drop: */

		const bool ltc_is_stationary = equal_ltc_sample_time (&prev_sample.ltc, &sample.ltc);

		if (detect_discontinuity (&sample, ceil(timecode.rate), !fps_detected)) {

			if (fps_detected) {
				ltc_detect_fps_cnt = ltc_detect_fps_max = 0;
			}

			fps_detected = false;
		}

		if (!ltc_is_stationary && detect_ltc_fps (stime.frame, (sample.ltc.dfbit)? true : false)) {
			reset();
			fps_detected=true;
		}

#ifndef NDEBUG
		if (DEBUG_ENABLED (DEBUG::LTC)) {
			/* use fprintf for simpler correct formatting of times
			 */
			fprintf (stderr, "LTC@%ld %02d:%02d:%02d%c%02d | %8lld %8lld%s\n",
			         now,
			         stime.hours,
			         stime.mins,
			         stime.secs,
			         (sample.ltc.dfbit) ? '.' : ':',
			         stime.frame,
			         sample.off_start,
			         sample.off_end,
			         sample.reverse ? " R" : "  "
				);
		}
#endif

		/* when a full LTC sample is decoded, the timecode the LTC sample
		 * is referring has just passed.
		 * So we send the _next_ timecode which
		 * is expected to start at the end of the current sample
		 */
		int fps_i = ceil(timecode.rate);

		switch(fps_i) {
			case 30:
				if (timecode.drop) {
					tv_standard = LTC_TV_525_60;
				} else {
					tv_standard = LTC_TV_1125_60;
				}
				break;
			case 25:
				tv_standard = LTC_TV_625_50;
				break;
			default:
				tv_standard = LTC_TV_FILM_24; /* == LTC_TV_1125_60 == no offset, 24,30fps BGF */
				break;
		}

		if (!sample.reverse) {
			ltc_frame_increment(&sample.ltc, fps_i, tv_standard, 0);
			ltc_frame_to_time(&stime, &sample.ltc, 0);
			transport_direction = 1;
			sample.off_start -= ltc_frame_alignment(samples_per_timecode_frame, tv_standard);
			sample.off_end -= ltc_frame_alignment(samples_per_timecode_frame, tv_standard);
		} else {
			ltc_frame_decrement(&sample.ltc, fps_i, tv_standard, 0);
			int off = sample.off_end - sample.off_start;
			sample.off_start += off - ltc_frame_alignment(samples_per_timecode_frame, tv_standard);
			sample.off_end += off - ltc_frame_alignment(samples_per_timecode_frame, tv_standard);
			transport_direction = -1;
		}

		timecode.hours   = stime.hours;
		timecode.minutes = stime.mins;
		timecode.seconds = stime.secs;
		timecode.frames  = stime.frame;

		samplepos_t ltc_sample; // audio-sample corresponding to position of LTC frame

		if (_session && Config->get_use_session_timecode_format()) {
			Timecode::timecode_to_sample (timecode, ltc_sample, true, false, (double)ENGINE->sample_rate(), _session->config.get_subframes_per_frame(), timecode_negative_offset, timecode_offset);
		} else {
			Timecode::timecode_to_sample (timecode, ltc_sample, true, false, (double)ENGINE->sample_rate(), 100, timecode_negative_offset, timecode_offset);
		}

		ltc_sample += ltc_slave_latency.max;

		/* This LTC frame spans sample time between sample.off_start  .. sample.off_end
		 *
		 * NOTE: these sample times are NOT the ones that LTC is representing. They are
		 * derived our own audioengine's monotonic audio clock.
		 *
		 * So we expect the next frame to span sample.off_end+1 and ... <don't care for now>.
		 * That isn't the time we will necessarily receive the LTC frame, but the decoder
		 * should tell us that its span begins there.
		 *
		 */

		samplepos_t cur_timestamp = sample.off_end + 1;

		DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC S: %1 LS: %2  N: %3 L: %4\n", ltc_sample, last_ltc_sample, cur_timestamp, last_timestamp));

		if (cur_timestamp <= last_timestamp || last_timestamp == 0) {
			DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC speed: UNCHANGED: %1\n", ltc_speed));
		} else {
			ltc_speed = double (ltc_sample - last_ltc_sample) / double (cur_timestamp - last_timestamp);
			DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC speed: %1\n", ltc_speed));
		}

		if (fabs (ltc_speed) > 10.0) {
			ltc_speed = 0;
		}

		last_timestamp = cur_timestamp;
		last_ltc_sample = ltc_sample;

	} /* end foreach decoded LTC sample */
}

bool
LTC_TransportMaster::speed_and_position (double& speed, samplepos_t& pos, samplepos_t now)
{
	if (!_collect || last_timestamp == 0) {
		return false;
	}

	/* XXX these are not atomics and maybe modified in a thread other other than the one
	   that is executing this.
	*/

	speed = ltc_speed;

	/* provide a .1% deadzone to lock the speed */
	if (fabs (speed - 1.0) <= 0.001) {
		speed = 1.0;
	}

	if (speed != 0 && delayedlocked == 0 && fabs(speed) != 1.0) {
		sync_lock_broken = true;
		DEBUG_TRACE (DEBUG::LTC, string_compose ("LTC speed not locked %1 based on %2\n", speed, ltc_speed));
	}

	pos =  last_ltc_sample;
	pos += (now - last_timestamp) * speed;

	return true;
}

void
LTC_TransportMaster::pre_process (ARDOUR::pframes_t nframes, samplepos_t now, boost::optional<samplepos_t> session_pos)
{
	Sample* in = (Sample*) AudioEngine::instance()->port_engine().get_buffer (_port->port_handle(), nframes);
	sampleoffset_t skip = now - (monotonic_cnt + nframes);
	monotonic_cnt = now;

	DEBUG_TRACE (DEBUG::LTC, string_compose ("pre-process - TID:%1 | latency: %2 | skip %3 | session ? %4| last %5 | dir %6 | sp %7\n",
	                                         pthread_name(), ltc_slave_latency.max, skip, (_session ? 'y' : 'n'), last_timestamp, transport_direction, ltc_speed));

	if (last_timestamp == 0) {
		if (delayedlocked < 10) {
			++delayedlocked;
		}

	} else if (ltc_speed != 0) {

	}

	DEBUG_TRACE (DEBUG::LTC, string_compose ("pre-process with audio clock time: %1\n", now));

	/* if the audioengine failed to take the process lock, it won't
	   call this method, and time will appear to skip. Reset the
	   LTC decoder's state by giving it some silence.
	*/

	if (skip > 0) {
		DEBUG_TRACE (DEBUG::LTC, string_compose("engine skipped %1 samples. Feeding silence to LTC parser.\n", skip));
		if (skip >= 8192) skip = 8192;
		unsigned char sound[8192];
		memset (sound, 0x80, sizeof(char) * skip);
		ltc_decoder_write (decoder, sound, nframes, now);
	} else if (skip != 0) {
		/* this should never happen. it may if monotonic_cnt, now overflow on 64bit */
		DEBUG_TRACE (DEBUG::LTC, string_compose("engine skipped %1 samples\n", skip));
		reset();
	}

	/* Now feed the incoming LTC signal into the decoder */

	parse_ltc (nframes, in, now);

	/* and pull out actual LTC frame data */

	process_ltc (now);

	if (last_timestamp == 0) {
		DEBUG_TRACE (DEBUG::LTC, "last timestamp == 0\n");
		return;
	} else if (ltc_speed != 0) {
		DEBUG_TRACE (DEBUG::LTC, string_compose ("speed non-zero (%1)\n", ltc_speed));
		if (delayedlocked > 1) {
			delayedlocked--;
		} else if (_current_delta == 0) {
			delayedlocked = 0;
		}
	}

	if (abs (now - last_timestamp) > FLYWHEEL_TIMEOUT) {
		DEBUG_TRACE (DEBUG::LTC, "flywheel timeout\n");
		reset();
		/* don't change position from last known */

		return;
	}

	if (session_pos) {
		const samplepos_t current_pos = last_ltc_sample + ((now - last_timestamp) * ltc_speed);
		_current_delta = current_pos - *session_pos;
	} else {
		_current_delta = 0;
	}
}

Timecode::TimecodeFormat
LTC_TransportMaster::apparent_timecode_format () const
{
	if      (timecode.rate == 24 && !timecode.drop)
		return timecode_24;
	else if (timecode.rate == 25 && !timecode.drop)
		return timecode_25;
	else if (rint(timecode.rate * 100) == 2997 && !timecode.drop)
		return (fr2997() ? timecode_2997000 : timecode_2997);
	else if (rint(timecode.rate * 100) == 2997 &&  timecode.drop)
		return (fr2997() ? timecode_2997000drop : timecode_2997drop);
	else if (timecode.rate == 30 &&  timecode.drop)
		return timecode_2997drop; // timecode_30drop; // LTC counting to 30 samples w/DF *means* 29.97 df
	else if (timecode.rate == 30 && !timecode.drop)
		return timecode_30;

	/* XXX - unknown timecode format */
	return _session->config.get_timecode_format();
}

std::string
LTC_TransportMaster::position_string() const
{
	if (!_collect || last_timestamp == 0) {
		return " --:--:--:--";
	}
	return Timecode::timecode_format_time(timecode);
}

std::string
LTC_TransportMaster::delta_string() const
{
	char delta[80];

	if (!_collect || last_timestamp == 0) {
		snprintf (delta, sizeof(delta), "\u2012\u2012\u2012\u2012");
	} else if ((monotonic_cnt - last_timestamp) > 2 * samples_per_ltc_frame) {
		snprintf (delta, sizeof(delta), "%s", _("flywheel"));
	} else {
		snprintf (delta, sizeof(delta), "\u0394<span foreground=\"%s\" face=\"monospace\" >%s%s%lld</span>sm",
				sync_lock_broken ? "red" : "green",
				LEADINGZERO(::llabs(_current_delta)), PLUSMINUS(-_current_delta), ::llabs(_current_delta));
	}

	return delta;
}
