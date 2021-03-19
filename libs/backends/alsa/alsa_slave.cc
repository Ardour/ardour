/*
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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


#include <cmath>
#include <glibmm.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/pthread_utils.h"

#include "alsa_slave.h"

#include "pbd/i18n.h" 

using namespace ARDOUR;

AlsaAudioSlave::AlsaAudioSlave (
			const char   *play_name,
			const char   *capt_name,
			unsigned int  master_rate,
			unsigned int  master_samples_per_period,
			unsigned int  slave_rate,
			unsigned int  slave_samples_per_period,
			unsigned int  periods_per_cycle)
	: _pcmi (play_name, capt_name, 0, slave_rate, slave_samples_per_period, periods_per_cycle, 2, /* Alsa_pcmi::DEBUG_ALL */ 0)
	, _run (false)
	, _active (false)
	, _samples_since_dll_reset (0)
	, _ratio (1.0)
	, _slave_speed (1.0)
	, _rb_capture (4 * /* AlsaAudioBackend::_max_buffer_size */ 8192 * _pcmi.ncapt ())
	, _rb_playback (4 * /* AlsaAudioBackend::_max_buffer_size */ 8192 * _pcmi.nplay ())
	, _samples_per_period (master_samples_per_period)
	, _capt_buff (0)
	, _play_buff (0)
	, _src_buff (0)
{
	g_atomic_int_set (&_draining, 1);

	if (0 != _pcmi.state()) {
		return;
	}

	/* from alsa-slave to master */
	_ratio = (double) master_rate / (double) _pcmi.fsamp();

#ifndef NDEBUG
	fprintf (stdout, " --[[ ALSA Slave %s/%s ratio: %.4f\n",
			capt_name ? capt_name : "-",
			play_name ? play_name : "-",
			_ratio);
	_pcmi.printinfo ();
	fprintf (stdout, " --]]\n");
#endif

	if (_pcmi.ncapt () > 0) {
		_src_capt.setup (_ratio, _pcmi.ncapt (), /*quality*/ 32); // save capture to master
		_src_capt.set_rrfilt (100);
		_capt_buff = (float*) malloc (sizeof(float) * _pcmi.ncapt () * _samples_per_period);
	}
	if (_pcmi.nplay () > 0) {
		_src_play.setup (1.0 / _ratio, _pcmi.nplay (), /*quality*/ 32); // master to slave play
		_src_play.set_rrfilt (100);
		_play_buff = (float*) malloc (sizeof(float) * _pcmi.nplay () * _samples_per_period);
	}

	if (_pcmi.nplay () > 0 || _pcmi.ncapt () > 0) {
		_src_buff  = (float*) malloc (sizeof(float) * std::max (_pcmi.nplay (), _pcmi.ncapt ()));
	}
}

AlsaAudioSlave::~AlsaAudioSlave ()
{
	stop ();
	free (_capt_buff);
	free (_play_buff);
	free (_src_buff);
}

void
AlsaAudioSlave::reset_resampler (ArdourZita::VResampler& src)
{
	src.reset ();
	src.inp_count = src.inpsize () - 1;
	src.out_count = 200000;
	src.process ();
}

bool
AlsaAudioSlave::start ()
{
	if (_run) {
		return false;
	}

	_run = true;
	if (pbd_realtime_pthread_create (PBD_SCHED_FIFO, PBD_RT_PRI_MAIN, PBD_RT_STACKSIZE_HELP,
				&_thread, _process_thread, this))
	{
		if (pbd_pthread_create (PBD_RT_STACKSIZE_HELP, &_thread, _process_thread, this)) {
			_run = false;
			PBD::error << _("AlsaAudioBackend: failed to create slave process thread.") << endmsg;
			return false;
		}
	}

	int timeout = 5000;
	while (!_active && --timeout > 0) { Glib::usleep (1000); }

	if (timeout == 0 || !_active) {
		_run = false;
		PBD::error << _("AlsaAudioBackend: failed to start slave process thread.") << endmsg;
		return false;
	}

	return true;
}

void
AlsaAudioSlave::stop ()
{
	void *status;
	if (!_run) {
		return;
	}

	_run = false;
	if (pthread_join (_thread, &status)) {
		PBD::error << _("AlsaAudioBackend: slave failed to terminate properly.") << endmsg;
	}
	_pcmi.pcm_stop ();
}

void*
AlsaAudioSlave::_process_thread (void* arg)
{
	AlsaAudioSlave* aas = static_cast<AlsaAudioSlave*> (arg);
	pthread_set_name ("AlsaAudioSlave");
	return aas->process_thread ();
}

void*
AlsaAudioSlave::process_thread ()
{
	 _active = true;

	bool reset_dll = true;
	int last_n_periods = 0;
	int no_proc_errors = 0;
	const int bailout = 2 * _pcmi.fsamp () / _pcmi.fsize ();

	double dll_dt = (double) _pcmi.fsize () / (double)_pcmi.fsamp ();
	double dll_w1 = 2 * M_PI * 0.1 * dll_dt;
	double dll_w2 = dll_w1 * dll_w1;

	const double sr_norm = 1e-6 * (double) _pcmi.fsamp () / (double) _pcmi.fsize ();

	_pcmi.pcm_start ();

	while (_run) {
		bool xrun = false;
		long nr = _pcmi.pcm_wait ();

		/* update DLL */
		uint64_t clock0 = g_get_monotonic_time();

		if (reset_dll || last_n_periods != 1) {
			reset_dll = false;
			dll_dt = 1e6 * (double) _pcmi.fsize () / (double) _pcmi.fsamp();
			_t0 = clock0;
			_t1 = clock0 + dll_dt;
			_samples_since_dll_reset = 0;
		} else {
			const double er = clock0 - _t1;
			_t0 = _t1;
			_t1 = _t1 + dll_w1 * er + dll_dt;
			dll_dt += dll_w2 * er;
			_samples_since_dll_reset += _pcmi.fsize ();
		}

		_slave_speed = (_t1 - _t0) * sr_norm; // XXX atomic

		if (_pcmi.state () > 0) {
			++no_proc_errors;
			xrun = true;
		}

		if (_pcmi.state () < 0) {
			PBD::error << _("AlsaAudioBackend: Slave I/O error.") << endmsg;
			break;
		}

		if (no_proc_errors > bailout) {
			PBD::error << _("AlsaAudioBackend: Slave terminated due to continuous xruns.") << endmsg;
			break;
		}

		const size_t spp = _pcmi.fsize ();
		const bool drain = g_atomic_int_get (&_draining);
		last_n_periods = 0;

		while (nr >= (long)spp) {
			no_proc_errors = 0;

			_pcmi.capt_init (spp);
			if (drain || _pcmi.ncapt () == 0) {
				/* do nothing */
			} else if (_rb_capture.write_space () >= _pcmi.ncapt () * spp) {
#if 0 // failsafe: write interleave sample by sample
				for (uint32_t s = 0; s < spp; ++s) {
					for (uint32_t c = 0; c < _pcmi.ncapt (); ++c) {
						float d;
						_pcmi.capt_chan (c, &d, 1);
						_rb_capture.write (&d, 1);
					}
				}
#else
				unsigned int nchn = _pcmi.ncapt ();
				PBD::RingBuffer<float>::rw_vector vec;
				_rb_capture.get_write_vector (&vec);
				if (vec.len[0] >= nchn * spp) {
					for (uint32_t c = 0; c < nchn; ++c) {
						_pcmi.capt_chan (c, vec.buf[0] + c, spp, nchn);
					}
				} else {
					uint32_t c;
					/* first copy continuous area */
					uint32_t k = vec.len[0] / nchn;
					for (c = 0; c < nchn; ++c) {
						_pcmi.capt_chan (c, vec.buf[0] + c, k, nchn);
					}

					/* possible samples at end of first buffer chunk, 
					 * incomplete audio-sample */
					uint32_t s = vec.len[0] - k * nchn;
					assert (s < nchn);

					for (c = 0; c < s; ++c) {
						_pcmi.capt_chan (c, vec.buf[0] + k * nchn + c, 1, nchn);
					}
					/* cont'd audio-sample at second ringbuffer chunk */
					for (; c < nchn; ++c) {
						_pcmi.capt_chan (c, vec.buf[1] + c - s, spp - k, nchn);
					}
					/* remaining data in 2nd area */
					for (c = 0; c < s; ++c) {
						_pcmi.capt_chan (c, vec.buf[1] + c + nchn - s, spp - k, nchn);
					}
				}
				_rb_capture.increment_write_idx (spp * nchn);
#endif
			} else {
				g_atomic_int_set (&_draining, 1);
			}
			_pcmi.capt_done (spp);


			if (drain) {
				_rb_playback.increment_read_idx (_rb_playback.read_space ());
			}

			_pcmi.play_init (spp);
			if (_pcmi.nplay () == 0) {
				/* relax */
			}
			else if (_rb_playback.read_space () >= _pcmi.nplay () * spp) {
#if 0 // failsafe: read sample by sample de-interleave
				for (uint32_t s = 0; s < spp; ++s) {
					for (uint32_t c = 0; c < _pcmi.nplay (); ++c) {
						float d;
						_rb_playback.read (&d, 1);
						_pcmi.play_chan (c, (const float*)&d, 1); 
					}
				}
#else
				unsigned int nchn = _pcmi.nplay ();
				PBD::RingBuffer<float>::rw_vector vec;
				_rb_playback.get_read_vector (&vec);
				if (vec.len[0] >= nchn * spp) {
					for (uint32_t c = 0; c < nchn; ++c) {
						_pcmi.play_chan (c, vec.buf[0] + c, spp, nchn);
					}
				} else {
					uint32_t c;
					uint32_t k = vec.len[0] / nchn;
					for (c = 0; c < nchn; ++c) {
						_pcmi.play_chan (c, vec.buf[0] + c, k, nchn);
					}

					uint32_t s = vec.len[0] - k * nchn;
					assert (s < nchn);

					for (c = 0; c < s; ++c) {
						_pcmi.play_chan (c, vec.buf[0] + k * nchn + c, 1, nchn);
					}

					for (; c < nchn; ++c) {
						_pcmi.play_chan (c, vec.buf[1] + c - s, spp - k, nchn);
					}
					for (c = 0; c < s; ++c) {
						_pcmi.play_chan (c, vec.buf[1] + c + nchn - s, spp - k, nchn);
					}
				}
				_rb_playback.increment_read_idx (spp * nchn);
#endif
			} else {
				if (!drain) {
#ifndef NDEBUG
					printf ("Slave Process: Playback Buffer Underflow, have %u want %lu\n", _rb_playback.read_space (), _pcmi.nplay () * spp); // XXX DEBUG 
#endif
					_play_latency += spp * _ratio;
					update_latencies (_play_latency, _capt_latency);
				}
				/* silence outputs */
				for (uint32_t c = 0; c < _pcmi.nplay (); ++c) {
					_pcmi.clear_chan (c, spp);
				}
			}
			_pcmi.play_done (spp);

			nr -= spp;
			++last_n_periods;
		}

		if (xrun && (_pcmi.capt_xrun() > 0 || _pcmi.play_xrun() > 0)) {
			reset_dll = true;
			_samples_since_dll_reset = 0;
			g_atomic_int_set (&_draining, 1);
		}
	}

	_pcmi.pcm_stop ();
	_active = false;

	if (_run) {
		Halted (); /* Emit Signal */
	}
	return 0;
}

void
AlsaAudioSlave::cycle_start (double tme, double mst_speed, bool drain)
{
	//printf ("SRC %f / %f = %f\n", mst_speed, _slave_speed, mst_speed / _slave_speed);
	//printf ("DRIFT (mst) %11.1f - (slv) %11.1f = %.1f us = %.1f spl\n", tme, _t0, tme - _t0, (tme - _t0) * _pcmi.fsamp () * 1e-6);
	//printf ("Slave capt: %u play: %u\n", _rb_capture.read_space (), _rb_playback.read_space ());

	// TODO LPF filter ratios, atomic _slave_speed
	const double slave_speed = _slave_speed;

	_src_capt.set_rratio (mst_speed / slave_speed);
	_src_play.set_rratio (slave_speed / mst_speed);

	if (_capt_buff) {
		memset (_capt_buff, 0, sizeof(float) * _pcmi.ncapt () * _samples_per_period);
	}

	if (drain) {
		g_atomic_int_set (&_draining, 1);
		return;
	}

	if (g_atomic_int_get (&_draining)) {
		_rb_capture.increment_read_idx (_rb_capture.read_space());
		return;
	}

	/* resample slave capture data from ringbuffer */
	unsigned int nchn = _pcmi.ncapt ();
	_src_capt.out_count = _samples_per_period;
	_src_capt.out_data  = _capt_buff;

	/* estimate required samples */
	const double rratio = _ratio * mst_speed / slave_speed;
	if (_rb_capture.read_space() < ceil (nchn * _samples_per_period / rratio)) {
#ifndef NDEBUG
		printf ("--- UNDERFLOW ---  have %u  want %.1f\n", _rb_capture.read_space(), ceil (nchn * _samples_per_period / rratio)); // XXX DEBUG
#endif
		_capt_latency += _samples_per_period;
		update_latencies (_play_latency, _capt_latency);
		return;
	}

	bool underflow = false;
	while (_src_capt.out_count && _active && nchn > 0) {
		if (_rb_capture.read_space() < nchn) {
			underflow = true;
			break;
		}
		unsigned int n;
		PBD::RingBuffer<float>::rw_vector vec;
		_rb_capture.get_read_vector (&vec);
		if (vec.len[0] < nchn) {
			_rb_capture.read (_src_buff, nchn);
			_src_capt.inp_count = 1;
			_src_capt.inp_data  = _src_buff;
			_src_capt.process ();
		} else {
			_src_capt.inp_count = n = vec.len[0] / nchn;
			_src_capt.inp_data  = vec.buf[0];
			_src_capt.process ();
			n -= _src_capt.inp_count;
			_rb_capture.increment_read_idx (n * _pcmi.ncapt ());
		}
	}

	if (underflow) {
#ifndef NDEBUG
		std::cerr << "ALSA Slave: Capture Ringbuffer Underflow\n"; // XXX DEBUG
#endif
		g_atomic_int_set(&_draining, 1);
	}

	if ((!_active || underflow) && _capt_buff) {
		memset (_capt_buff, 0, sizeof(float) * _pcmi.ncapt () * _samples_per_period);
	}

	if (_play_buff) {
		memset (_play_buff, 0, sizeof(float) * _pcmi.nplay () * _samples_per_period);
	}
}

void
AlsaAudioSlave::cycle_end ()
{
	bool drain_done = false;
	bool overflow = false;

	if (g_atomic_int_get (&_draining)) {
		if (_rb_capture.read_space() == 0 && _rb_playback.read_space() == 0 && _samples_since_dll_reset > _pcmi.fsamp ()) {
			reset_resampler (_src_capt);
			reset_resampler (_src_play);
			memset (_src_buff, 0, sizeof (float) * _pcmi.nplay());
			/* prefill ringbuffers, resampler variance */
			for (int i = 0; i < 16; ++i) {
				_rb_playback.write (_src_buff, _pcmi.nplay());
			}
			memset (_src_buff, 0, sizeof (float) * _pcmi.ncapt());
			// It's safe to write here, process-thread NO-OPs while draining.
			for (int i = 0; i < 16; ++i) {
				_rb_capture.write (_src_buff, _pcmi.ncapt());
			}
			_capt_latency = 16;
			_play_latency = 16 + _ratio * _pcmi.fsize () * (_pcmi.play_nfrag () - 1);
			update_latencies (_play_latency, _capt_latency);
			drain_done = true;
		} else {
			return;
		}
	}

	/* resample collected playback data into ringbuffer */
	unsigned int nchn = _pcmi.nplay ();
	_src_play.inp_count = _samples_per_period;
	_src_play.inp_data  = _play_buff;

	while (_src_play.inp_count && _active && nchn > 0) {
		unsigned int n;
		PBD::RingBuffer<float>::rw_vector vec;
		_rb_playback.get_write_vector (&vec);
		if (vec.len[0] < nchn) {
			_src_play.out_count = 1;
			_src_play.out_data  = _src_buff;
			_src_play.process ();
			if (_rb_playback.write_space() < nchn) {
				overflow = true;
				break;
			} else if (_src_play.out_count == 0) {
				_rb_playback.write (_src_buff, nchn);
			}
		} else {
			_src_play.out_count = n = vec.len[0] / nchn;
			_src_play.out_data  = vec.buf[0];
			_src_play.process ();
			n -= _src_play.out_count;
			if (_rb_playback.write_space() < n * nchn) {
				overflow = true;
				break;
			}
			_rb_playback.increment_write_idx (n * nchn);
		}
	}

	if (overflow) {
#ifndef NDEBUG
		std::cerr << "ALSA Slave: Playback Ringbuffer Overflow\n"; // XXX DEBUG
#endif
		g_atomic_int_set (&_draining, 1);
		return;
	}
	if (drain_done) {
		g_atomic_int_set (&_draining, 0);
	}
}

void
AlsaAudioSlave::freewheel (bool onoff)
{
	if (onoff) {
		g_atomic_int_set (&_draining, 1);
	}
}

/* master read slave's capture.
 * resampled at cycle_start, before master can call this
 */
uint32_t
AlsaAudioSlave::capt_chan (uint32_t chn, float* dst, uint32_t n_samples)
{
	uint32_t nchn = _pcmi.ncapt ();
	assert (chn < nchn && n_samples == _samples_per_period);
	float* src = &_capt_buff[chn];
	for (uint32_t s = 0; s < n_samples; ++s) {
		dst[s] = src[s * nchn];
	}
	return n_samples;
}

/* write from master to slave output,
 * resampled at cycle_end, after master called this.
 */
uint32_t
AlsaAudioSlave::play_chan (uint32_t chn, float* src, uint32_t n_samples)
{
	uint32_t nchn = _pcmi.nplay ();
	assert (chn < nchn && n_samples == _samples_per_period);
	float* dst = &_play_buff[chn];
	for (uint32_t s = 0; s < n_samples; ++s) {
		dst[s * nchn] = src[s];
	}
	return n_samples;
}
