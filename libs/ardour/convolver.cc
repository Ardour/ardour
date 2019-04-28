/*
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <assert.h>

#include "pbd/error.h"
#include "pbd/pthread_utils.h"

#include "ardour/audioengine.h"
#include "ardour/audiofilesource.h"
#include "ardour/convolver.h"
#include "ardour/session.h"
#include "ardour/srcfilesource.h"
#include "ardour/source_factory.h"

#include "pbd/i18n.h"

using namespace ARDOUR::DSP;
using namespace ArdourZita;

using ARDOUR::Session;

Convolver::Convolver (
		Session& session,
		std::string const& path,
		IRChannelConfig irc,
		IRSettings irs)
	: SessionHandleRef (session)
	, _irc (irc)
	, _ir_settings (irs)
	, _n_samples (0)
	, _max_size (0)
	, _offset (0)
	, _configured (false)
{
	ARDOUR::SoundFileInfo sf_info;
	std::string error_msg;

	if (!AudioFileSource::get_soundfile_info (path, sf_info, error_msg)) {
		PBD::error << string_compose(_("Convolver: cannot open IR \"%1\": %2"), path, error_msg) << endmsg;
		throw failed_constructor ();
	}

	if (sf_info.length > 0x1000000 /*2^24*/) {
		PBD::error << string_compose(_("Convolver: IR \"%1\" file too long."), path) << endmsg;
		throw failed_constructor ();
	}

	for (unsigned int n = 0; n < sf_info.channels; ++n) {
		try {
			boost::shared_ptr<AudioFileSource> afs;
			afs = boost::dynamic_pointer_cast<AudioFileSource> (
					SourceFactory::createExternal (DataType::AUDIO, _session,
						path, n,
						Source::Flag (ARDOUR::AudioFileSource::NoPeakFile), false));

			if (afs->sample_rate() != _session.nominal_sample_rate()) {
				boost::shared_ptr<SrcFileSource> sfs (new SrcFileSource(_session, afs, ARDOUR::SrcBest));
				_readables.push_back(sfs);
			} else {
				_readables.push_back (afs);
			}
		} catch (failed_constructor& err) {
			PBD::error << string_compose(_("Convolver: Could not open IR \"%1\"."), path) << endmsg;
			throw failed_constructor ();
		}
	}

	if (_readables.empty ()) {
		PBD::error << string_compose (_("Convolver: IR \"%1\" no usable audio-channels sound."), path) << endmsg;
		throw failed_constructor ();
	}

	AudioEngine::instance ()->BufferSizeChanged.connect_same_thread (*this, boost::bind (&Convolver::reconfigure, this));

	reconfigure ();
}

void
Convolver::reconfigure ()
{
	_convproc.stop_process ();
	_convproc.cleanup ();
	_convproc.set_options (0);

	assert (!_readables.empty ());

	_offset    = 0;
	_n_samples = _session.get_block_size ();
	_max_size  = _readables[0]->readable_length ();

	uint32_t power_of_two;
	for (power_of_two = 1; 1U << power_of_two < _n_samples; ++power_of_two) ;
	_n_samples = 1 << power_of_two;

	int n_part = std::min ((uint32_t)Convproc::MAXPART, 4 * _n_samples);
	int rv = _convproc.configure (
			/*in*/  n_inputs (),
			/*out*/ n_outputs (),
			/*max-convolution length */ _max_size,
			/*quantum, nominal-buffersize*/ _n_samples,
			/*Convproc::MINPART*/ _n_samples,
			/*Convproc::MAXPART*/ n_part,
			/*density*/ 0);

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
	uint32_t n_chn = _readables.size ();

	if (_irc == Stereo && n_chn == 3) {
		/* ignore 3rd channel */
		n_chn = 2;
	}
	if (_irc == Stereo && n_chn <= 2) {
		/* ignore x-over */
		n_imp = 2;
	}

#ifndef NDEBUG
	printf ("Convolver::reconfigure Nin=%d Nout=%d Nimp=%d Nchn=%d\n", n_inputs (), n_outputs (), n_imp, n_chn);
#endif

	assert (n_imp <= 4);

	for (uint32_t c = 0; c < n_imp && rv == 0; ++c) {
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


		boost::shared_ptr<Readable> r = _readables[ir_c];
		assert (r->readable_length () == _max_size);
		assert (r->n_channels () == 1);

		const float    chan_gain  = _ir_settings.gain * _ir_settings.channel_gain[c];
		const uint32_t chan_delay = _ir_settings.pre_delay + _ir_settings.channel_delay[c];

#ifndef NDEBUG
		printf ("Convolver map: IR-chn %d: in %d -> out %d (gain: %.1fdB delay; %d)\n", ir_c + 1, io_i + 1, io_o + 1, 20.f * log10f (chan_gain), chan_delay);
#endif

		uint32_t pos = 0;
		while (true) {
			float ir[8192];

			samplecnt_t to_read = std::min ((uint32_t)8192, _max_size - pos);
			samplecnt_t ns      = r->read (ir, pos, to_read, 0);

			if (ns == 0) {
				assert (pos == _max_size);
				break;
			}

			if (chan_gain != 1.f) {
				for (samplecnt_t i = 0; i < ns; ++i) {
					ir[i] *= chan_gain;
				}
			}

			rv = _convproc.impdata_create (
					/*i/o map */ io_i, io_o,
					/*stride, de-interleave */ 1,
					ir,
					chan_delay + pos, chan_delay + pos + ns);

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
		rv = _convproc.start_process (pbd_absolute_rt_priority (PBD_SCHED_FIFO, AudioEngine::instance()->client_real_time_priority() - 2), PBD_SCHED_FIFO);
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

bool
Convolver::ready () const
{
	return _configured && _convproc.state () == Convproc::ST_PROC;
}

void
Convolver::run (float* buf, uint32_t n_samples)
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
			_convproc.process (/*sync, freewheeling*/ true);
			_offset = 0;
		}
	}
}

void
Convolver::run_stereo (float* left, float* right, uint32_t n_samples)
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
			_convproc.process (true);
			_offset = 0;
		}
	}
}
