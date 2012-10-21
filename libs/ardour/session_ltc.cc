/*
  Copyright (C) 2012 Paul Davis
  Written by Robin Gareus <robin@gareus.org>

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
#include "ardour/debug.h"
#include "timecode/time.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/audio_port.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace MIDI;
using namespace PBD;
using namespace Timecode;

//#define LTC_GEN_FRAMEDBUG

void
Session::ltc_tx_initialize()
{
	ltc_enc_tcformat = config.get_timecode_format();

	DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX init sr: %1 fps: %2\n", nominal_frame_rate(), timecode_to_frames_per_second(ltc_enc_tcformat)));
	ltc_encoder = ltc_encoder_create(nominal_frame_rate(),
			timecode_to_frames_per_second(ltc_enc_tcformat),
			0);

	ltc_encoder_set_bufsize(ltc_encoder, nominal_frame_rate(), 23.0);

	/* buffersize for 1 LTC frame: (1 + sample-rate / fps) bytes
	 * usually returned by ltc_encoder_get_buffersize(encoder)
	 *
	 * since the fps can change and A3's  min fps: 24000/1001 */
	ltc_enc_buf = (ltcsnd_sample_t*) calloc((nominal_frame_rate() / 23), sizeof(ltcsnd_sample_t));
	ltc_speed = 0;
	ltc_tx_reset();
}

void
Session::ltc_tx_cleanup()
{
	DEBUG_TRACE (DEBUG::LTC, "LTC TX cleanup\n");
	if (ltc_enc_buf) free(ltc_enc_buf);
	ltc_encoder_free(ltc_encoder);
	ltc_encoder = NULL;
}

void
Session::ltc_tx_reset()
{
	DEBUG_TRACE (DEBUG::LTC, "LTC TX reset\n");
	ltc_enc_pos = -1; // force re-start
	ltc_buf_len = 0;
	ltc_buf_off = 0;
	ltc_enc_byte = 0;
}

int
Session::ltc_tx_send_time_code_for_cycle (framepos_t start_frame, framepos_t end_frame, 
		double target_speed, double current_speed,
		pframes_t nframes)
{
	assert(nframes > 0);

	const float smult = 2.0/(3.0*255.0);
	SMPTETimecode tc;
	jack_default_audio_sample_t *out;
	pframes_t txf = 0;
	boost::shared_ptr<Port> ltcport = engine().ltc_output_port();

	if (!ltc_encoder || !ltc_enc_buf || !ltcport || ! ltcport->jack_port()) return 0;

	out = (jack_default_audio_sample_t*) jack_port_get_buffer (ltcport->jack_port(), nframes);
	if (!out) return 0;

	if (engine().freewheeling() || !Config->get_send_ltc()) {
		memset(out, 0, nframes * sizeof(jack_default_audio_sample_t));
		return nframes;
	}

	DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX %1 to %2 / %3\n", start_frame, end_frame, nframes));

	/* all systems go. Now here's the plan:
	 *
	 *  1) check if fps has changed
	 *  2) check direction of encoding, calc speed
	 *  3) calculate frame and byte to send aligned to jack-period size
	 *  4) check if it's the frame/byte that is already in the queue
	 *  5) if (4) mismatch, re-calculate offset of LTC frame relative to period size
	 *  6) actual LTC audio output
	 *  6a) send remaining part of already queued frame; break on nframes
	 *  6b) encode new LTC-frame byte
	 *  6c) goto 6a
	 *  7) done
	 */

	// (1)
	TimecodeFormat cur_timecode = config.get_timecode_format();
	if (cur_timecode != ltc_enc_tcformat) {
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX1: TC format mismatch - reinit sr: %1 fps: %2\n", nominal_frame_rate(), timecode_to_frames_per_second(cur_timecode)));
		if (ltc_encoder_reinit(ltc_encoder, nominal_frame_rate(), timecode_to_frames_per_second(cur_timecode), 0)) {
			PBD::error << _("LTC encoder: invalid framerate - LTC encoding is disabled for the remainder of this session.") << endmsg;
			ltc_tx_cleanup();
			return 0;
		}
		ltc_enc_tcformat = cur_timecode;
		ltc_tx_reset();
	}

	/* LTC is max. 30 fps */
	if (timecode_to_frames_per_second(cur_timecode) > 30) {
		memset(out, 0, nframes * sizeof(jack_default_audio_sample_t));
		return nframes;
	}

	// (2)
#define SIGNUM(a) ( (a) < 0 ? -1 : 1)

	framepos_t cycle_start_frame = (current_speed < 0) ? end_frame :start_frame;
	double new_ltc_speed = double(labs(end_frame - start_frame) * SIGNUM(current_speed)) / double(nframes);

	if (SIGNUM(new_ltc_speed) != SIGNUM (ltc_speed)) {
		// transport changed direction
		DEBUG_TRACE (DEBUG::LTC, "LTC TX2: transport direction changed\n");
		ltc_tx_reset();
	}

	if (ltc_speed != current_speed || target_speed != fabs(current_speed)) {
		/* TODO check ./libs/ardour/interpolation.cc  CubicInterpolation::interpolate
		 * if target_speed != current_speed we should interpolate, too.
		 *
		 * However, currenly in A3 target_speed == current_speed for each process cycle
		 * (except for the sign). Besides, above speed calulation uses the
		 * difference (end_frame - start_frame).
		 * end_frame is calculated from 'frames_moved' which includes the interpolation.
		 * so we're good, except for the fact that each LTC byte is sent at fixed speed.
		 */
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX2: speed change old: %1 cur: %2 tgt: %3 ctd: %4\n", ltc_speed, current_speed, target_speed, fabs(current_speed) - target_speed));
	}

	if (end_frame == start_frame || fabs(current_speed) < 0.1 ) {
		DEBUG_TRACE (DEBUG::LTC, "LTC TX2: transport is not rolling or absolute-speed < 0.1\n");
		/* keep repeating current frame
		 *
		 * an LTC generator must be able to continue generating LTC when Ardours transport is in stop
		 * some machines do odd things if LTC goes away:
		 * e.g. a tape based machine (video or audio), some think they have gone into park if LTC goes away,
		 * so unspool the tape from the playhead. That might be inconvenient.
		 * If LTC keeps arriving they remain in a stop position with the tape on the playhead.
		 */
		new_ltc_speed = 0;
	}

	ltc_speed = new_ltc_speed;
	DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX2: transport speed %1.\n", ltc_speed));

	if (fabs(ltc_speed) > 10.0) {
		DEBUG_TRACE (DEBUG::LTC, "LTC TX2: speed is out of bounds.\n");
		ltc_tx_reset();
		memset(out, 0, nframes * sizeof(jack_default_audio_sample_t));
		return nframes;
	}

	// (3)
	Timecode::Time tc_start;
	framepos_t tc_sample_start;

	/* calc timecode frame from current position - round down to nearest timecode */
	int boff;
	if (ltc_speed == 0) {
		boff = 0;
	}	else if (ltc_speed < 0) {
		boff = (ltc_enc_byte - 9) * frames_per_timecode_frame() / 10;
	} else {
		boff = ltc_enc_byte * frames_per_timecode_frame() / 10;
	}

	sample_to_timecode(cycle_start_frame
			- boff
			- ltc_speed * (ltc_buf_len - ltc_buf_off),
			tc_start, true, false);

	/* convert timecode back to sample-position */
	Timecode::timecode_to_sample (tc_start, tc_sample_start, true, false,
		double(frame_rate()),
		config.get_subframes_per_frame(),
		config.get_timecode_offset_negative(), config.get_timecode_offset()
		);

	/* difference between current frame and TC frame in samples */
	frameoffset_t soff = cycle_start_frame - tc_sample_start;
	DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX3: now: %1 tra: %2 eoff %3\n", cycle_start_frame, tc_sample_start, soff));

	// (4)
	DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX4: enc: %1 boff: %2 || enc-byte:%3\n", ltc_enc_pos, boff, ltc_enc_byte));
	if (ltc_enc_pos != tc_sample_start) {

		/* re-calc timecode w/o buffer offset */
		sample_to_timecode(cycle_start_frame, tc_start, true, false);
		Timecode::timecode_to_sample (tc_start, tc_sample_start, true, false,
			double(frame_rate()),
			config.get_subframes_per_frame(),
			config.get_timecode_offset_negative(), config.get_timecode_offset()
			);
		soff = cycle_start_frame - tc_sample_start;
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX4: now: %1 trs: %2 toff %3\n", cycle_start_frame, tc_sample_start, soff));

		tc.hours = tc_start.hours;
		tc.mins = tc_start.minutes;
		tc.secs = tc_start.seconds;
		tc.frame = tc_start.frames;
		ltc_encoder_set_timecode(ltc_encoder, &tc);

#if 1
		/* workaround for libltc recognizing 29.97 and 30000/1001 as drop-frame TC.
		 * in A3 only 30000/1001 is drop-frame and there are also other questionable
		 * DF timecodes  such as 24k/1001 and 25k/1001.
		 */
		LTCFrame ltcframe;
		ltc_encoder_get_frame(ltc_encoder, &ltcframe);
		ltcframe.dfbit = timecode_has_drop_frames(cur_timecode)?1:0;
		ltc_encoder_set_frame(ltc_encoder, &ltcframe);
#endif

		// (5)
		int fdiff = 0;
		if (soff < 0) {
			fdiff = ceil(-soff / frames_per_timecode_frame());
			soff += fdiff * frames_per_timecode_frame();
			for (int i=0; i < fdiff; ++i) {
				ltc_encoder_inc_timecode(ltc_encoder);
			}
		}
		else if (soff >= frames_per_timecode_frame()) {
			fdiff = floor(soff / frames_per_timecode_frame());
			soff -= fdiff * frames_per_timecode_frame();
			for (int i=0; i < fdiff; ++i) {
				ltc_encoder_dec_timecode(ltc_encoder);
			}
		}

		assert(soff >= 0 && soff < frames_per_timecode_frame());

		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX5 restart encoder fdiff %1 sdiff %2\n", fdiff, soff));
		ltc_tx_reset();

		if (ltc_speed == 0) {
			// offset is irrelevant when not rolling
			soff = 0;
		}
		if (soff > 0 && soff <= nframes) {
			txf=soff;
			memset(out, 0, soff * sizeof(jack_default_audio_sample_t));
		} else if (soff > 0) {
			/* resync next cycle */
			memset(out, 0, soff * sizeof(jack_default_audio_sample_t));
			return nframes;
		}

		ltc_enc_byte = soff * 10 / frames_per_timecode_frame();

		if (ltc_speed < 0 ) {
			ltc_enc_byte = 9 - ltc_enc_byte;
		}

		ltc_enc_pos = tc_sample_start;
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX5 restart @ %1 byte %2\n", ltc_enc_pos, ltc_enc_byte));
	}

	// (6)
	while (1) {
#ifdef LTC_GEN_FRAMEDBUG
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX6.1 @%1  [ %2 / %3 ]\n", txf, ltc_buf_off, ltc_buf_len));
#endif
		// (6a)
		while ((ltc_buf_off < ltc_buf_len) && (txf < nframes)) {
			const float v1 = ltc_enc_buf[ltc_buf_off++] - 128.0;
			const jack_default_audio_sample_t val = (jack_default_audio_sample_t) (v1*smult);
			out[txf++] = val;
		}
#ifdef LTC_GEN_FRAMEDBUG
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX6.2 @%1  [ %2 / %3 ]\n", txf, ltc_buf_off, ltc_buf_len));
#endif

		if (txf >= nframes) {
			DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX7 txf = %1 nframes = %2\n", txf, nframes));
			return nframes;
		}

		ltc_buf_len = 0;
		ltc_buf_off = 0;

		// (6b)

		if (SIGNUM(ltc_speed) == -1) {
			ltc_enc_byte = (ltc_enc_byte + 9)%10;
			if (ltc_enc_byte == 9) {
				ltc_encoder_dec_timecode(ltc_encoder);
#if 0 // this does not work for fractional or drop-frame TC
				ltc_enc_pos -= frames_per_timecode_frame();
#else // TODO make this a function
				SMPTETimecode enctc;
				Timecode::Time a3tc;
				ltc_encoder_get_timecode(ltc_encoder, &enctc);

				a3tc.hours   = enctc.hours ;
				a3tc.minutes = enctc.mins  ;
				a3tc.seconds = enctc.secs  ;
				a3tc.frames  = enctc.frame ;
				a3tc.rate = timecode_to_frames_per_second(cur_timecode);
				a3tc.drop = timecode_has_drop_frames(cur_timecode);

				Timecode::timecode_to_sample (a3tc, ltc_enc_pos, true, false,
					double(frame_rate()),
					config.get_subframes_per_frame(),
					config.get_timecode_offset_negative(), config.get_timecode_offset()
					);
#endif
			}
		}

		if (ltc_encoder_encode_byte(ltc_encoder, ltc_enc_byte, (ltc_speed==0)?1.0:(1.0/ltc_speed))) {
			DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX6.3 encoder error byte %1\n", ltc_enc_byte));
			ltc_encoder_buffer_flush(ltc_encoder);
			ltc_tx_reset();
			memset(out, 0, nframes * sizeof(jack_default_audio_sample_t));
			return -1;
		}
		int enc_frames = ltc_encoder_get_buffer(ltc_encoder, &(ltc_enc_buf[ltc_buf_len]));
#ifdef LTC_GEN_FRAMEDBUG
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX6.3 encoded %1 bytes LTC-byte %2 at spd %3\n", enc_frames, ltc_enc_byte, ltc_speed));
#endif
		if (enc_frames <=0) {
			DEBUG_TRACE (DEBUG::LTC, "LTC TX6.3 encoder empty buffer.\n");
			ltc_encoder_buffer_flush(ltc_encoder);
			ltc_tx_reset();
			memset(out, 0, nframes * sizeof(jack_default_audio_sample_t));
			return -1;
		}

		ltc_buf_len += enc_frames;

		if (SIGNUM(ltc_speed) == 1) {
			ltc_enc_byte = (ltc_enc_byte + 1)%10;
			if (ltc_enc_byte == 0 && ltc_speed != 0) {
				ltc_encoder_inc_timecode(ltc_encoder);
#if 0 // this does not work for fractional or drop-frame TC
				ltc_enc_pos += frames_per_timecode_frame();
#else // TODO make this a function
				SMPTETimecode enctc;
				Timecode::Time a3tc;
				ltc_encoder_get_timecode(ltc_encoder, &enctc);

				a3tc.hours   = enctc.hours ;
				a3tc.minutes = enctc.mins  ;
				a3tc.seconds = enctc.secs  ;
				a3tc.frames  = enctc.frame ;
				a3tc.rate = timecode_to_frames_per_second(cur_timecode);
				a3tc.drop = timecode_has_drop_frames(cur_timecode);

				Timecode::timecode_to_sample (a3tc, ltc_enc_pos, true, false,
					double(frame_rate()),
					config.get_subframes_per_frame(),
					config.get_timecode_offset_negative(), config.get_timecode_offset()
					);
#endif
			}
		}
#ifdef LTC_GEN_FRAMEDBUG
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX6.4 fno: %1  [ %2 / %3 ] spd %4\n", ltc_enc_pos, ltc_buf_off, ltc_buf_len, ltc_speed));
#endif
	}

	return nframes;
}
