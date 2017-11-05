// ----------------------------------------------------------------------------
//
//  Copyright (C) 2006-2013 Fons Adriaensen <fons@linuxaudio.org>
//  Copyright (C) 2017 Robin Gareus <robin@gareus.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>

#include "zita-resampler/vmresampler.h"

using namespace ArdourZita;

VMResampler::VMResampler (void)
	: _table (0)
  , _buff  (0)
  , _c1 (0)
  , _c2 (0)
{
	reset ();
}

VMResampler::~VMResampler (void)
{
	clear ();
}

int
VMResampler::setup (unsigned int hlen)
{
	if ((hlen < 8) || (hlen > 96)) return 1;
	return setup (hlen, 1.0 - 2.6 / hlen);
}

int
VMResampler::setup (unsigned int hlen, double frel)
{
	unsigned int       h, k, n;
	double             s;
	Resampler_table    *T = 0;

	n = NPHASE;
	s = n;
	h = hlen;
	k = 250;
	T = Resampler_table::create (frel, h, n);
	clear ();
	if (T) {
		_table = T;
		_buff  = new float [2 * h - 1 + k];
		_c1 = new float [2 * h];
		_c2 = new float [2 * h];
		_inmax = k;
		_pstep = s;
		_qstep = s;
		_wstep = 1;
		return reset ();
	}
	else return 1;
}

void
VMResampler::clear (void)
{
	Resampler_table::destroy (_table);
	delete[] _buff;
	delete[] _c1;
	delete[] _c2;
	_buff  = 0;
	_c1 = 0;
	_c2 = 0;
	_table = 0;
	_inmax = 0;
	_pstep = 0;
	_qstep = 0;
	_wstep = 1;
	reset ();
}

void
VMResampler::set_phase (double p)
{
	if (!_table) return;
	_phase = (p - floor (p)) * _table->_np;
}

void
VMResampler::set_rrfilt (double t)
{
	if (!_table) return;
	_wstep =  (t < 1) ? 1 : 1 - exp (-1 / t);
}

double
VMResampler::set_rratio (double r)
{
	if (!_table) return 0;
	if (r > 16.0) r = 16.0;
	if (r < 0.02) r = 0.02;

	_qstep = _table->_np / r;

	if (_qstep < 4.) {
		_qstep = 4.;
	}
	if (_qstep > 2. * _table->_np * _table->_hl) {
		_qstep = 2. * _table->_np * _table->_hl;
	}
	return _table->_np / _qstep;
}

double
VMResampler::inpdist (void) const
{
	if (!_table) return 0;
	return (int)(_table->_hl + 1 - _nread) - _phase / _table->_np;
}

int
VMResampler::inpsize (void) const
{
	if (!_table) return 0;
	return 2 * _table->_hl;
}

int
VMResampler::reset (void)
{
	if (!_table) return 1;

	inp_count = 0;
	out_count = 0;
	inp_data = 0;
	out_data = 0;
	_index = 0;
	_phase = 0;
	_nread = 2 * _table->_hl;

	memset (_buff, 0, sizeof(float) * (_nread + 249));
	_nread -= _table->_hl - 1;
	return 0;
}

int
VMResampler::process (void)
{
	unsigned int   in, nr, n;
	double         ph, dp;
	float          a, *p1, *p2;

	if (!_table) return 1;

	const int hl = _table->_hl;
	const unsigned int np = _table->_np;
	in = _index;
	nr = _nread;
	ph = _phase;
	dp = _pstep;
	n = 2 * hl - nr;

#if 1
	/* optimized full-cycle no-resampling */
	if (dp == np && _qstep == np && nr == 1 && inp_count == out_count) {

		if (out_count >= n) {
			const unsigned int h1 = hl - 1;
			const unsigned int head = out_count - h1;
			const unsigned int tail = out_count - n;

			memcpy (out_data, &_buff[in + hl], h1 * sizeof (float));
			memcpy (&out_data[h1], inp_data, head * sizeof (float));
			memcpy (_buff, &inp_data[tail], n * sizeof (float));
			_index = 0;
			inp_count = 0;
			out_count = 0;
			return 0;
		}

		while (out_count) {
			unsigned int to_proc = std::min (out_count, _inmax - in);
			memcpy (&_buff[in + n], inp_data, to_proc * sizeof (float));
			memcpy (out_data, &_buff[in + hl], to_proc * sizeof (float));
			inp_data  += to_proc;
			out_data  += to_proc;
			out_count -= to_proc;
			in        += to_proc;
			if (in >= _inmax) {
				memcpy (_buff, _buff + in, (2 * hl - 1) * sizeof (float));
				in = 0;
			}
		}
		inp_count = out_count;
		_index = in;
		return 0;
	}
#endif

	p1 = _buff + in;
	p2 = p1 + n;

	while (out_count) {
		if (nr) {
			if (inp_count == 0) break;
			*p2 = *inp_data;
			inp_data++;
			nr--;
			p2++;
			inp_count--;
		} else {
			if (dp == np) {
				const unsigned int k = (unsigned int) /*floor (ph / np) +*/ hl;
				*out_data++ = p1[k];
			} else {
				const unsigned int k = (unsigned int) ph;
				const float bb = (float)(ph - k);
				const float aa = 1.0f - bb;
				float const* cq1 = _table->_ctab + hl * k;
				float const* cq2 = _table->_ctab + hl * (np - k);
				for (int i = 0; i < hl; i++) {
					_c1 [i] = aa * cq1 [i] + bb * cq1 [i + hl];
					_c2 [i] = aa * cq2 [i] + bb * cq2 [i - hl];
				}

				a = 1e-25f;
				for (int i = 0; i < hl; i++) {
					a += p1[i] * _c1 [i] + p2[-i-1] * _c2 [i];
				}
				*out_data++ = a - 1e-25f;
			}
			out_count--;

			const double dd = _qstep - dp;
			if (fabs (dd) < 1e-12) {
				dp = _qstep;
			} else {
				dp += _wstep * dd;
			}
			ph += dp;

			if (ph >= np) {
				nr = (unsigned int) floor (ph / np);
				ph -= nr * np;
				in += nr;
				p1 += nr;
				if (in >= _inmax) {
					n = (2 * hl - nr);
					memcpy (_buff, p1, n * sizeof (float));
					in = 0;
					p1 = _buff;
					p2 = p1 + n;
				}
			}
		}
	}
	_index = in;
	_nread = nr;
	_phase = ph;
	_pstep = dp;

	return 0;
}
