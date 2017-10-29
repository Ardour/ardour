// ----------------------------------------------------------------------------
//
//  Copyright (C) 2006-2013 Fons Adriaensen <fons@linuxaudio.org>
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

#include "zita-resampler/vresampler.h"

using namespace ArdourZita;

VResampler::VResampler (void)
	: _table (0)
  , _nchan (0)
  , _buff  (0)
  , _c1 (0)
  , _c2 (0)
{
	reset ();
}

VResampler::~VResampler (void)
{
	clear ();
}

int
VResampler::setup (double       ratio,
                   unsigned int nchan,
                   unsigned int hlen)
{
	if ((hlen < 8) || (hlen > 96) || (16 * ratio < 1) || (ratio > 256)) return 1;
	return setup (ratio, nchan, hlen, 1.0 - 2.6 / hlen);
}

int
VResampler::setup (double       ratio,
                   unsigned int nchan,
                   unsigned int hlen,
                   double       frel)
{
	unsigned int       h, k, n;
	double             s;
	Resampler_table    *T = 0;

	if (! nchan) return 1;
	n = NPHASE;
	s = n / ratio;
	h = hlen;
	k = 250;
	if (ratio < 1) {
		frel *= ratio;
		h = (unsigned int)(ceil (h / ratio));
		k = (unsigned int)(ceil (k / ratio));
	}
	T = Resampler_table::create (frel, h, n);
	clear ();
	if (T) {
		_table = T;
		_buff  = new float [nchan * (2 * h - 1 + k)];
		_c1 = new float [2 * h];
		_c2 = new float [2 * h];
		_nchan = nchan;
		_inmax = k;
		_ratio = ratio;
		_pstep = s;
		_qstep = s;
		_wstep = 1;
		return reset ();
	}
	else return 1;
}

void
VResampler::clear (void)
{
	Resampler_table::destroy (_table);
	delete[] _buff;
	delete[] _c1;
	delete[] _c2;
	_buff  = 0;
	_c1 = 0;
	_c2 = 0;
	_table = 0;
	_nchan = 0;
	_inmax = 0;
	_pstep = 0;
	_qstep = 0;
	_wstep = 1;
	reset ();
}

void
VResampler::set_phase (double p)
{
	if (!_table) return;
	_phase = (p - floor (p)) * _table->_np;
}

void
VResampler::set_rrfilt (double t)
{
	if (!_table) return;
	_wstep =  (t < 1) ? 1 : 1 - exp (-1 / t);
}

void
VResampler::set_rratio (double r)
{
	if (!_table) return;
	if (r > 16.0) r = 16.0;
	if (r < 0.95) r = 0.95;
	_qstep = _table->_np / (_ratio * r);
}

double
VResampler::inpdist (void) const
{
	if (!_table) return 0;
	return (int)(_table->_hl + 1 - _nread) - _phase / _table->_np;
}

int
VResampler::inpsize (void) const
{
	if (!_table) return 0;
	return 2 * _table->_hl;
}

int
VResampler::reset (void)
{
	if (!_table) return 1;

	inp_count = 0;
	out_count = 0;
	inp_data = 0;
	out_data = 0;
	_index = 0;
	_phase = 0;
	_nread = 2 * _table->_hl;
	_nzero = 0;
	return 0;
}

int
VResampler::process (void)
{
	unsigned int   k, np, in, nr, n, c;
	int            i, hl, nz;
	double         ph, dp, dd;
	float          a, b, *p1, *p2, *q1, *q2;

	if (!_table) return 1;

	hl = _table->_hl;
	np = _table->_np;
	in = _index;
	nr = _nread;
	nz = _nzero;
	ph = _phase;
	dp = _pstep;
	n = (2 * hl - nr) * _nchan;
	p1 = _buff + in * _nchan;
	p2 = p1 + n;

	while (out_count) {
		if (nr) {
			if (inp_count == 0) break;
			if (inp_data) {
				for (c = 0; c < _nchan; c++) p2 [c] = inp_data [c];
				inp_data += _nchan;
				nz = 0;
			} else {
				for (c = 0; c < _nchan; c++) p2 [c] = 0;
				if (nz < 2 * hl) nz++;
			}
			nr--;
			p2 += _nchan;
			inp_count--;
		} else {
			if (out_data) {
				if (nz < 2 * hl) {
					k = (unsigned int) ph;
					b = (float)(ph - k);
					a = 1.0f - b;
					q1 = _table->_ctab + hl * k;
					q2 = _table->_ctab + hl * (np - k);
					for (i = 0; i < hl; i++) {
						_c1 [i] = a * q1 [i] + b * q1 [i + hl];
						_c2 [i] = a * q2 [i] + b * q2 [i - hl];
					}
					for (c = 0; c < _nchan; c++) {
						q1 = p1 + c;
						q2 = p2 + c;
						a = 1e-25f;
						for (i = 0; i < hl; i++) {
							q2 -= _nchan;
							a += *q1 * _c1 [i] + *q2 * _c2 [i];
							q1 += _nchan;
						}
						*out_data++ = a - 1e-25f;
					}
				} else {
					for (c = 0; c < _nchan; c++) *out_data++ = 0;
				}
			}
			out_count--;

			dd =  _qstep - dp;
			if (fabs (dd) < 1e-30) dp = _qstep;
			else dp += _wstep * dd;
			ph += dp;
			if (ph >= np) {
				nr = (unsigned int) floor( ph / np);
				ph -= nr * np;;
				in += nr;
				p1 += nr * _nchan;;
				if (in >= _inmax) {
					n = (2 * hl - nr) * _nchan;
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
	_nzero = nz;

	return 0;
}
