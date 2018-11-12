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

const float Analyser::fft_range_db (120); // dB

Analyser::Analyser (float sample_rate, unsigned int channels, samplecnt_t bufsize, samplecnt_t n_samples)
	: LoudnessReader (sample_rate, channels, bufsize)
	, _n_samples (n_samples)
	, _pos (0)
{
	//printf ("NEW ANALYSER %p r:%.1f c:%d f:%ld l%ld\n", this, sample_rate, channels, bufsize, n_samples);
	assert (bufsize % channels == 0);
	assert (bufsize > 1);
	assert (_bufsize > 0);



	const size_t peaks = sizeof (_result.peaks) / sizeof (ARDOUR::PeakData::PeakDatum) / 4;
	_spp = ceil ((_n_samples + 2.f) / (float) peaks);

	const size_t swh = sizeof (_result.spectrum) / sizeof (float);
	const size_t height = sizeof (_result.spectrum[0]) / sizeof (float);
	const size_t width = swh / height;
	_fpp = ceil ((_n_samples + 2.f) / (float) width);

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
#define YPOS(FREQ) rint (height * (1.0 - FREQ / nyquist))
#else
#define YPOS(FREQ) rint (height * (1 - logf (1.f + .1f * _fft_data_size * FREQ / nyquist) / logf (1.f + .1f * _fft_data_size)))
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
	fftwf_destroy_plan (_fft_plan);
	fftwf_free (_fft_data_in);
	fftwf_free (_fft_data_out);
	free (_fft_power);
	free (_hann_window);
}

void
Analyser::process (ProcessContext<float> const & ctx)
{
	const samplecnt_t n_samples = ctx.samples () / ctx.channels ();
	assert (ctx.channels () == _channels);
	assert (ctx.samples () % ctx.channels () == 0);
	assert (n_samples <= _bufsize);
	//printf ("PROC %p @%ld F: %ld, S: %ld C:%d\n", this, _pos, ctx.samples (), n_samples, ctx.channels ());

	// allow 1 sample slack for resampling
	if (_pos + n_samples > _n_samples + 1) {
		_pos += n_samples;
		ListedSource<float>::output (ctx);
		return;
	}

	float const * d = ctx.data ();
	samplecnt_t s;
	const unsigned cmask = _result.n_channels - 1; // [0, 1]
	for (s = 0; s < n_samples; ++s) {
		_fft_data_in[s] = 0;
		const samplecnt_t pbin = (_pos + s) / _spp;
		for (unsigned int c = 0; c < _channels; ++c) {
			const float v = *d;
			if (fabsf(v) > _result.peak) { _result.peak = fabsf(v); }
			if (c < _result.n_channels) {
				_bufs[c][s] = v;
			}
			const unsigned int cc = c & cmask;
			if (_result.peaks[cc][pbin].min > v) { _result.peaks[cc][pbin].min = *d; }
			if (_result.peaks[cc][pbin].max < v) { _result.peaks[cc][pbin].max = *d; }
			_fft_data_in[s] += v * _hann_window[s] / (float) _channels;
			++d;
		}
	}

	for (; s < _bufsize; ++s) {
		_fft_data_in[s] = 0;
		for (unsigned int c = 0; c < _result.n_channels; ++c) {
			_bufs[c][s] = 0.f;
		}
	}

	if (_ebur_plugin) {
		_ebur_plugin->process (_bufs, Vamp::RealTime::fromSeconds ((double) _pos / _sample_rate));
	}

	float const * const data = ctx.data ();
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
	const samplecnt_t x0 = _pos / _fpp;
	samplecnt_t x1 = (_pos + n_samples) / _fpp;
	if (x0 == x1) x1 = x0 + 1;

	for (uint32_t i = 0; i < _fft_data_size - 1; ++i) {
		const float level = fft_power_at_bin (i, i);
		if (level < -fft_range_db) continue;
		const float pk = level > 0.0 ? 1.0 : (fft_range_db + level) / fft_range_db;
#if 0 // linear
		const uint32_t y0 = floor (i * (float) height / _fft_data_size);
		uint32_t y1 = ceil ((i + 1.0) * (float) height / _fft_data_size);
#else // logscale
		const uint32_t y0 = floor (height * logf (1.f + .1f * i) / logf (1.f + .1f * _fft_data_size));
		uint32_t y1 = ceilf (height * logf (1.f + .1f * (i + 1.f)) / logf (1.f + .1f * _fft_data_size));
#endif
		assert (y0 < height);
		assert (y1 > 0 && y1 <= height);
		if (y0 == y1) y1 = y0 + 1;
		for (int x = x0; x < x1; ++x) {
			for (uint32_t y = y0; y < y1 && y < height; ++y) {
				uint32_t yy = height - 1 - y;
				if (_result.spectrum[x][yy] < pk) { _result.spectrum[x][yy] = pk; }
			}
		}
	}

	_pos += n_samples;

	/* pass audio audio through */
	ListedSource<float>::output (ctx);
}

ARDOUR::ExportAnalysisPtr
Analyser::result ()
{
	//printf ("PROCESSED %ld / %ld samples\n", _pos, _n_samples);
	if (_pos == 0 || _pos > _n_samples + 1) {
		return ARDOUR::ExportAnalysisPtr ();
	}

	if (_pos + 1 < _n_samples) {
		// crude re-bin (silence stripped version)
		const size_t peaks = sizeof (_result.peaks) / sizeof (ARDOUR::PeakData::PeakDatum) / 4;
		for (samplecnt_t b = peaks - 1; b > 0; --b) {
			for (unsigned int c = 0; c < _result.n_channels; ++c) {
				const samplecnt_t sb = b * _pos / _n_samples;
				_result.peaks[c][b].min = _result.peaks[c][sb].min;
				_result.peaks[c][b].max = _result.peaks[c][sb].max;
			}
		}

		const size_t swh = sizeof (_result.spectrum) / sizeof (float);
		const size_t height = sizeof (_result.spectrum[0]) / sizeof (float);
		const size_t width = swh / height;
		for (samplecnt_t b = width - 1; b > 0; --b) {
			// TODO round down to prev _fft_data_size bin
			const samplecnt_t sb = b * _pos / _n_samples;
			for (unsigned int y = 0; y < height; ++y) {
				_result.spectrum[b][y] = _result.spectrum[sb][y];
			}
		}
	}

	if (_ebur_plugin) {
		Vamp::Plugin::FeatureSet features = _ebur_plugin->getRemainingFeatures ();
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

	const unsigned cmask = _result.n_channels - 1; // [0, 1]
	for (unsigned int c = 0; c < _channels; ++c) {
		if (!_dbtp_plugin[c]) { continue; }
		Vamp::Plugin::FeatureSet features = _dbtp_plugin[c]->getRemainingFeatures ();
		if (!features.empty () && features.size () == 2) {
			_result.have_dbtp = true;
			float p = features[0][0].values[0];
			if (p > _result.truepeak) { _result.truepeak = p; }

			for (std::vector<float>::const_iterator i = features[1][0].values.begin();
					i != features[1][0].values.end(); ++i) {
				const samplecnt_t pk = (*i) / _spp;
				const unsigned int cc = c & cmask;
				_result.truepeakpos[cc].insert (pk);
			}
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
