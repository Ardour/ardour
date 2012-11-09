/*
    Copyright (C) 2003-2012 Fons Adriaensen <fons@kokkinizita.net>

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

MTDM::MTDM (int fsamp) 
  : _cnt (0)
  , _inv (0)
{
    int   i;
    Freq  *F;

    _freq [0].f  = 4096;
    _freq [1].f  = 2048;
    _freq [2].f  = 3072;
    _freq [3].f  = 2560;
    _freq [4].f  = 2304;
    _freq [5].f  = 2176; 
    _freq [6].f  = 1088;
    _freq [7].f  = 1312;
    _freq [8].f  = 1552;
    _freq [9].f  = 1800;
    _freq [10].f = 3332;
    _freq [11].f = 3586;
    _freq [12].f = 3841;
    _wlp = 200.0f / fsamp;
    for (i = 0, F = _freq; i < 13; i++, F++)
    {
	F->p = 128;
	F->xa = F->ya = 0.0f;
	F->x1 = F->y1 = 0.0f;
	F->x2 = F->y2 = 0.0f;
    }
}


int MTDM::process (size_t len, float *ip, float *op)
{
    int    i;
    float  vip, vop, a, c, s;
    Freq   *F;

    while (len--)
    {
        vop = 0.0f;
	vip = *ip++;
	for (i = 0, F = _freq; i < 13; i++, F++)
	{
	    a = 2 * (float) M_PI * (F->p & 65535) / 65536.0; 
	    F->p += F->f;
	    c =  cosf (a); 
	    s = -sinf (a); 
	    vop += (i ? 0.01f : 0.20f) * s;
	    F->xa += s * vip;
	    F->ya += c * vip;
	} 
	*op++ = vop;
	if (++_cnt == 16)
	{
	    for (i = 0, F = _freq; i < 13; i++, F++)
	    {
		F->x1 += _wlp * (F->xa - F->x1 + 1e-20);
		F->y1 += _wlp * (F->ya - F->y1 + 1e-20);
		F->x2 += _wlp * (F->x1 - F->x2 + 1e-20);
		F->y2 += _wlp * (F->y1 - F->y2 + 1e-20);
		F->xa = F->ya = 0.0f;
	    }
            _cnt = 0;
	}
    }

    return 0;
}


int MTDM::resolve (void)
{
    int     i, k, m;
    double  d, e, f0, p;
    Freq    *F = _freq;

    if (hypot (F->x2, F->y2) < 0.001) return -1;
    d = atan2 (F->y2, F->x2) / (2 * M_PI);
    if (_inv) d += 0.5;
    if (d > 0.5) d -= 1.0;
    f0 = _freq [0].f;
    m = 1;
    _err = 0.0;
    for (i = 0; i < 12; i++)
    {
	F++;
	p = atan2 (F->y2, F->x2) / (2 * M_PI) - d * F->f / f0;
        if (_inv) p += 0.5;
	p -= floor (p);
	p *= 2;
	k = (int)(floor (p + 0.5));
	e = fabs (p - k);
        if (e > _err) _err = e;
        if (e > 0.4) return 1; 
	d += m * (k & 1);
	m *= 2;
    }  
    _del = 16 * d;

    return 0;
}


