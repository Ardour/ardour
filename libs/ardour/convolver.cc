/*
 * Copyright (C) 2018-2021 Robin Gareus <robin@gareus.org>
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

#include <assert.h>

#include "pbd/error.h"
#include "pbd/pthread_utils.h"

#include "ardour/audio_buffer.h"
#include "ardour/audioengine.h"
#include "ardour/audiofilesource.h"
#include "ardour/chan_mapping.h"
#include "ardour/convolver.h"
#include "ardour/dsp_filter.h"
#include "ardour/readable.h"
#include "ardour/session.h"
#include "ardour/source_factory.h"
#include "ardour/srcfilesource.h"
#include "ardour/types.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ARDOUR::DSP;
using namespace ArdourZita;

Convolution::Convolution (Session& session, uint32_t n_in, uint32_t n_out)
    : SessionHandleRef (session)
    , _n_samples (0)
    , _max_size (0)
    , _offset (0)
    , _configured (false)
    , _threaded (false)
    , _n_inputs (n_in)
    , _n_outputs (n_out)
{
	AudioEngine::instance ()->BufferSizeChanged.connect_same_thread (*this, boost::bind (&Convolution::restart, this));
}

bool
Convolution::add_impdata (
    uint32_t                    c_in,
    uint32_t                    c_out,
    boost::shared_ptr<AudioReadable> readable,
    float                       gain,
    uint32_t                    pre_delay,
    sampleoffset_t              offset,
    samplecnt_t                 length,
    uint32_t                    channel)
{
	if (_configured || c_in >= _n_inputs || c_out >= _n_outputs) {
		return false;
	}
	if (!readable || readable->readable_length_samples () <= offset || readable->n_channels () <= channel) {
		return false;
	}

	_impdata.push_back (ImpData (c_in, c_out, readable, gain, pre_delay, offset, length));
	return true;
}

bool
Convolution::ready () const
{
	return _configured && _convproc.state () == Convproc::ST_PROC;
}

void
Convolution::restart ()
{
	_convproc.stop_process ();
	_convproc.cleanup ();
	_convproc.set_options (0);

	uint32_t n_part;

	if (_threaded) {
		_n_samples = 64;
		n_part     = Convproc::MAXPART;
	} else {
		_n_samples = _session.get_block_size ();
		uint32_t power_of_two;
		for (power_of_two = 1; 1U << power_of_two < _n_samples; ++power_of_two) ;
		_n_samples = 1 << power_of_two;
		n_part     = std::min ((uint32_t)Convproc::MAXPART, _n_samples);
	}

	_offset    = 0;
	_max_size  = 0;

	for (std::vector<ImpData>::const_iterator i = _impdata.begin (); i != _impdata.end (); ++i) {
		_max_size = std::max (_max_size, (uint32_t)i->readable_length_samples ());
	}

	int rv = _convproc.configure (
	    /*in*/ _n_inputs,
	    /*out*/ _n_outputs,
	    /*max-convolution length */ _max_size,
	    /*quantum, nominal-buffersize*/ _n_samples,
	    /*Convproc::MINPART*/ _n_samples,
	    /*Convproc::MAXPART*/ n_part,
	    /*density 0 = auto, i/o dependent */ 0);

	for (std::vector<ImpData>::const_iterator i = _impdata.begin (); i != _impdata.end (); ++i) {
		uint32_t pos = 0;

		const float    ir_gain  = i->gain;
		const uint32_t ir_delay = i->delay;
		const uint32_t ir_len   = i->readable_length_samples ();

		while (true) {
			float ir[8192];

			samplecnt_t to_read = std::min ((uint32_t)8192, ir_len - pos);
			samplecnt_t ns      = i->read (ir, pos, to_read);

			if (ns == 0) {
				break;
			}

			if (ir_gain != 1.f) {
				for (samplecnt_t i = 0; i < ns; ++i) {
					ir[i] *= ir_gain;
				}
			}

			rv = _convproc.impdata_create (
			    /*i/o map */ i->c_in, i->c_out,
			    /*stride, de-interleave */ 1,
			    ir,
			    ir_delay + pos, ir_delay + pos + ns);

			if (rv != 0) {
				break;
			}

			pos += ns;

			if (pos == _max_size) {
				break;
			}
		}
	}

	if (rv == 0) {
		rv = _convproc.start_process (pbd_absolute_rt_priority (PBD_SCHED_FIFO, AudioEngine::instance ()->client_real_time_priority () - 1), PBD_SCHED_FIFO);
	}

	assert (rv == 0); // bail out in debug builds

	if (rv != 0) {
		_convproc.stop_process ();
		_convproc.cleanup ();
		_configured = false;
		return;
	}

	_configured = true;

#ifndef NDEBUG
	_convproc.print (stdout);
#endif
}

void
Convolution::run (BufferSet& bufs, ChanMapping const& in_map, ChanMapping const& out_map, pframes_t n_samples, samplecnt_t offset)
{
	if (!ready ()) {
		process_map (&bufs, ChanCount (DataType::AUDIO, _n_outputs), in_map, out_map, n_samples, offset);
		return;
	}

	uint32_t done   = 0;
	uint32_t remain = n_samples;

	while (remain > 0) {
		uint32_t ns = std::min (remain, _n_samples - _offset);

		for (uint32_t c = 0; c < _n_inputs; ++c) {
			bool valid;
			const uint32_t idx = in_map.get (DataType::AUDIO, c, &valid);
			if (!valid) {
				::memset (&_convproc.inpdata (c)[_offset], 0, sizeof (float) * ns);
			} else {
				AudioBuffer const& ab (bufs.get_audio (idx));
				memcpy (&_convproc.inpdata (c)[_offset], ab.data (done + offset), sizeof (float) * ns);
			}
		}

		for (uint32_t c = 0; c < _n_outputs; ++c) {
			bool valid;
			const uint32_t idx = out_map.get (DataType::AUDIO, c, &valid);
			if (valid) {
				AudioBuffer& ab (bufs.get_audio (idx));
				memcpy (ab.data (done + offset), &_convproc.outdata (c)[_offset], sizeof (float) * ns);
			}
		}

		_offset += ns;
		done    += ns;
		remain  -= ns;

		if (_offset == _n_samples) {
			_convproc.process ();
			_offset = 0;
		}
	}
}

/* ****************************************************************************/

Convolver::Convolver (
    Session&           session,
    std::string const& path,
    IRChannelConfig    irc,
    IRSettings         irs)
    : Convolution (session, ircc_in (irc), ircc_out (irc))
    , _irc (irc)
    , _ir_settings (irs)
{
	_threaded = true;

	std::vector<boost::shared_ptr<AudioReadable> > readables = AudioReadable::load (_session, path);

	if (readables.empty ()) {
		PBD::error << string_compose (_("Convolver: IR \"%1\" no usable audio-channels sound."), path) << endmsg;
		throw failed_constructor ();
	}

	if (readables[0]->readable_length_samples () > 0x1000000 /*2^24*/) {
		PBD::error << string_compose (_("Convolver: IR \"%1\" file too long."), path) << endmsg;
		throw failed_constructor ();
	}

	/* map channels
	 * - Mono:
	 *    always use first only
	 * - MonoToStereo:
	 *    mono-file: use 1st for M -> L, M -> R
	 *    else: use first two channels
	 * - Stereo
	 *    mono-file: use 1st for both L -> L, R -> R, no x-over
	 *    stereo-file: L -> L, R -> R  -- no L/R, R/L x-over
	 *    3chan-file: ignore 3rd channel, use as stereo-file.
	 *    4chan file:  L -> L, L -> R, R -> R, R -> L
	 */

	uint32_t n_imp = n_inputs () * n_outputs ();
	uint32_t n_chn = readables.size ();

	if (_irc == Stereo && n_chn == 3) {
		/* ignore 3rd channel */
		n_chn = 2;
	}
	if (_irc == Stereo && n_chn <= 2) {
		/* ignore x-over */
		n_imp = 2;
	}

#ifndef NDEBUG
	printf ("Convolver: Nin=%d Nout=%d Nimp=%d Nchn=%d\n", n_inputs (), n_outputs (), n_imp, n_chn);
#endif

	assert (n_imp <= 4);

	for (uint32_t c = 0; c < n_imp; ++c) {
		int ir_c = c % n_chn;
		int io_o = c % n_outputs ();
		int io_i;

		if (n_imp == 2 && _irc == Stereo) {
			/*           (imp, in, out)
			 * Stereo       (2, 2, 2)    1: L -> L, 2: R -> R
			 */
			io_i = c % n_inputs ();
		} else {
			/*           (imp, in, out)
			 * Mono         (1, 1, 1)   1: M -> M
			 * MonoToStereo (2, 1, 2)   1: M -> L, 2: M -> R
			 * Stereo       (4, 2, 2)   1: L -> L, 2: L -> R, 3: R -> L, 4: R -> R
			 */
			io_i = (c / n_outputs ()) % n_inputs ();
		}

		boost::shared_ptr<AudioReadable> r = readables[ir_c];
		assert (r->n_channels () == 1);

		const float    chan_gain  = _ir_settings.gain * _ir_settings.channel_gain[c];
		const uint32_t chan_delay = _ir_settings.pre_delay + _ir_settings.channel_delay[c];

#ifndef NDEBUG
		printf ("Convolver map: IR-chn %d: in %d -> out %d (gain: %.1fdB delay; %d)\n", ir_c + 1, io_i + 1, io_o + 1, 20.f * log10f (chan_gain), chan_delay);
#endif

		add_impdata (io_i, io_o, r, chan_gain, chan_delay);
	}

	Convolution::restart ();
}

void
Convolver::run_mono_buffered (float* buf, uint32_t n_samples)
{
	assert (_convproc.state () == Convproc::ST_PROC);
	assert (_irc == Mono);

	uint32_t done   = 0;
	uint32_t remain = n_samples;

	while (remain > 0) {
		uint32_t ns = std::min (remain, _n_samples - _offset);

		float* const       in  = _convproc.inpdata (/*channel*/ 0);
		float const* const out = _convproc.outdata (/*channel*/ 0);

		memcpy (&in[_offset], &buf[done], sizeof (float) * ns);
		memcpy (&buf[done], &out[_offset], sizeof (float) * ns);

		_offset += ns;
		done    += ns;
		remain  -= ns;

		if (_offset == _n_samples) {
			_convproc.process ();
			_offset = 0;
		}
	}
}

void
Convolver::run_stereo_buffered (float* left, float* right, uint32_t n_samples)
{
	assert (_convproc.state () == Convproc::ST_PROC);
	assert (_irc != Mono);

	uint32_t done   = 0;
	uint32_t remain = n_samples;

	while (remain > 0) {
		uint32_t ns = std::min (remain, _n_samples - _offset);

		memcpy (&_convproc.inpdata (0)[_offset], &left[done], sizeof (float) * ns);
		if (_irc >= Stereo) {
			memcpy (&_convproc.inpdata (1)[_offset], &right[done], sizeof (float) * ns);
		}
		memcpy (&left[done],  &_convproc.outdata (0)[_offset], sizeof (float) * ns);
		memcpy (&right[done], &_convproc.outdata (1)[_offset], sizeof (float) * ns);

		_offset += ns;
		done    += ns;
		remain  -= ns;

		if (_offset == _n_samples) {
			_convproc.process ();
			_offset = 0;
		}
	}
}

void
Convolver::run_mono_no_latency (float* buf, uint32_t n_samples)
{
	assert (_convproc.state () == Convproc::ST_PROC);
	assert (_irc == Mono);

	uint32_t done   = 0;
	uint32_t remain = n_samples;

	while (remain > 0) {
		uint32_t ns = std::min (remain, _n_samples - _offset);

		float* const in  = _convproc.inpdata (/*channel*/ 0);
		float* const out = _convproc.outdata (/*channel*/ 0);

		memcpy (&in[_offset], &buf[done], sizeof (float) * ns);

		if (_offset + ns == _n_samples) {
			_convproc.process ();
			memcpy (&buf[done], &out[_offset], sizeof (float) * ns);
			_offset = 0;
		} else {
			assert (remain == ns);
			_convproc.tailonly (_offset + ns);
			memcpy (&buf[done], &out[_offset], sizeof (float) * ns);
			_offset += ns;
		}
		done   += ns;
		remain -= ns;
	}
}

void
Convolver::run_stereo_no_latency (float* left, float* right, uint32_t n_samples)
{
	assert (_convproc.state () == Convproc::ST_PROC);
	assert (_irc != Mono);

	uint32_t done   = 0;
	uint32_t remain = n_samples;

	float* const outL = _convproc.outdata (0);
	float* const outR = _convproc.outdata (1);

	while (remain > 0) {
		uint32_t ns = std::min (remain, _n_samples - _offset);

		memcpy (&_convproc.inpdata (0)[_offset], &left[done], sizeof (float) * ns);
		if (_irc >= Stereo) {
			memcpy (&_convproc.inpdata (1)[_offset], &right[done], sizeof (float) * ns);
		}

		if (_offset + ns == _n_samples) {
			_convproc.process ();
			memcpy (&left[done],  &outL[_offset], sizeof (float) * ns);
			memcpy (&right[done], &outR[_offset], sizeof (float) * ns);
			_offset = 0;
		} else {
			assert (remain == ns);
			_convproc.tailonly (_offset + ns);
			memcpy (&left[done],  &outL[_offset], sizeof (float) * ns);
			memcpy (&right[done], &outR[_offset], sizeof (float) * ns);
			_offset += ns;
		}
		done   += ns;
		remain -= ns;
	}
}
