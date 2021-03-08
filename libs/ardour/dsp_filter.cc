/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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

#include <algorithm>
#include <stdlib.h>
#include <cmath>
#include <boost/math/special_functions/fpclassify.hpp>

#include "ardour/dB.h"
#include "ardour/buffer.h"
#include "ardour/dsp_filter.h"
#include "ardour/runtime_functions.h"

#ifdef COMPILER_MSVC
#include <float.h>
#define isfinite_local(val) (bool)_finite((double)val)
#else
#define isfinite_local std::isfinite
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace ARDOUR::DSP;

void
ARDOUR::DSP::memset (float *data, const float val, const uint32_t n_samples) {
	for (uint32_t i = 0; i < n_samples; ++i) {
		data[i] = val;
	}
}

void
ARDOUR::DSP::mmult (float *data, float *mult, const uint32_t n_samples) {
	for (uint32_t i = 0; i < n_samples; ++i) {
		data[i] *= mult[i];
	}
}

float
ARDOUR::DSP::log_meter (float power) {
	// compare to libs/ardour/log_meter.h
	static const float lower_db = -192.f;
	static const float upper_db = 0.f;
	static const float non_linearity = 8.0;
	return (power < lower_db ? 0.0 : powf ((power - lower_db) / (upper_db - lower_db), non_linearity));
}

float
ARDOUR::DSP::log_meter_coeff (float coeff) {
	if (coeff <= 0) return 0;
	return log_meter (fast_coefficient_to_dB (coeff));
}

void
ARDOUR::DSP::peaks (const float *data, float &min, float &max, uint32_t n_samples) {
	ARDOUR::find_peaks (data, n_samples, &min, &max);
}

void
ARDOUR::DSP::process_map (BufferSet* bufs, const ChanCount& n_out, const ChanMapping& in_map, const ChanMapping& out_map, pframes_t nframes, samplecnt_t offset)
{
	/* PluginInsert already handles most, in particular `no-inplace` buffers in case
	 * or x-over connections, and through connections.
	 *
	 * This just fills output buffers, forwarding inputs as needed:
	 * Input -> plugin-sink == plugin-src -> Output
	 */
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		for (uint32_t out = 0; out < n_out.get (*t); ++out) {
			bool valid;
			uint32_t out_idx = out_map.get (*t, out, &valid);
			if (!valid) {
				continue;
			}
			uint32_t in_idx = in_map.get (*t, out, &valid);
			if (!valid) {
				bufs->get_available (*t, out_idx).silence (nframes, offset);
				continue;
			}
			if (in_idx != out_idx) {
				bufs->get_available (*t, out_idx).read_from (bufs->get_available (*t, in_idx), nframes, offset, offset);
			}
		}
	}
}

LowPass::LowPass (double samplerate, float freq)
	: _rate (samplerate)
	, _z (0)
{
	set_cutoff (freq);
}

void
LowPass::set_cutoff (float freq)
{
	_a = 1.f - expf (-2.f * M_PI * freq / _rate);
}

void
LowPass::proc (float *data, const uint32_t n_samples)
{
	// localize variables
	const float a = _a;
	float z = _z;
	for (uint32_t i = 0; i < n_samples; ++i) {
		data[i] += a * (data[i] - z);
		z = data[i];
	}
	_z = z;
	if (!isfinite_local (_z)) { _z = 0; }
	else if (!boost::math::isnormal (_z)) { _z = 0; }
}

void
LowPass::ctrl (float *data, const float val, const uint32_t n_samples)
{
	// localize variables
	const float a = _a;
	float z = _z;
	for (uint32_t i = 0; i < n_samples; ++i) {
		data[i] += a * (val - z);
		z = data[i];
	}
	_z = z;
}

///////////////////////////////////////////////////////////////////////////////

Biquad::Biquad (double samplerate)
	: _rate (samplerate)
	, _z1 (0.0)
	, _z2 (0.0)
	, _a1 (0.0)
	, _a2 (0.0)
	, _b0 (1.0)
	, _b1 (0.0)
	, _b2 (0.0)
{
}

Biquad::Biquad (const Biquad &other)
	: _rate (other._rate)
	, _z1 (0.0)
	, _z2 (0.0)
	, _a1 (other._a1)
	, _a2 (other._a2)
	, _b0 (other._b0)
	, _b1 (other._b1)
	, _b2 (other._b2)
{
}

void
Biquad::run (float *data, const uint32_t n_samples)
{
	for (uint32_t i = 0; i < n_samples; ++i) {
		const float xn = data[i];
		const float z = _b0 * xn + _z1;
		_z1           = _b1 * xn - _a1 * z + _z2;
		_z2           = _b2 * xn - _a2 * z;
		data[i] = z;
	}

	if (!isfinite_local (_z1)) { _z1 = 0; }
	else if (!boost::math::isnormal (_z1)) { _z1 = 0; }
	if (!isfinite_local (_z2)) { _z2 = 0; }
	else if (!boost::math::isnormal (_z2)) { _z2 = 0; }
}

void
Biquad::configure (double a1, double a2, double b0, double b1, double b2)
{
	_a1 = a1;
	_a2 = a2;
	_b0 = b0;
	_b1 = b1;
	_b2 = b2;
}

void
Biquad::compute (Type type, double freq, double Q, double gain)
{
	if (Q <= .001)  { Q = 0.001; }
	if (freq <= 1.) { freq = 1.; }
	if (freq >= 0.4998 * _rate) { freq = 0.4998 * _rate; }

	/* Compute biquad filter settings.
	 * Based on 'Cookbook formulae for audio EQ biquad filter coefficents'
	 * by Robert Bristow-Johnson
	 */
	const double A = pow (10.0, (gain / 40.0));
	const double W0 = (2.0 * M_PI * freq) / _rate;
	const double sinW0 = sin (W0);
	const double cosW0 = cos (W0);
	const double alpha = sinW0 / (2.0 * Q);
	const double beta  = sqrt (A) / Q;

	double _a0;

	switch (type) {
		case LowPass:
			_b0 = (1.0 - cosW0) / 2.0;
			_b1 =  1.0 - cosW0;
			_b2 = (1.0 - cosW0) / 2.0;
			_a0 =  1.0 + alpha;
			_a1 = -2.0 * cosW0;
			_a2 =  1.0 - alpha;
			break;

		case HighPass:
			_b0 =  (1.0 + cosW0) / 2.0;
			_b1 = -(1.0 + cosW0);
			_b2 =  (1.0 + cosW0) / 2.0;
			_a0 =   1.0 + alpha;
			_a1 =  -2.0 * cosW0;
			_a2 =   1.0 - alpha;
			break;

		case BandPassSkirt: /* Constant skirt gain, peak gain = Q */
			_b0 =  sinW0 / 2.0;
			_b1 =  0.0;
			_b2 = -sinW0 / 2.0;
			_a0 =  1.0 + alpha;
			_a1 = -2.0 * cosW0;
			_a2 =  1.0 - alpha;
			break;

		case BandPass0dB: /* Constant 0 dB peak gain */
			_b0 =  alpha;
			_b1 =  0.0;
			_b2 = -alpha;
			_a0 =  1.0 + alpha;
			_a1 = -2.0 * cosW0;
			_a2 =  1.0 - alpha;
			break;

		case Notch:
			_b0 =  1.0;
			_b1 = -2.0 * cosW0;
			_b2 =  1.0;
			_a0 =  1.0 + alpha;
			_a1 = -2.0 * cosW0;
			_a2 =  1.0 - alpha;
			break;

		case AllPass:
			_b0 =  1.0 - alpha;
			_b1 = -2.0 * cosW0;
			_b2 =  1.0 + alpha;
			_a0 =  1.0 + alpha;
			_a1 = -2.0 * cosW0;
			_a2 =  1.0 - alpha;
			break;

		case Peaking:
			_b0 =  1.0 + (alpha * A);
			_b1 = -2.0 * cosW0;
			_b2 =  1.0 - (alpha * A);
			_a0 =  1.0 + (alpha / A);
			_a1 = -2.0 * cosW0;
			_a2 =  1.0 - (alpha / A);
			break;

		case LowShelf:
			_b0 =         A * ((A + 1) - ((A - 1) * cosW0) + (beta * sinW0));
			_b1 = (2.0 * A) * ((A - 1) - ((A + 1) * cosW0));
			_b2 =         A * ((A + 1) - ((A - 1) * cosW0) - (beta * sinW0));
			_a0 =              (A + 1) + ((A - 1) * cosW0) + (beta * sinW0);
			_a1 =      -2.0 * ((A - 1) + ((A + 1) * cosW0));
			_a2 =              (A + 1) + ((A - 1) * cosW0) - (beta * sinW0);
			break;

		case HighShelf:
			_b0 =          A * ((A + 1) + ((A - 1) * cosW0) + (beta * sinW0));
			_b1 = -(2.0 * A) * ((A - 1) + ((A + 1) * cosW0));
			_b2 =          A * ((A + 1) + ((A - 1) * cosW0) - (beta * sinW0));
			_a0 =               (A + 1) - ((A - 1) * cosW0) + (beta * sinW0);
			_a1 =        2.0 * ((A - 1) - ((A + 1) * cosW0));
			_a2 =               (A + 1) - ((A - 1) * cosW0) - (beta * sinW0);
			break;
		default:
			abort(); /*NOTREACHED*/
			break;
	}

	_b0 /= _a0;
	_b1 /= _a0;
	_b2 /= _a0;
	_a1 /= _a0;
	_a2 /= _a0;
}

float
Biquad::dB_at_freq (float freq) const
{
	const double W0 = (2.0 * M_PI * freq) / _rate;
	const float c1 = cosf (W0);
	const float s1 = sinf (W0);

	const float A = _b0 + _b2;
	const float B = _b0 - _b2;
	const float C = 1.0 + _a2;
	const float D = 1.0 - _a2;

	const float a = A * c1 + _b1;
	const float b = B * s1;
	const float c = C * c1 + _a1;
	const float d = D * s1;

#define SQUARE(x) ( (x) * (x) )
	float rv = 20.f * log10f (sqrtf ((SQUARE(a) + SQUARE(b)) * (SQUARE(c) + SQUARE(d))) / (SQUARE(c) + SQUARE(d)));
	if (!isfinite_local (rv)) { rv = 0; }
	return std::min (120.f, std::max(-120.f, rv));
}


Glib::Threads::Mutex FFTSpectrum::fft_planner_lock;

FFTSpectrum::FFTSpectrum (uint32_t window_size, double rate)
	: hann_window (0)
{
	init (window_size, rate);
}

FFTSpectrum::~FFTSpectrum ()
{
	{
		Glib::Threads::Mutex::Lock lk (fft_planner_lock);
		fftwf_destroy_plan (_fftplan);
	}
	fftwf_free (_fft_data_in);
	fftwf_free (_fft_data_out);
	free (_fft_power);
	free (hann_window);
}

void
FFTSpectrum::init (uint32_t window_size, double rate)
{
	assert (window_size > 0);
	Glib::Threads::Mutex::Lock lk (fft_planner_lock);

	_fft_window_size = window_size;
	_fft_data_size   = window_size / 2;
	_fft_freq_per_bin = rate / _fft_data_size / 2.f;

	_fft_data_in  = (float *) fftwf_malloc (sizeof(float) * _fft_window_size);
	_fft_data_out = (float *) fftwf_malloc (sizeof(float) * _fft_window_size);
	_fft_power    = (float *) malloc (sizeof(float) * _fft_data_size);

	reset ();

	_fftplan = fftwf_plan_r2r_1d (_fft_window_size, _fft_data_in, _fft_data_out, FFTW_R2HC, FFTW_MEASURE);

	hann_window  = (float *) malloc(sizeof(float) * window_size);
	double sum = 0.0;

	for (uint32_t i = 0; i < window_size; ++i) {
		hann_window[i] = 0.5f - (0.5f * (float) cos (2.0f * M_PI * (float)i / (float)(window_size)));
		sum += hann_window[i];
	}
	const double isum = 2.0 / sum;
	for (uint32_t i = 0; i < window_size; ++i) {
		hann_window[i] *= isum;
	}
}

void
FFTSpectrum::reset ()
{
	for (uint32_t i = 0; i < _fft_data_size; ++i) {
		_fft_power[i] = 0;
	}
	for (uint32_t i = 0; i < _fft_window_size; ++i) {
		_fft_data_out[i] = 0;
	}
}

void
FFTSpectrum::set_data_hann (float const * const data, uint32_t n_samples, uint32_t offset)
{
	assert(n_samples + offset <= _fft_window_size);
	for (uint32_t i = 0; i < n_samples; ++i) {
		_fft_data_in[i + offset] = data[i] * hann_window[i + offset];
	}
}

void
FFTSpectrum::execute ()
{
	fftwf_execute (_fftplan);

	_fft_power[0] = _fft_data_out[0] * _fft_data_out[0];

#define FRe (_fft_data_out[i])
#define FIm (_fft_data_out[_fft_window_size - i])
	for (uint32_t i = 1; i < _fft_data_size - 1; ++i) {
		_fft_power[i] = (FRe * FRe) + (FIm * FIm);
		//_fft_phase[i] = atan2f (FIm, FRe);
	}
#undef FRe
#undef FIm
}

float
FFTSpectrum::power_at_bin (const uint32_t b, const float norm) const {
	assert (b < _fft_data_size);
	const float a = _fft_power[b] * norm;
	return a > 1e-12 ? 10.0 * fast_log10 (a) : -INFINITY;
}

Generator::Generator ()
	: _type (UniformWhiteNoise)
	, _rseed (1)
{
	set_type (UniformWhiteNoise);
}

void
Generator::set_type (Generator::Type t) {
	_type = t;
	_b0 = _b1 = _b2 = _b3 = _b4 = _b5 = _b6 = 0;
	_pass = false;
	_rn = 0;
}

void
Generator::run (float *data, const uint32_t n_samples)
{
	switch (_type) {
		default:
		case UniformWhiteNoise:
			for (uint32_t i = 0; i < n_samples; ++i) {
				data[i] = randf();
			}
			break;
		case GaussianWhiteNoise:
			for (uint32_t i = 0 ; i < n_samples; ++i) {
				data[i] = 0.7079f * grandf();
			}
			break;
		case PinkNoise:
			for (uint32_t i = 0 ; i < n_samples; ++i) {
				const float white = .39572f * randf ();
				_b0 = .99886f * _b0 + white * .0555179f;
				_b1 = .99332f * _b1 + white * .0750759f;
				_b2 = .96900f * _b2 + white * .1538520f;
				_b3 = .86650f * _b3 + white * .3104856f;
				_b4 = .55000f * _b4 + white * .5329522f;
				_b5 = -.7616f * _b5 - white * .0168980f;
				data[i] = _b0 + _b1 + _b2 + _b3 + _b4 + _b5 + _b6 + white * 0.5362f;
				_b6 = white * 0.115926f;
			}
			break;
	}
}

inline uint32_t
Generator::randi ()
{
	// 31bit Park-Miller-Carta Pseudo-Random Number Generator
	uint32_t hi, lo;
	lo = 16807 * (_rseed & 0xffff);
	hi = 16807 * (_rseed >> 16);
	lo += (hi & 0x7fff) << 16;
	lo += hi >> 15;
	lo = (lo & 0x7fffffff) + (lo >> 31);
	return (_rseed = lo);
}

inline float
Generator::grandf ()
{
	float x1, x2, r;

	if (_pass) {
		_pass = false;
		return _rn;
	}

	do {
		x1 = randf ();
		x2 = randf ();
		r = x1 * x1 + x2 * x2;
	} while ((r >= 1.0f) || (r < 1e-22f));

	r = sqrtf (-2.f * logf (r) / r);

	_pass = true;
	_rn = r * x2;
	return r * x1;
}
