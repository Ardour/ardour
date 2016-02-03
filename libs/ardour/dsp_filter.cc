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
 *
 */

#include <stdlib.h>
#include <math.h>
#include "ardour/dsp_filter.h"

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

BiQuad::BiQuad (double samplerate)
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

BiQuad::BiQuad (const BiQuad &other)
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
BiQuad::run (float *data, const uint32_t n_samples)
{
	for (uint32_t i = 0; i < n_samples; ++i) {
		const float xn = data[i];
		const float z = _b0 * xn + _z1;
		_z1           = _b1 * xn - _a1 * z + _z2;
		_z2           = _b2 * xn - _a2 * z;
		data[i] = z;
	}
}

void
BiQuad::compute (Type type, double freq, double Q, double gain)
{
	/* Compute biquad filter settings.
	 * Based on 'Cookbook formulae for audio EQ biquad filter coefficents'
	 * by Robert Bristow-Johnson
	 */
	const double     A = pow (10.0, (gain / 40.0));
	const double W0 = (2.0 * M_PI * freq) / _rate;
	const double sinW0  = sin (W0);
	const double cosW0  = cos (W0);
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
