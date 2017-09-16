// ----------------------------------------------------------------------------
//
//  Copyright (C) 2006-2012 Fons Adriaensen <fons@linuxaudio.org>
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

#include "zita-resampler/resampler.h"

using namespace ArdourZita;

static unsigned int gcd (unsigned int a, unsigned int b)
{
	if (a == 0) return b;
	if (b == 0) return a;
	while (1) {
		if (a > b) {
			a = a % b;
			if (a == 0) return b;
			if (a == 1) return 1;
		} else {
			b = b % a;
			if (b == 0) return a;
			if (b == 1) return 1;
		}
	}
	return 1;
}

Resampler::Resampler (void)
	: _table (0)
	, _nchan (0)
	, _buff  (0)
{
	reset ();
}

Resampler::~Resampler (void)
{
	clear ();
}

int
Resampler::setup (unsigned int fs_inp,
                  unsigned int fs_out,
                  unsigned int nchan,
                  unsigned int hlen)
{
	if ((hlen < 8) || (hlen > 96)) return 1;
	return setup (fs_inp, fs_out, nchan, hlen, 1.0 - 2.6 / hlen);
}

int
Resampler::setup (unsigned int fs_inp,
                  unsigned int fs_out,
                  unsigned int nchan,
                  unsigned int hlen,
                  double       frel)
{
	unsigned int       g, h, k, n, s;
	double             r;
	float              *B = 0;
	Resampler_table    *T = 0;

	k = s = 0;
	if (fs_inp && fs_out && nchan) {
		r = (double) fs_out / (double) fs_inp;
		g = gcd (fs_out, fs_inp);
		n = fs_out / g;
		s = fs_inp / g;
		if ((16 * r >= 1) && (n <= 1000)) {
			h = hlen;
			k = 250;
			if (r < 1) {
				frel *= r;
				h = (unsigned int)(ceil (h / r));
				k = (unsigned int)(ceil (k / r));
			}
			T = Resampler_table::create (frel, h, n);
			B = new float [nchan * (2 * h - 1 + k)];
		}
	}
	clear ();
	if (T) {
		_table = T;
		_buff  = B;
		_nchan = nchan;
		_inmax = k;
		_pstep = s;
		return reset ();
	}
	else return 1;
}

void
Resampler::clear (void)
{
	Resampler_table::destroy (_table);
	delete[] _buff;
	_buff  = 0;
	_table = 0;
	_nchan = 0;
	_inmax = 0;
	_pstep = 0;
	reset ();
}

double
Resampler::inpdist (void) const
{
	if (!_table) return 0;
	return (int)(_table->_hl + 1 - _nread) - (double)_phase / _table->_np;
}

int
Resampler::inpsize (void) const
{
	if (!_table) return 0;
	return 2 * _table->_hl;
}

int
Resampler::reset (void)
{
	if (!_table) return 1;

	inp_count = 0;
	out_count = 0;
	inp_data = 0;
	out_data = 0;
	_index = 0;
	_nread = 0;
	_nzero = 0;
	_phase = 0;
	if (_table) {
		_nread = 2 * _table->_hl;
		return 0;
	}
	return 1;
}

int
Resampler::process (void)
{
	unsigned int   hl, ph, np, dp, in, nr, nz, i, n, c;
	float          *p1, *p2;

	if (!_table) return 1;

	hl = _table->_hl;
	np = _table->_np;
	dp = _pstep;
	in = _index;
	nr = _nread;
	ph = _phase;
	nz = _nzero;
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
					float *c1 = _table->_ctab + hl * ph;
					float *c2 = _table->_ctab + hl * (np - ph);
					for (c = 0; c < _nchan; c++) {
						float *q1 = p1 + c;
						float *q2 = p2 + c;
						float s = 1e-20f;
						for (i = 0; i < hl; i++) {
							q2 -= _nchan;
							s += *q1 * c1 [i] + *q2 * c2 [i];
							q1 += _nchan;
						}
						*out_data++ = s - 1e-20f;
					}
				} else {
					for (c = 0; c < _nchan; c++) *out_data++ = 0;
				}
			}
			out_count--;

			ph += dp;
			if (ph >= np) {
				nr = ph / np;
				ph -= nr * np;
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
	_nzero = nz;

	return 0;
}
