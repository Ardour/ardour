/*
    Copyright (C) 2003-2008 Fons Adriaensen <fons@kokkinizita.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <math.h>

#include "ardour/mtdm.h"

MTDM::MTDM ()
	: _cnt (0)
	, _inv (0)
{
	int   i;
	Freq  *F;

	_freq [0].f = 4096;
	_freq [1].f =  512;
	_freq [2].f = 1088;
	_freq [3].f = 1544;
	_freq [4].f = 2049;

	_freq [0].a = 0.2f;
	_freq [1].a = 0.1f;
	_freq [2].a = 0.1f;
	_freq [3].a = 0.1f;
	_freq [4].a = 0.1f;

	for (i = 0, F = _freq; i < 5; i++, F++) {
		F->p = 128;
		F->xa = F->ya = 0.0f;
		F->xf = F->yf = 0.0f;
	}
}

int
MTDM::process (size_t len, float *ip, float *op)
{
	int    i;
	float  vip, vop, a, c, s;
	Freq   *F;

	while (len--)
	{
		vop = 0.0f;
		vip = *ip++;
		for (i = 0, F = _freq; i < 5; i++, F++)
		{
			a = 2 * (float) M_PI * (F->p & 65535) / 65536.0;
			F->p += F->f;
			c =  cosf (a);
			s = -sinf (a);
			vop += F->a * s;
			F->xa += s * vip;
			F->ya += c * vip;
		}
		*op++ = vop;
		if (++_cnt == 16)
		{
			for (i = 0, F = _freq; i < 5; i++, F++)
			{
				F->xf += 1e-3f * (F->xa - F->xf + 1e-20);
				F->yf += 1e-3f * (F->ya - F->yf + 1e-20);
				F->xa = F->ya = 0.0f;
			}
			_cnt = 0;
		}
	}

	return 0;
}

int
MTDM::resolve ()
{
	int     i, k, m;
	double  d, e, f0, p;
	Freq    *F = _freq;

	if (hypot (F->xf, F->yf) < 0.01) {
		return -1;
	}

	d = atan2 (F->yf, F->xf) / (2 * M_PI);

	if (_inv) {
		d += 0.5f;
	}

	if (d > 0.5f) {
		d -= 1.0f;
	}

	f0 = _freq [0].f;
	m = 1;
	_err = 0.0;

	for (i = 0; i < 4; i++) {
		F++;
		p = atan2 (F->yf, F->xf) / (2 * M_PI) - d * F->f / f0;
		if (_inv) {
			p += 0.5f;
		}
		p -= floor (p);
		p *= 8;
		k = (int)(floor (p + 0.5));
		e = fabs (p - k);
		if (e > _err) {
			_err = e;
		}
		if (e > 0.4) {
			return 1;
		}
		d += m * (k & 7);
		m *= 8;
	}

	_del = 16 * d;

	return 0;
}
