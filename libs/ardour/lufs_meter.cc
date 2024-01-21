/*
 * Copyright (C) 2016,2023 Robin Gareus <robin@gareus.org>
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
#include <cmath>
#include <cstring>

#ifdef COMPILER_MSVC
#include <float.h>
#define isfinite_local(val) (bool)_finite ((double)val)
#else
#define isfinite_local std::isfinite
#endif

#include "pbd/failed_constructor.h"

#include "ardour/dB.h"
#include "ardour/lufs_meter.h"

using namespace ARDOUR;

void
LUFSMeter::FilterState::reset ()
{
	z1 = z2 = z3 = z4 = 0;
}

void
LUFSMeter::FilterState::sanitize ()
{
	z1 = !isfinite_local (z1) ? 0 : z1;
	z2 = !isfinite_local (z2) ? 0 : z2;
	z3 = !isfinite_local (z3) ? 0 : z3;
	z4 = !isfinite_local (z4) ? 0 : z4;
}

LUFSMeter::LUFSMeter (double samplerate, uint32_t n_channels)
	: _samplerate (samplerate)
	, _n_channels (n_channels)
{
	if (_n_channels > 5 || _n_channels == 0) {
		throw failed_constructor ();
	}
	_n_fragment = samplerate / 10;

	using std::placeholders::_1;
	using std::placeholders::_2;
	if (samplerate > 48000) {
		upsample = std::bind (&LUFSMeter::upsample_x2, this, _1, _2);
	} else {
		upsample = std::bind (&LUFSMeter::upsample_x4, this, _1, _2);
	}

	for (uint32_t c = 0; c < 5; ++c) {
		_z[c] = new float[48];
	}

	init ();
	reset ();
}

LUFSMeter::~LUFSMeter ()
{
	for (uint32_t c = 0; c < 5; ++c) {
		delete[] _z[c];
	}
}

void
LUFSMeter::init ()
{
	float a, b, c, d, r, u, w1, w2;

	/* shelf */
	r  = 1 / tan (4712.3890f / _samplerate);
	w1 = r / 1.121f;
	w2 = r * 1.121f;

	u = 1.4085f + 210.0f / _samplerate;
	a = w1 * u;
	b = w1 * w1;

	c = w2 * u;
	d = w2 * w2;

	r   = 1 + a + b;
	_a0 = (1 + c + d) / r;
	_a1 = (2 - 2 * d) / r;
	_a2 = (1 - c + d) / r;
	_b1 = (2 - 2 * b) / r;
	_b2 = (1 - a + b) / r;

	/* HP */
	r = 48.0f / _samplerate;
	a = 4.9886075f * r;
	b = 6.2298014f * r * r;
	r = 1 + a + b;
	a *= 2 / r;
	b *= 4 / r;

	_c3 = a + b;
	_c4 = b;

	/* normalize */
	r = 1.004995f / r;
	_a0 *= r;
	_a1 *= r;
	_a2 *= r;
}

void
LUFSMeter::reset ()
{
	for (uint32_t c = 0; c < _n_channels; ++c) {
		_fst[c].reset ();
		memset (_z[c], 0, 48 * sizeof (float));
	}
	_frag_pos = _n_fragment;
	_frag_pwr = 1e-30f;

	_maxloudn_M = -200;
	_integrated = -200;

	_thresh_rel = -70;
	_block_pwr  = 0.0;
	_block_cnt  = 0;
	_pow_idx    = 0;
	_dbtp       = 0;

	memset (_power, 0, 8 * sizeof (float));

	_hist.clear ();
}

void
LUFSMeter::run (float const** data, uint32_t n_samples)
{
	uint32_t offset = 0;

	calc_true_peak (data, n_samples);

	while (n_samples > 0) {
		uint32_t n = (_frag_pos < n_samples) ? _frag_pos : n_samples;

		_frag_pwr += process (data, n, offset);
		_frag_pos -= n;
		offset += n;
		n_samples -= n;

		if (_frag_pos == 0) {
			/* every 100 ms */

			_power[_pow_idx++] = _frag_pwr / (float)_n_fragment;
			_pow_idx &= 7;
			_frag_pwr = 1e-30f;
			_frag_pos = _n_fragment;

			const float sum_m      = sumfrag (4); // 400ms
			const float loudness_m = -0.691f + 10.f * log10f (sum_m);

			_momentary_l = loudness_m;

			_maxloudn_M = std::max<float> (_maxloudn_M, loudness_m);

			/* observe 400ms window every 100ms */
			if (loudness_m > -70.f) {
				_block_pwr += sum_m;
				++_block_cnt;
				/* see ITU-R BS.1770-3, page 6 */
				_thresh_rel = -10.691 + 10.f * log10f (_block_pwr / _block_cnt);
			}

			if (loudness_m > -100.f) {
				_hist[round (loudness_m * 10.f)] += 1;
			}

			if (_hist.size () == 0) {
				continue;
			}

			if (_thresh_rel < (--_hist.end ())->first * 0.1) {
				int b = _thresh_rel * 10.f;
				while (_hist.find (b) == _hist.end ()) {
					++b; // += .1LU
				}
				int    n   = 0;
				double sum = 0.0;

				for (auto i = _hist.find (b); i != _hist.end (); ++i) {
					n += i->second;
					const double s = powf (10.0, (i->first * 0.1 + 0.691) * 0.1);
					sum += i->second * s;
				}
				if (n > 0) {
					_integrated = -0.691f + 10.f * log10f (sum / n);
				}
			}
		}
	}
}

float
LUFSMeter::process (float const** data, const uint32_t n_samples, uint32_t off)
{
	float l = 0;
	for (uint32_t c = 0; c < _n_channels; ++c) {
		float const* d = data[c];
		FilterState& z = _fst[c];
		float        s = 0;
		for (uint32_t i = 0; i < n_samples; ++i) {
			float x = d[i + off] - _b1 * z.z1 - _b2 * z.z2 + 1e-15f;
			float y = _a0 * x + _a1 * z.z1 + _a2 * z.z2 - _c3 * z.z3 - _c4 * z.z4;
			z.z2    = z.z1;
			z.z1    = x;
			z.z4 += z.z3;
			z.z3 += y;
			s += y * y;
		}
		l += s * _g[c];
		z.sanitize ();
	}

	if (_n_channels == 1) {
		l *= 2;
	}
	return l;
}

float
LUFSMeter::sumfrag (uint32_t n_frag) const
{
	float s = 0;
	int   k = (8 + _pow_idx - n_frag) & 7;
	for (uint32_t i = 0; i < n_frag; i++) {
		s += _power[(i + k) & 7];
	}
	return s / n_frag;
}

float
LUFSMeter::integrated_loudness () const
{
	return _integrated;
}

float
LUFSMeter::momentary () const
{
	return _momentary_l;
}

float
LUFSMeter::max_momentary () const
{
	return _maxloudn_M;
}

float
LUFSMeter::dbtp () const
{
	return accurate_coefficient_to_dB (_dbtp);
}

float
LUFSMeter::upsample_x2 (int chn, float const x)
{
	float* r = _z[chn];
	float  u[2];
	r[47] = x;
	/* 2x upsample for true-peak analysis, cosine windowed sinc.  */

	/* clang-format off */
	u[0] = r[47];
	u[1] = r[ 0] * -1.450055e-05f + r[ 1] * +1.359163e-04f + r[ 2] * -3.928527e-04f + r[ 3] * +8.006445e-04f
	     + r[ 4] * -1.375510e-03f + r[ 5] * +2.134915e-03f + r[ 6] * -3.098103e-03f + r[ 7] * +4.286860e-03f
	     + r[ 8] * -5.726614e-03f + r[ 9] * +7.448018e-03f + r[10] * -9.489286e-03f + r[11] * +1.189966e-02f
	     + r[12] * -1.474471e-02f + r[13] * +1.811472e-02f + r[14] * -2.213828e-02f + r[15] * +2.700557e-02f
	     + r[16] * -3.301023e-02f + r[17] * +4.062971e-02f + r[18] * -5.069345e-02f + r[19] * +6.477499e-02f
	     + r[20] * -8.625619e-02f + r[21] * +1.239454e-01f + r[22] * -2.101678e-01f + r[23] * +6.359382e-01f
	     + r[24] * +6.359382e-01f + r[25] * -2.101678e-01f + r[26] * +1.239454e-01f + r[27] * -8.625619e-02f
	     + r[28] * +6.477499e-02f + r[29] * -5.069345e-02f + r[30] * +4.062971e-02f + r[31] * -3.301023e-02f
	     + r[32] * +2.700557e-02f + r[33] * -2.213828e-02f + r[34] * +1.811472e-02f + r[35] * -1.474471e-02f
	     + r[36] * +1.189966e-02f + r[37] * -9.489286e-03f + r[38] * +7.448018e-03f + r[39] * -5.726614e-03f
	     + r[40] * +4.286860e-03f + r[41] * -3.098103e-03f + r[42] * +2.134915e-03f + r[43] * -1.375510e-03f
	     + r[44] * +8.006445e-04f + r[45] * -3.928527e-04f + r[46] * +1.359163e-04f + r[47] * -1.450055e-05f;
	/* clang-format on */
	
	for (int i = 0; i < 47; ++i) {
		r[i] = r[i + 1];
	}

	return std::max (u[0], u[1]);
}

float
LUFSMeter::upsample_x4 (int chn, float const x)
{
	float* r = _z[chn];
	float  u[4];
	r[47] = x;
	/* 4x upsample for true-peak analysis, cosine windowed sinc.
	 * This effectively introduces a latency of 23 samples
	 */

	/* clang-format off */
	u[0] = r[47];
	u[1] = r[ 0] * -2.330790e-05f + r[ 1] * +1.321291e-04f + r[ 2] * -3.394408e-04f + r[ 3] * +6.562235e-04f
	     + r[ 4] * -1.094138e-03f + r[ 5] * +1.665807e-03f + r[ 6] * -2.385230e-03f + r[ 7] * +3.268371e-03f
	     + r[ 8] * -4.334012e-03f + r[ 9] * +5.604985e-03f + r[10] * -7.109989e-03f + r[11] * +8.886314e-03f
	     + r[12] * -1.098403e-02f + r[13] * +1.347264e-02f + r[14] * -1.645206e-02f + r[15] * +2.007155e-02f
	     + r[16] * -2.456432e-02f + r[17] * +3.031531e-02f + r[18] * -3.800644e-02f + r[19] * +4.896667e-02f
	     + r[20] * -6.616853e-02f + r[21] * +9.788141e-02f + r[22] * -1.788607e-01f + r[23] * +9.000753e-01f
	     + r[24] * +2.993829e-01f + r[25] * -1.269367e-01f + r[26] * +7.922398e-02f + r[27] * -5.647748e-02f
	     + r[28] * +4.295093e-02f + r[29] * -3.385706e-02f + r[30] * +2.724946e-02f + r[31] * -2.218943e-02f
	     + r[32] * +1.816976e-02f + r[33] * -1.489313e-02f + r[34] * +1.217411e-02f + r[35] * -9.891211e-03f
	     + r[36] * +7.961470e-03f + r[37] * -6.326144e-03f + r[38] * +4.942202e-03f + r[39] * -3.777065e-03f
	     + r[40] * +2.805240e-03f + r[41] * -2.006106e-03f + r[42] * +1.362416e-03f + r[43] * -8.592768e-04f
	     + r[44] * +4.834383e-04f + r[45] * -2.228007e-04f + r[46] * +6.607267e-05f + r[47] * -2.537056e-06f;
	u[2] = r[ 0] * -1.450055e-05f + r[ 1] * +1.359163e-04f + r[ 2] * -3.928527e-04f + r[ 3] * +8.006445e-04f
	     + r[ 4] * -1.375510e-03f + r[ 5] * +2.134915e-03f + r[ 6] * -3.098103e-03f + r[ 7] * +4.286860e-03f
	     + r[ 8] * -5.726614e-03f + r[ 9] * +7.448018e-03f + r[10] * -9.489286e-03f + r[11] * +1.189966e-02f
	     + r[12] * -1.474471e-02f + r[13] * +1.811472e-02f + r[14] * -2.213828e-02f + r[15] * +2.700557e-02f
	     + r[16] * -3.301023e-02f + r[17] * +4.062971e-02f + r[18] * -5.069345e-02f + r[19] * +6.477499e-02f
	     + r[20] * -8.625619e-02f + r[21] * +1.239454e-01f + r[22] * -2.101678e-01f + r[23] * +6.359382e-01f
	     + r[24] * +6.359382e-01f + r[25] * -2.101678e-01f + r[26] * +1.239454e-01f + r[27] * -8.625619e-02f
	     + r[28] * +6.477499e-02f + r[29] * -5.069345e-02f + r[30] * +4.062971e-02f + r[31] * -3.301023e-02f
	     + r[32] * +2.700557e-02f + r[33] * -2.213828e-02f + r[34] * +1.811472e-02f + r[35] * -1.474471e-02f
	     + r[36] * +1.189966e-02f + r[37] * -9.489286e-03f + r[38] * +7.448018e-03f + r[39] * -5.726614e-03f
	     + r[40] * +4.286860e-03f + r[41] * -3.098103e-03f + r[42] * +2.134915e-03f + r[43] * -1.375510e-03f
	     + r[44] * +8.006445e-04f + r[45] * -3.928527e-04f + r[46] * +1.359163e-04f + r[47] * -1.450055e-05f;
	u[3] = r[ 0] * -2.537056e-06f + r[ 1] * +6.607267e-05f + r[ 2] * -2.228007e-04f + r[ 3] * +4.834383e-04f
	     + r[ 4] * -8.592768e-04f + r[ 5] * +1.362416e-03f + r[ 6] * -2.006106e-03f + r[ 7] * +2.805240e-03f
	     + r[ 8] * -3.777065e-03f + r[ 9] * +4.942202e-03f + r[10] * -6.326144e-03f + r[11] * +7.961470e-03f
	     + r[12] * -9.891211e-03f + r[13] * +1.217411e-02f + r[14] * -1.489313e-02f + r[15] * +1.816976e-02f
	     + r[16] * -2.218943e-02f + r[17] * +2.724946e-02f + r[18] * -3.385706e-02f + r[19] * +4.295093e-02f
	     + r[20] * -5.647748e-02f + r[21] * +7.922398e-02f + r[22] * -1.269367e-01f + r[23] * +2.993829e-01f
	     + r[24] * +9.000753e-01f + r[25] * -1.788607e-01f + r[26] * +9.788141e-02f + r[27] * -6.616853e-02f
	     + r[28] * +4.896667e-02f + r[29] * -3.800644e-02f + r[30] * +3.031531e-02f + r[31] * -2.456432e-02f
	     + r[32] * +2.007155e-02f + r[33] * -1.645206e-02f + r[34] * +1.347264e-02f + r[35] * -1.098403e-02f
	     + r[36] * +8.886314e-03f + r[37] * -7.109989e-03f + r[38] * +5.604985e-03f + r[39] * -4.334012e-03f
	     + r[40] * +3.268371e-03f + r[41] * -2.385230e-03f + r[42] * +1.665807e-03f + r[43] * -1.094138e-03f
	     + r[44] * +6.562235e-04f + r[45] * -3.394408e-04f + r[46] * +1.321291e-04f + r[47] * -2.330790e-05f;
	/* clang-format on */

	for (int i = 0; i < 47; ++i) {
		r[i] = r[i + 1];
	}

	float p1 = std::max (fabsf (u[0]), fabsf (u[1]));
	float p2 = std::max (fabsf (u[2]), fabsf (u[3]));
	return std::max (p1, p2);
}

void
LUFSMeter::calc_true_peak (float const** data, const uint32_t n_samples)
{
	for (uint32_t c = 0; c < _n_channels; ++c) {
		float const* d = data[c];
		for (uint32_t i = 0; i < n_samples; ++i) {
			float peak = upsample (c, d[i]);
			_dbtp      = std::max (_dbtp, peak);
		}
	}
}
