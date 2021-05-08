/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __libbackend_alsa_slave_h__
#define __libbackend_alsa_slave_h__

#include <pthread.h>

#include "pbd/ringbuffer.h"
#include "pbd/g_atomic_compat.h"

#include "zita-resampler/vresampler.h"
#include "zita-alsa-pcmi.h"

namespace ARDOUR {

class AlsaAudioSlave
{
public:
	AlsaAudioSlave (
			const char   *play_name,
			const char   *capt_name,
			unsigned int  master_rate,
			unsigned int  master_samples_per_period,
			unsigned int  slave_rate,
			unsigned int  slave_samples_per_period,
			unsigned int  periods_per_cycle);

	virtual ~AlsaAudioSlave ();

	bool start ();
	void stop ();

	void cycle_start (double, double, bool);
	void cycle_end ();

	uint32_t capt_chan (uint32_t chn, float* dst, uint32_t n_samples);
	uint32_t play_chan (uint32_t chn, float* src, uint32_t n_samples);

	bool running () const { return _active; }
	void freewheel (bool);

	int      state (void) const { return _pcmi.state (); }
	uint32_t nplay (void) const { return _pcmi.nplay (); }
	uint32_t ncapt (void) const { return _pcmi.ncapt (); }

	PBD::Signal0<void> Halted;

protected:
	virtual void update_latencies (uint32_t, uint32_t) = 0;

private:
	Alsa_pcmi _pcmi;

	static void* _process_thread (void *);
	void* process_thread ();
	pthread_t _thread;

	bool  _run; /* keep going or stop, ardour thread */
	bool  _active; /* is running, process thread */

	/* DLL, track slave process callback */
	double _t0, _t1;
	uint64_t _samples_since_dll_reset;

	double   _ratio;
	uint32_t _capt_latency;
	double   _play_latency;

	volatile double _slave_speed;

	GATOMIC_QUAL gint _draining;

	PBD::RingBuffer<float> _rb_capture;
	PBD::RingBuffer<float> _rb_playback;

	size_t _samples_per_period; // master

	float* _capt_buff;
	float* _play_buff;
	float* _src_buff;

	ArdourZita::VResampler _src_capt;
	ArdourZita::VResampler _src_play;

	static void reset_resampler (ArdourZita::VResampler&);

}; // class AlsaAudioSlave

} // namespace
#endif /* __libbackend_alsa_slave_h__ */
