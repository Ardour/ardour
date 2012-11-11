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

#include "timecode/time.h"

#include "ardour/audioengine.h"
#include "ardour/audio_port.h"
#include "ardour/debug.h"
#include "ardour/io.h"
#include "ardour/session.h"
#include "ardour/slave.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace MIDI;
using namespace PBD;
using namespace Timecode;

/* really verbose timing debug */
//#define LTC_GEN_FRAMEDBUG
//#define LTC_GEN_TXDBUG

#ifndef MAX
#define MAX(a,b) ( (a) > (b) ? (a) : (b) )
#endif
#ifndef MIN
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#endif

/* LTC signal should have a rise time of 25 us +/- 5 us.
 * yet with most sound-cards a square-wave of 1-2 sample
 * introduces ringing and small oscillations.
 * https://en.wikipedia.org/wiki/Gibbs_phenomenon
 * A low-pass filter in libltc can reduce this at
 * the cost of being slightly out of spec WRT to rise-time.
 *
 * This filter is adaptive so that fast vari-speed signals
 * will not be affected by it.
 */
#define LTC_RISE_TIME(speed) MIN (100, MAX(25, (4000000 / ((speed==0)?1:speed) / engine().frame_rate())))

void
Session::ltc_tx_initialize()
{
	ltc_enc_tcformat = config.get_timecode_format();

	DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX init sr: %1 fps: %2\n", nominal_frame_rate(), timecode_to_frames_per_second(ltc_enc_tcformat)));
	ltc_encoder = ltc_encoder_create(nominal_frame_rate(),
			timecode_to_frames_per_second(ltc_enc_tcformat),
			-2);

	ltc_encoder_set_bufsize(ltc_encoder, nominal_frame_rate(), 23.0);
	ltc_encoder_set_filter(ltc_encoder, LTC_RISE_TIME(1.0));

	/* buffersize for 1 LTC frame: (1 + sample-rate / fps) bytes
	 * usually returned by ltc_encoder_get_buffersize(encoder)
	 *
	 * since the fps can change and A3's  min fps: 24000/1001 */
	ltc_enc_buf = (ltcsnd_sample_t*) calloc((nominal_frame_rate() / 23), sizeof(ltcsnd_sample_t));
	ltc_speed = 0;
	ltc_tx_reset();
	ltc_tx_resync_latency();
	Xrun.connect_same_thread (*this, boost::bind (&Session::ltc_tx_reset, this));
	engine().GraphReordered.connect_same_thread (*this, boost::bind (&Session::ltc_tx_resync_latency, this));
	restarting = false;
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
Session::ltc_tx_resync_latency()
{
	DEBUG_TRACE (DEBUG::LTC, "LTC TX resync latency\n");
	if (!deletion_in_progress()) {
		boost::shared_ptr<Port> ltcport = ltc_output_port();
		if (ltcport) {
			ltcport->get_connected_latency_range(ltc_out_latency, true);
		}
	}
}

void
Session::ltc_tx_reset()
{
	DEBUG_TRACE (DEBUG::LTC, "LTC TX reset\n");
	ltc_enc_pos = -9999; // force re-start
	ltc_buf_len = 0;
	ltc_buf_off = 0;
	ltc_enc_byte = 0;
	ltc_enc_cnt = 0;

	ltc_encoder_reset(ltc_encoder);
}

void
Session::ltc_tx_recalculate_position()
{
	SMPTETimecode enctc;
	Timecode::Time a3tc;
	ltc_encoder_get_timecode(ltc_encoder, &enctc);

	a3tc.hours   = enctc.hours;
	a3tc.minutes = enctc.mins;
	a3tc.seconds = enctc.secs;
	a3tc.frames  = enctc.frame;
	a3tc.rate = timecode_to_frames_per_second(ltc_enc_tcformat);
	a3tc.drop = timecode_has_drop_frames(ltc_enc_tcformat);

	Timecode::timecode_to_sample (a3tc, ltc_enc_pos, true, false,
		(double)frame_rate(),
		config.get_subframes_per_frame(),
		config.get_timecode_generator_offset_negative(), config.get_timecode_generator_offset()
		);
	restarting = false;
}

void
Session::ltc_tx_send_time_code_for_cycle (framepos_t start_frame, framepos_t end_frame,
					  double target_speed, double current_speed,
					  pframes_t nframes)
{
	assert (nframes > 0);

	Sample *out;
	pframes_t txf = 0;
	boost::shared_ptr<Port> ltcport = ltc_output_port();

	Buffer& buf (ltcport->get_buffer (nframes));

	if (!ltc_encoder || !ltc_enc_buf) {
		return;
	}

	SyncSource sync_src = Config->get_sync_source();
	if (engine().freewheeling() || !Config->get_send_ltc()
	    /* TODO
	     * decide which time-sources we can generated LTC from.
	     * Internal, JACK or sample-synced slaves should be fine.
	     * talk to oofus.
	     *
	     || (config.get_external_sync() && sync_src == LTC)
	     || (config.get_external_sync() && sync_src == MTC)
	    */
	     ||(config.get_external_sync() && sync_src == MIDIClock)
		) {
		return;
	}

	out = dynamic_cast<AudioBuffer*>(&buf)->data ();

	/* range from libltc (38..218) || - 128.0  -> (-90..90) */
	const float ltcvol = Config->get_ltc_output_volume()/(90.0); // pow(10, db/20.0)/(90.0);

	DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX %1 to %2 / %3 | lat: %4\n", start_frame, end_frame, nframes, ltc_out_latency.max));

	/* all systems go. Now here's the plan:
	 *
	 *  1) check if fps has changed
	 *  2) check direction of encoding, calc speed, re-sample existing buffer
	 *  3) calculate frame and byte to send aligned to jack-period size
	 *  4) check if it's the frame/byte that is already in the queue
	 *  5) if (4) mismatch, re-calculate offset of LTC frame relative to period size
	 *  6) actual LTC audio output
	 *  6a) send remaining part of already queued frame; break on nframes
	 *  6b) encode new LTC-frame byte
	 *  6c) goto 6a
	 *  7) done
	 */

	// (1) check fps
	TimecodeFormat cur_timecode = config.get_timecode_format();
	if (cur_timecode != ltc_enc_tcformat) {
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX1: TC format mismatch - reinit sr: %1 fps: %2\n", nominal_frame_rate(), timecode_to_frames_per_second(cur_timecode)));
		if (ltc_encoder_reinit(ltc_encoder, nominal_frame_rate(), timecode_to_frames_per_second(cur_timecode), -2)) {
			PBD::error << _("LTC encoder: invalid framerate - LTC encoding is disabled for the remainder of this session.") << endmsg;
			ltc_tx_cleanup();
			return;
		}
		ltc_encoder_set_filter(ltc_encoder, LTC_RISE_TIME(ltc_speed));
		ltc_enc_tcformat = cur_timecode;
		ltc_tx_reset();
	}

	/* LTC is max. 30 fps */
	if (timecode_to_frames_per_second(cur_timecode) > 30) {
		return;
	}

	// (2) speed & direction

	/* speed 0 aka transport stopped is interpreted as rolling forward.
	 * keep repeating current frame
	 */
#define SIGNUM(a) ( (a) < 0 ? -1 : 1)
	bool speed_changed = false;

	/* port latency compensation:
	 * The _generated timecode_ is offset by the port-latency,
	 * therefore the offset depends on the direction of transport.
	 */
	framepos_t cycle_start_frame = (current_speed < 0) ? (start_frame - ltc_out_latency.max) : (start_frame + ltc_out_latency.max);

	/* cycle-start may become negative due to latency compensation */
	if (cycle_start_frame < 0) { cycle_start_frame = 0; }

	double new_ltc_speed = (double)(labs(end_frame - start_frame) * SIGNUM(current_speed)) / (double)nframes;
	if (nominal_frame_rate() != frame_rate()) {
		new_ltc_speed *= (double)nominal_frame_rate() / (double)frame_rate();
	}

	if (SIGNUM(new_ltc_speed) != SIGNUM (ltc_speed)) {
		DEBUG_TRACE (DEBUG::LTC, "LTC TX2: transport changed direction\n");
		ltc_tx_reset();
	}

	if (ltc_speed != new_ltc_speed) {
		/* check ./libs/ardour/interpolation.cc  CubicInterpolation::interpolate
		 * if target_speed != current_speed we should interpolate, too.
		 *
		 * However, currency in A3 target_speed == current_speed for each process cycle
		 * (except for the sign and if target_speed > 8.0).
		 * Besides, above speed calculation uses the difference (end_frame - start_frame).
		 * end_frame is calculated from 'frames_moved' which includes the interpolation.
		 * so we're good.
		 */
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX2: speed change old: %1 cur: %2 tgt: %3 ctd: %4\n", ltc_speed, current_speed, target_speed, fabs(current_speed) - target_speed));
		speed_changed = true;
		ltc_encoder_set_filter(ltc_encoder, LTC_RISE_TIME(new_ltc_speed));
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
		if (!Config->get_ltc_send_continuously()) {
			ltc_speed = new_ltc_speed;
			return;
		}
	}

	if (fabs(new_ltc_speed) > 10.0) {
		DEBUG_TRACE (DEBUG::LTC, "LTC TX2: speed is out of bounds.\n");
		ltc_tx_reset();
		return;
	}

	if (ltc_speed == 0 && new_ltc_speed != 0) {
		DEBUG_TRACE (DEBUG::LTC, "LTC TX2: transport started rolling - reset\n");
		ltc_tx_reset();
	}

	/* the timecode duration corresponding to the samples that are still
	 * in the buffer. Here, the speed of previous cycle is used to calculate
	 * the alignment at the beginning of this cycle later.
	 */
	double poff = (ltc_buf_len - ltc_buf_off) * ltc_speed;

	if (speed_changed && new_ltc_speed != 0) {
		/* we need to re-sample the existing buffer.
		 * "make space for the en-coder to catch up to the new speed"
		 *
		 * since the LTC signal is a rectangular waveform we can simply squeeze it
		 * by removing samples or duplicating samples /here and there/.
		 *
		 * There may be a more elegant way to do this, in fact one could
		 * simply re-render the buffer using ltc_encoder_encode_byte()
		 * but that'd require some timecode offset buffer magic,
		 * which is left for later..
		 */

		double oldbuflen = (double)(ltc_buf_len - ltc_buf_off);
		double newbuflen = (double)(ltc_buf_len - ltc_buf_off) * fabs(ltc_speed / new_ltc_speed);

		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX2: bufOld %1 bufNew %2 | diff %3\n",
					(ltc_buf_len - ltc_buf_off), newbuflen, newbuflen - oldbuflen
					));

		double bufrspdiff = rint(newbuflen - oldbuflen);

		if (abs(bufrspdiff) > newbuflen || abs(bufrspdiff) > oldbuflen) {
			DEBUG_TRACE (DEBUG::LTC, "LTC TX2: resampling buffer would destroy information.\n");
			ltc_tx_reset();
			poff = 0;
		} else if (bufrspdiff != 0 && newbuflen > oldbuflen) {
			int incnt = 0;
			double samples_to_insert = ceil(newbuflen - oldbuflen);
			double avg_distance = newbuflen / samples_to_insert;
			DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX2: resample buffer insert: %1\n", samples_to_insert));

			for (int rp = ltc_buf_off; rp < ltc_buf_len - 1; ++rp) {
				const int ro = rp - ltc_buf_off;
				if (ro < (incnt*avg_distance)) continue;
				const ltcsnd_sample_t v1 = ltc_enc_buf[rp];
				const ltcsnd_sample_t v2 = ltc_enc_buf[rp+1];
				if (v1 != v2 && ro < ((incnt+1)*avg_distance)) continue;
				memmove(&ltc_enc_buf[rp+1], &ltc_enc_buf[rp], ltc_buf_len-rp);
				incnt++;
				ltc_buf_len++;
			}
		} else if (bufrspdiff != 0 && newbuflen < oldbuflen) {
			double samples_to_remove = ceil(oldbuflen - newbuflen);
			DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX2: resample buffer - remove: %1\n", samples_to_remove));
			if (oldbuflen <= samples_to_remove) {
				ltc_buf_off = ltc_buf_len= 0;
			} else {
				double avg_distance = newbuflen / samples_to_remove;
				int rmcnt = 0;
				for (int rp = ltc_buf_off; rp < ltc_buf_len - 1; ++rp) {
					const int ro = rp - ltc_buf_off;
					if (ro < (rmcnt*avg_distance)) continue;
					const ltcsnd_sample_t v1 = ltc_enc_buf[rp];
					const ltcsnd_sample_t v2 = ltc_enc_buf[rp+1];
					if (v1 != v2 && ro < ((rmcnt+1)*avg_distance)) continue;
					memmove(&ltc_enc_buf[rp], &ltc_enc_buf[rp+1], ltc_buf_len-rp-1);
					ltc_buf_len--;
					rmcnt++;
				}
			}
		}
	}

	ltc_speed = new_ltc_speed;
	DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX2: transport speed %1.\n", ltc_speed));

	// (3) bit/sample alignment
	Timecode::Time tc_start;
	framepos_t tc_sample_start;

	/* calc timecode frame from current position - round down to nearest timecode */
	Timecode::sample_to_timecode(cycle_start_frame, tc_start, true, false,
			timecode_frames_per_second(),
			timecode_drop_frames(),
			(double)frame_rate(),
			config.get_subframes_per_frame(),
			config.get_timecode_generator_offset_negative(), config.get_timecode_generator_offset()
			);

	/* convert timecode back to sample-position */
	Timecode::timecode_to_sample (tc_start, tc_sample_start, true, false,
		(double)frame_rate(),
		config.get_subframes_per_frame(),
		config.get_timecode_generator_offset_negative(), config.get_timecode_generator_offset()
		);

	/* difference between current frame and TC frame in samples */
	frameoffset_t soff = cycle_start_frame - tc_sample_start;
	DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX3: A3cycle: %1 = A3tc: %2 +off: %3\n",
				cycle_start_frame, tc_sample_start, soff));


	// (4) check if alignment matches
	const double fptcf = frames_per_timecode_frame();

	/* maximum difference of bit alignment in audio-samples.
	 *
	 * if transport and LTC generator differs more than this, the LTC
	 * generator will be re-initialized
	 *
	 * due to rounding error and variations in LTC-bit duration depending
	 * on the speed, it can be off by +- ltc_speed audio-samples.
	 * When the playback speed changes, it can actually reach +- 2 * ltc_speed
	 * in the cycle _after_ the speed changed. The average delta however is 0.
	 */
	double maxdiff;

	if (config.get_external_sync() && slave()) {
		maxdiff = slave()->resolution();
	} else {
		maxdiff = ceil(fabs(ltc_speed))*2.0;
		if (nominal_frame_rate() != frame_rate()) {
			maxdiff *= 3.0;
		}
		if (ltc_enc_tcformat == Timecode::timecode_23976 || ltc_enc_tcformat == Timecode::timecode_24976) {
			maxdiff *= 15.0;
		}
	}

	DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX4: enc: %1 + %2 - %3 || buf-bytes: %4 enc-byte: %5\n",
				ltc_enc_pos, ltc_enc_cnt, poff, (ltc_buf_len - ltc_buf_off), poff, ltc_enc_byte));

	DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX4: enc-pos: %1  | d: %2\n",
				ltc_enc_pos + ltc_enc_cnt - poff,
				rint(ltc_enc_pos + ltc_enc_cnt - poff) - cycle_start_frame
				));

	if (ltc_enc_pos < 0
			|| (ltc_speed != 0 && fabs(ceil(ltc_enc_pos + ltc_enc_cnt - poff) - cycle_start_frame) > maxdiff)
			) {

		// (5) re-align
		ltc_tx_reset();

		/* set frame to encode */
		SMPTETimecode tc;
		tc.hours = tc_start.hours;
		tc.mins = tc_start.minutes;
		tc.secs = tc_start.seconds;
		tc.frame = tc_start.frames;
		ltc_encoder_set_timecode(ltc_encoder, &tc);

		/* workaround for libltc recognizing 29.97 and 30000/1001 as drop-frame TC.
		 * In A3 30000/1001 or 30 fps can be drop-frame.
		 */
		LTCFrame ltcframe;
		ltc_encoder_get_frame(ltc_encoder, &ltcframe);
		ltcframe.dfbit = timecode_has_drop_frames(cur_timecode)?1:0;
		ltc_encoder_set_frame(ltc_encoder, &ltcframe);


		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX4: now: %1 trs: %2 toff %3\n", cycle_start_frame, tc_sample_start, soff));

		uint32_t cyc_off;
		if (soff < 0 || soff >= fptcf) {
			/* session framerate change between (2) and now */
			ltc_tx_reset();
			return;
		}

		if (ltc_speed < 0 ) {
			/* calculate the byte that starts at or after the current position */
			ltc_enc_byte = floor((10.0 * soff) / (fptcf));
			ltc_enc_cnt = ltc_enc_byte * fptcf / 10.0;

			/* calculate difference between the current position and the byte to send */
			cyc_off = soff- ceil(ltc_enc_cnt);

		} else {
			/* calculate the byte that starts at or after the current position */
			ltc_enc_byte = ceil((10.0 * soff) / fptcf);
			ltc_enc_cnt = ltc_enc_byte * fptcf / 10.0;

			/* calculate difference between the current position and the byte to send */
			cyc_off = ceil(ltc_enc_cnt) - soff;

			if (ltc_enc_byte == 10) {
				ltc_enc_byte = 0;
				ltc_encoder_inc_timecode(ltc_encoder);
			}
		}

		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX5 restart encoder: soff %1 byte %2 cycoff %3\n",
					soff, ltc_enc_byte, cyc_off));

		if ( (ltc_speed < 0 && ltc_enc_byte !=9 ) || (ltc_speed >= 0 && ltc_enc_byte !=0 ) ) {
			restarting = true;
		}

		if (cyc_off > 0 && cyc_off <= nframes) {
			/* offset in this cycle */
			txf= rint(cyc_off / fabs(ltc_speed));
			memset(out, 0, cyc_off * sizeof(Sample));
		} else {
			/* resync next cycle */
			memset(out, 0, nframes * sizeof(Sample));
			return;
		}

		ltc_enc_pos = tc_sample_start;

		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX5 restart @ %1 + %2 - %3 |  byte %4\n",
					ltc_enc_pos, ltc_enc_cnt, cyc_off, ltc_enc_byte));
	}
	else if (ltc_speed != 0 && (fptcf / ltc_speed / 80) > 3 ) {
		/* reduce (low freq) jitter.
		 * The granularity of the LTC encoder speed is 1 byte =
		 * (frames-per-timecode-frame / 10) audio-samples.
		 * Thus, tiny speed changes [as produced by some slaves]
		 * may not have any effect in the cycle when they occur,
		 * but they will add up over time.
		 *
		 * This is a linear approx to compensate for this jitter
		 * and prempt re-sync when the drift builds up.
		 *
		 * However, for very fast speeds - when 1 LTC bit is
		 * <= 3 audio-sample - adjusting speed may lead to
		 * invalid frames.
		 *
		 * To do better than this, resampling (or a rewrite of the
		 * encoder) is required.
		 */
		ltc_speed -= ((ltc_enc_pos + ltc_enc_cnt - poff) - cycle_start_frame) / engine().frame_rate();
	}


	// (6) encode and output
	while (1) {
#ifdef LTC_GEN_TXDBUG
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX6.1 @%1  [ %2 / %3 ]\n", txf, ltc_buf_off, ltc_buf_len));
#endif
		// (6a) send remaining buffer
		while ((ltc_buf_off < ltc_buf_len) && (txf < nframes)) {
			const float v1 = ltc_enc_buf[ltc_buf_off++] - 128.0;
			const Sample val = (Sample) (v1*ltcvol);
			out[txf++] = val;
		}
#ifdef LTC_GEN_TXDBUG
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX6.2 @%1  [ %2 / %3 ]\n", txf, ltc_buf_off, ltc_buf_len));
#endif

		if (txf >= nframes) {
			DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX7 enc: %1 [ %2 / %3 ] byte: %4 spd %5 fpp %6 || nf: %7\n",
						ltc_enc_pos, ltc_buf_off, ltc_buf_len, ltc_enc_byte, ltc_speed, nframes, txf));
			break;
		}

		ltc_buf_len = 0;
		ltc_buf_off = 0;

		// (6b) encode LTC, bump timecode

		if (ltc_speed < 0) {
			ltc_enc_byte = (ltc_enc_byte + 9)%10;
			if (ltc_enc_byte == 9) {
				ltc_encoder_dec_timecode(ltc_encoder);
				ltc_tx_recalculate_position();
				ltc_enc_cnt = fptcf;
			}
		}

		int enc_frames;

		if (restarting) {
			/* write zero bytes -- don't touch encoder until we're at a frame-boundary
			 * otherwise the biphase polarity may be inverted.
			 */
			enc_frames = fptcf / 10.0;
			memset(&ltc_enc_buf[ltc_buf_len], 127, enc_frames * sizeof(ltcsnd_sample_t));
		} else {
			if (ltc_encoder_encode_byte(ltc_encoder, ltc_enc_byte, (ltc_speed==0)?1.0:(1.0/ltc_speed))) {
				DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX6.3 encoder error byte %1\n", ltc_enc_byte));
				ltc_encoder_buffer_flush(ltc_encoder);
				ltc_tx_reset();
				return;
			}
			enc_frames = ltc_encoder_get_buffer(ltc_encoder, &(ltc_enc_buf[ltc_buf_len]));
		}

#ifdef LTC_GEN_FRAMEDBUG
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX6.3 encoded %1 bytes for LTC-byte %2 at spd %3\n", enc_frames, ltc_enc_byte, ltc_speed));
#endif
		if (enc_frames <=0) {
			DEBUG_TRACE (DEBUG::LTC, "LTC TX6.3 encoder empty buffer.\n");
			ltc_encoder_buffer_flush(ltc_encoder);
			ltc_tx_reset();
			return;
		}

		ltc_buf_len += enc_frames;
		if (ltc_speed < 0)
			ltc_enc_cnt -= fptcf/10.0;
		else
			ltc_enc_cnt += fptcf/10.0;

		if (ltc_speed >= 0) {
			ltc_enc_byte = (ltc_enc_byte + 1)%10;
			if (ltc_enc_byte == 0 && ltc_speed != 0) {
				ltc_encoder_inc_timecode(ltc_encoder);
#if 0 /* force fixed parity -- scope debug */
				LTCFrame f;
				ltc_encoder_get_frame(ltc_encoder, &f);
				f.biphase_mark_phase_correction=0;
				ltc_encoder_set_frame(ltc_encoder, &f);
#endif
				ltc_tx_recalculate_position();
				ltc_enc_cnt = 0;
			} else if (ltc_enc_byte == 0) {
				ltc_enc_cnt = 0;
				restarting=false;
			}
		}
#ifdef LTC_GEN_FRAMEDBUG
		DEBUG_TRACE (DEBUG::LTC, string_compose("LTC TX6.4 enc-pos: %1 + %2 [ %4 / %5 ] spd %6\n", ltc_enc_pos, ltc_enc_cnt, ltc_buf_off, ltc_buf_len, ltc_speed));
#endif
	}

	dynamic_cast<AudioBuffer*>(&buf)->set_written (true);
	return;
}
