/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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
 */

#ifndef AUDIOGRAPHER_ANALYSER_H
#define AUDIOGRAPHER_ANALYSER_H

#include <fftw3.h>
#include <vamp-hostsdk/PluginLoader.h>
#include <vamp-sdk/Plugin.h>

#include "audiographer/visibility.h"
#include "audiographer/sink.h"
#include "audiographer/routines.h"
#include "audiographer/utils/listed_source.h"

#include "pbd/fastlog.h"
#include "ardour/export_analysis.h"

namespace AudioGrapher
{

class /*LIBAUDIOGRAPHER_API*/ Analyser : public ListedSource<float>, public Sink<float>
{
  public:
	Analyser (float sample_rate, unsigned int channels, framecnt_t bufsize, framecnt_t n_samples)
	: _ebur128_plugin (0)
	, _sample_rate (sample_rate)
	, _channels (channels)
	, _bufsize (bufsize / channels)
	, _n_samples (n_samples)
	, _pos (0)
	{
		assert (bufsize % channels == 0);
		//printf("NEW ANALYSER %p r:%.1f c:%d f:%ld l%ld\n", this, sample_rate, channels, bufsize, n_samples);
		if (channels > 0 && channels <= 2) {
			using namespace Vamp::HostExt;
			PluginLoader* loader (PluginLoader::getInstance());
			_ebur128_plugin = loader->loadPlugin ("libardourvampplugins:ebur128", sample_rate, PluginLoader::ADAPT_ALL_SAFE);
			assert (_ebur128_plugin);
			_ebur128_plugin->reset ();
			_ebur128_plugin->initialise (channels, _bufsize, _bufsize);
		}
		_bufs[0] = (float*) malloc (sizeof(float) * _bufsize);
		_bufs[1] = (float*) malloc (sizeof(float) * _bufsize);
		const size_t peaks = sizeof(_result._peaks) / sizeof (ARDOUR::PeakData::PeakDatum) / 2;
		_spp = ceil ((_n_samples + 1.f) / (float) peaks);

		_fft_data_size   = _bufsize / 2;
		_fft_freq_per_bin = sample_rate / _fft_data_size / 2.f;

		_fft_data_in  = (float *) fftwf_malloc (sizeof(float) * _bufsize);
		_fft_data_out = (float *) fftwf_malloc (sizeof(float) * _bufsize);
		_fft_power    = (float *) malloc (sizeof(float) * _fft_data_size);

		for (uint32_t i = 0; i < _fft_data_size; ++i) {
			_fft_power[i] = 0;
		}
		for (uint32_t i = 0; i < _bufsize; ++i) {
			_fft_data_out[i] = 0;
		}

		_fft_plan = fftwf_plan_r2r_1d (_bufsize, _fft_data_in, _fft_data_out, FFTW_R2HC, FFTW_MEASURE);

		_hann_window = (float *) malloc(sizeof(float) * _bufsize);
		double sum = 0.0;

		for (uint32_t i = 0; i < _bufsize; ++i) {
			_hann_window[i] = 0.5f - (0.5f * (float) cos (2.0f * M_PI * (float)i / (float)(_bufsize)));
			sum += _hann_window[i];
		}
		const double isum = 2.0 / sum;
		for (uint32_t i = 0; i < _bufsize; ++i) {
			_hann_window[i] *= isum;
		}
	}

	~Analyser ()
	{
		delete _ebur128_plugin;
		free (_bufs[0]);
		free (_bufs[1]);
		fftwf_destroy_plan (_fft_plan);
		fftwf_free (_fft_data_in);
		fftwf_free (_fft_data_out);
		free (_fft_power);
		free (_hann_window);
	}

	void process (ProcessContext<float> const & c)
	{
		framecnt_t n_samples = c.frames() / c.channels();
		assert (c.frames() % c.channels() == 0);
		assert (n_samples <= _bufsize);
		//printf("PROC %p @%ld F: %ld, S: %ld C:%d\n", this, _pos, c.frames(), n_samples, c.channels());
		float const * d = c.data ();
		framecnt_t s;
		for (s = 0; s < n_samples; ++s) {
			_fft_data_in[s] = 0;
			const framecnt_t pk = (_pos + s) / _spp;
			for (unsigned int c = 0; c < _channels; ++c) {
				const float v = *d;
				_bufs[c][s] = v;
				if (_result._peaks[pk].min > v) { _result._peaks[pk].min = *d; }
				if (_result._peaks[pk].max < v) { _result._peaks[pk].max = *d; }
				_fft_data_in[s] += v * _hann_window[s] / (float) _channels;
				++d;
			}
		}
		for (; s < _bufsize; ++s) {
			for (unsigned int c = 0; c < _channels; ++c) {
				_bufs[c][s] = 0.f;
				_fft_data_in[s] = 0;
			}
		}
		if (_ebur128_plugin) {
			_ebur128_plugin->process (_bufs, Vamp::RealTime::fromSeconds ((double) _pos / _sample_rate));
		}
		fftwf_execute (_fft_plan);

		_fft_power[0] = _fft_data_out[0] * _fft_data_out[0];

#define FRe (_fft_data_out[i])
#define FIm (_fft_data_out[_bufsize - i])
		for (uint32_t i = 1; i < _fft_data_size - 1; ++i) {
			_fft_power[i] = (FRe * FRe) + (FIm * FIm);
		}
#undef FRe
#undef FIm

		// TODO handle case where  _pos / _spp != (_pos + _bufsize) / _spp
		// TODO: get geometry from  ExportAnalysis
		const framecnt_t x = _pos / _spp;
		const float range = 80; // dB
		const double ypb = 256.0 / _fft_data_size;

		for (uint32_t i = 1; i < _fft_data_size - 1; ++i) {
			const float level = fft_power_at_bin (i, i);
			if (level < -range) continue;
			const float pk = level > 0.0 ? 1.0 : (range + level) / range;
			const uint32_t y = 256 - ceil (i * ypb); // log-y?
			assert (x >= 0 && x < 800);
			assert (y < 256);
			if (_result._spectrum[x][y] < pk) { _result._spectrum[x][y] = pk; }
		}

		_pos += n_samples;

		/* pass audio audio through */
		ListedSource<float>::output(c);
	}

	ARDOUR::ExportAnalysisPtr result () {
		//printf("PROCESSED %ld / %ld samples\n", _pos, _n_samples);
		if (_pos == 0) {
			return ARDOUR::ExportAnalysisPtr ();
		}
		if (_ebur128_plugin) {
			Vamp::Plugin::FeatureSet features = _ebur128_plugin->getRemainingFeatures ();
			if (!features.empty() && features.size() == 2) {
				_result.loudness = features[0][0].values[0];
				_result.loudness_range = features[1][0].values[0];
				_result.have_loudness = true;
			}
		}
		return ARDOUR::ExportAnalysisPtr (new ARDOUR::ExportAnalysis (_result));
	}

	using Sink<float>::process;

  private:
	ARDOUR::ExportAnalysis _result;
	Vamp::Plugin* _ebur128_plugin;

	float        _sample_rate;
	unsigned int _channels;
	framecnt_t   _bufsize;
	framecnt_t   _n_samples;
	framecnt_t   _pos;
	framecnt_t   _spp;

	float* _bufs[2];

	float*     _hann_window;
	uint32_t   _fft_data_size;
	double     _fft_freq_per_bin;
	float*     _fft_data_in;
	float*     _fft_data_out;
	float*     _fft_power;
	fftwf_plan _fft_plan;

	inline float fft_power_at_bin (const uint32_t b, const float norm) const {
		const float a = _fft_power[b] * norm;
		return a > 1e-12 ? 10.0 * fast_log10(a) : -INFINITY;
	}

};

} // namespace

#endif
