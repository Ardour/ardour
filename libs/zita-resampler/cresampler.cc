// ----------------------------------------------------------------------------
//
//  Copyright (C) 2013 Fons Adriaensen <fons@linuxaudio.org>
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

#include "zita-resampler/cresampler.h"

using namespace ArdourZita;

CResampler::CResampler (void)
	: _nchan (0)
	, _buff  (0)
{
	reset ();
}

CResampler::~CResampler (void)
{
	clear ();
}

int
CResampler::setup (double       ratio,
                   unsigned int nchan)
{
	if (! nchan) return 1;
	clear ();
	_inmax = 50;
	_buff = new float [nchan * (3 + _inmax)];
	_nchan = nchan;
	_pstep = 1 / ratio;
	return reset ();
}

void
CResampler::clear (void)
{
	delete[] _buff;
	_buff  = 0;
	_nchan = 0;
	_inmax = 0;
	_pstep = 0;
	reset ();
}

void
CResampler::set_phase (double p)
{
	_phase = p - floor (p);
}

void
CResampler::set_ratio (double r)
{
	_pstep = 1.0 / r;
}

double
CResampler::inpdist (void) const
{
	return (int)(3 - _nread) - _phase;
}

int
CResampler::inpsize (void) const
{
	return 4;
}

int
CResampler::reset (void)
{
	inp_count = 0;
	out_count = 0;
	inp_data = 0;
	out_data = 0;
	_index = 0;
	_phase = 0;
	_nread = 4;
	_nzero = 0;
	return 0;
}

int
CResampler::process (void)
{
	unsigned int   in, nr, n, c;
	int            nz;
	double         ph;
	float          *pb, a, b, d, m0, m1, m2, m3;

	in = _index;
	nr = _nread;
	nz = _nzero;
	ph = _phase;
	pb = _buff + in * _nchan;

	while (out_count) {
		if (nr) {
			if (inp_count == 0) break;
			n = (4 - nr) * _nchan;
			if (inp_data) {
				for (c = 0; c < _nchan; c++) pb [n + c] = inp_data [c];
				inp_data += _nchan;
				nz = 0;
			} else {
				for (c = 0; c < _nchan; c++) pb [n + c] = 0;
				if (nz < 4) nz++;
			}
			nr--;
			inp_count--;
		} else {
			n = _nchan;
			if (out_data) {
				if (nz < 4) {
					a = ph;
					b = 1 - a;
					d = a * b / 2;
					m0 = -d * b;
					m1 = b + (3 * b - 1) * d;
					m2 = a + (3 * a - 1) * d;
					m3 = -d * a;
					for (c = 0; c < n; c++) {
						*out_data++ = m0 * pb [0]
							+ m1 * pb [n]
							+ m2 * pb [2 * n]
							+ m3 * pb [3 * n];
						pb++;
					}
					pb -= n;
				} else {
					for (c = 0; c < n; c++) *out_data++ = 0;
				}
			}
			out_count--;

			ph += _pstep;
			if (ph >= 1.0) {
				nr = (unsigned int) floor (ph);
				ph -= nr;
				in += nr;
				pb += nr * _nchan;
				if (in >= _inmax) {
					memcpy (_buff, pb, (4 - nr) * _nchan * sizeof (float));
					in = 0;
					pb = _buff;
				}
			}
		}
	}

	_index = in;
	_nread = nr;
	_nzero = nz;
	_phase = ph;

	return 0;
}
