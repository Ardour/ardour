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

#include "audiographer/general/analyser.h"
#include "pbd/fastlog.h"

using namespace AudioGrapher;

Analyser::Analyser (float sample_rate, unsigned int channels, framecnt_t bufsize, framecnt_t n_samples)
	: _ebur128_plugin (0)
	, _dbtp_plugin (0)
	, _sample_rate (sample_rate)
	, _channels (channels)
	, _bufsize (bufsize / channels)
	, _n_samples (n_samples)
	, _pos (0)
{
	assert (bufsize % channels == 0);
	//printf ("NEW ANALYSER %p r:%.1f c:%d f:%ld l%ld\n", this, sample_rate, channels, bufsize, n_samples);
	if (channels > 0 && channels <= 2) {
		using namespace Vamp::HostExt;
		PluginLoader* loader (PluginLoader::getInstance ());
		_ebur128_plugin = loader->loadPlugin ("libardourvampplugins:ebur128", sample_rate, PluginLoader::ADAPT_ALL_SAFE);
		assert (_ebur128_plugin);
		_ebur128_plugin->reset ();
		if (!_ebur128_plugin->initialise (channels, _bufsize, _bufsize)) {
			printf ("FAILED TO INITIALIZE EBUR128\n");
			delete _ebur128_plugin;
			_ebur128_plugin = 0;
		}
	}

	_dbtp_plugin = (Vamp::Plugin**) malloc (sizeof(Vamp::Plugin*) * channels);
	for (unsigned int c = 0; c < _channels; ++c) {
		using namespace Vamp::HostExt;
		PluginLoader* loader (PluginLoader::getInstance ());
		_dbtp_plugin[c] = loader->loadPlugin ("libardourvampplugins:dBTP", sample_rate, PluginLoader::ADAPT_ALL_SAFE);
		assert (_dbtp_plugin[c]);
		_dbtp_plugin[c]->reset ();
		if (!_dbtp_plugin[c]->initialise (1, _bufsize, _bufsize)) {
			printf ("FAILED TO INITIALIZE DBTP %d\n", c);
			delete _dbtp_plugin[c];
			_dbtp_plugin[c] = 0;
		}
	}

	_bufs[0] = (float*) malloc (sizeof (float) * _bufsize);
	_bufs[1] = (float*) malloc (sizeof (float) * _bufsize);

	const size_t peaks = sizeof (_result.peaks) / sizeof (ARDOUR::PeakData::PeakDatum) / 4;
	_spp = ceil ((_n_samples + 1.f) / (float) peaks);

	const size_t swh = sizeof (_result.spectrum) / sizeof (float);
	const size_t height = sizeof (_result.spectrum[0]) / sizeof (float);
	const size_t width = swh / height;
	_fpp = ceil ((_n_samples + 1.f) / (float) width);

	_fft_data_size   = _bufsize / 2;
	_fft_freq_per_bin = sample_rate / _fft_data_size / 2.f;

	_fft_data_in  = (float *) fftwf_malloc (sizeof (float) * _bufsize);
	_fft_data_out = (float *) fftwf_malloc (sizeof (float) * _bufsize);
	_fft_power    = (float *) malloc (sizeof (float) * _fft_data_size);

	for (uint32_t i = 0; i < _fft_data_size; ++i) {
		_fft_power[i] = 0;
	}
	for (uint32_t i = 0; i < _bufsize; ++i) {
		_fft_data_out[i] = 0;
	}

	const float nyquist = (sample_rate * .5);
#if 0 // linear
#define YPOS(FREQ) ceil (height * (1.0 - FREQ / nyquist))
#else
#define YPOS(FREQ) ceil (height * (1 - logf (1.f + .1f * _fft_data_size * FREQ / nyquist) / logf (1.f + .1f * _fft_data_size)))
#endif

	_result.freq[0] = YPOS (50);
	_result.freq[1] = YPOS (100);
	_result.freq[2] = YPOS (500);
	_result.freq[3] = YPOS (1000);
	_result.freq[4] = YPOS (5000);
	_result.freq[5] = YPOS (10000);

	_fft_plan = fftwf_plan_r2r_1d (_bufsize, _fft_data_in, _fft_data_out, FFTW_R2HC, FFTW_MEASURE);

	_hann_window = (float *) malloc (sizeof (float) * _bufsize);
	double sum = 0.0;

	for (uint32_t i = 0; i < _bufsize; ++i) {
		_hann_window[i] = 0.5f - (0.5f * (float) cos (2.0f * M_PI * (float)i / (float)(_bufsize)));
		sum += _hann_window[i];
	}
	const double isum = 2.0 / sum;
	for (uint32_t i = 0; i < _bufsize; ++i) {
		_hann_window[i] *= isum;
	}

	if (channels == 2) {
		_result.n_channels = 2;
	} else {
		_result.n_channels = 1;
	}
}

Analyser::~Analyser ()
{
	delete _ebur128_plugin;
	for (unsigned int c = 0; c < _channels; ++c) {
		delete _dbtp_plugin[c];
	}
	free (_dbtp_plugin);
	free (_bufs[0]);
	free (_bufs[1]);
	fftwf_destroy_plan (_fft_plan);
	fftwf_free (_fft_data_in);
	fftwf_free (_fft_data_out);
	free (_fft_power);
	free (_hann_window);
}

void
Analyser::process (ProcessContext<float> const & c)
{
	framecnt_t n_samples = c.frames () / c.channels ();
	assert (c.frames () % c.channels () == 0);
	assert (n_samples <= _bufsize);
	//printf ("PROC %p @%ld F: %ld, S: %ld C:%d\n", this, _pos, c.frames (), n_samples, c.channels ());
	float const * d = c.data ();
	framecnt_t s;
	const unsigned cmask = _result.n_channels - 1; // [0, 1]
	for (s = 0; s < n_samples; ++s) {
		_fft_data_in[s] = 0;
		const framecnt_t pk = (_pos + s) / _spp;
		for (unsigned int c = 0; c < _channels; ++c) {
			const float v = *d;
			if (fabsf(v) > _result.peak) { _result.peak = fabsf(v); }
			_bufs[c][s] = v;
			const unsigned int cc = c & cmask;
			if (_result.peaks[cc][pk].min > v) { _result.peaks[cc][pk].min = *d; }
			if (_result.peaks[cc][pk].max < v) { _result.peaks[cc][pk].max = *d; }
			_fft_data_in[s] += v * _hann_window[s] / (float) _channels;
			++d;
		}
	}

	for (; s < _bufsize; ++s) {
		_fft_data_in[s] = 0;
		for (unsigned int c = 0; c < _channels; ++c) {
			_bufs[c][s] = 0.f;
		}
	}

	if (_ebur128_plugin) {
		_ebur128_plugin->process (_bufs, Vamp::RealTime::fromSeconds ((double) _pos / _sample_rate));
	}

	float const * const data = c.data ();
	for (unsigned int c = 0; c < _channels; ++c) {
		if (!_dbtp_plugin[c]) { continue; }
		for (s = 0; s < n_samples; ++s) {
			_bufs[0][s] = data[s * _channels + c];
		}
		_dbtp_plugin[c]->process (_bufs, Vamp::RealTime::fromSeconds ((double) _pos / _sample_rate));
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

	const size_t height = sizeof (_result.spectrum[0]) / sizeof (float);
	const framecnt_t x0 = _pos / _fpp;
	framecnt_t x1 = (_pos + n_samples) / _fpp;
	if (x0 == x1) x1 = x0 + 1;
	const float range = 80; // dB

	for (uint32_t i = 1; i < _fft_data_size - 1; ++i) {
		const float level = fft_power_at_bin (i, i);
		if (level < -range) continue;
		const float pk = level > 0.0 ? 1.0 : (range + level) / range;
#if 0 // linear
		const uint32_t y0 = height - ceil (i * (float) height / _fft_data_size);
		uint32_t y1= height - ceil (i * (float) height / _fft_data_size);
#else // logscale
		const uint32_t y0 = height - ceilf (height * logf (1.f + .1f * i) / logf (1.f + .1f * _fft_data_size));
		uint32_t y1 = height - ceilf (height * logf (1.f + .1f * (i + 1.f)) / logf (1.f + .1f * _fft_data_size));
#endif
		if (y0 == y1 && y0 > 0) y1 = y0 - 1;
		for (int x = x0; x < x1; ++x) {
			for (uint32_t y = y0; y > y1; --y) {
				if (_result.spectrum[x][y] < pk) { _result.spectrum[x][y] = pk; }
			}
		}
	}

	_pos += n_samples;

	/* pass audio audio through */
	ListedSource<float>::output (c);
}

ARDOUR::ExportAnalysisPtr
Analyser::result ()
{
	//printf ("PROCESSED %ld / %ld samples\n", _pos, _n_samples);
	if (_pos == 0) {
		return ARDOUR::ExportAnalysisPtr ();
	}
	if (_ebur128_plugin) {
		Vamp::Plugin::FeatureSet features = _ebur128_plugin->getRemainingFeatures ();
		if (!features.empty () && features.size () == 3) {
			_result.loudness = features[0][0].values[0];
			_result.loudness_range = features[1][0].values[0];
			assert (features[2][0].values.size () == 540);
			for (int i = 0; i < 540; ++i) {
				_result.loudness_hist[i] = features[2][0].values[i];
				if (_result.loudness_hist[i] > _result.loudness_hist_max) {
					_result.loudness_hist_max = _result.loudness_hist[i]; }
			}
			_result.have_loudness = true;
		}
	}

	for (unsigned int c = 0; c < _channels; ++c) {
		if (!_dbtp_plugin[c]) { continue; }
		Vamp::Plugin::FeatureSet features = _dbtp_plugin[c]->getRemainingFeatures ();
		if (!features.empty () && features.size () == 1) {
			_result.have_dbtp = true;
			float p = features[0][0].values[0];
			if (p > _result.truepeak) { _result.truepeak = p; }
		}
	}

	return ARDOUR::ExportAnalysisPtr (new ARDOUR::ExportAnalysis (_result));
}

float
Analyser::fft_power_at_bin (const uint32_t b, const float norm) const
{
	const float a = _fft_power[b] * norm;
	return a > 1e-12 ? 10.0 * fast_log10 (a) : -INFINITY;
}
