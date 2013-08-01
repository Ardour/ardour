/*
    Copyright (C) 2012 Fons Adriaensen <fons@linuxaudio.org>
    Adopted for Ardour 2013 by Robin Gareus <robin@gareus.org>

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
#include "ardour/iec2ppmdsp.h"


float Iec2ppmdsp::_w1;
float Iec2ppmdsp::_w2;
float Iec2ppmdsp::_w3;
float Iec2ppmdsp::_g;


Iec2ppmdsp::Iec2ppmdsp (void) :
    _z1 (0),
    _z2 (0),
    _m (0),
    _res (true)
{
}


Iec2ppmdsp::~Iec2ppmdsp (void)
{
}


void Iec2ppmdsp::process (float *p, int n)
{
    float z1, z2, m, t;

    z1 = _z1;
    z2 = _z2;
    m = _res ? 0: _m;
    _res = false;

    n /= 4;
    while (n--)
    {
	z1 *= _w3;
	z2 *= _w3;
	t = fabsf (*p++);
	if (t > z1) z1 += _w1 * (t - z1);
	if (t > z2) z2 += _w2 * (t - z2);
	t = fabsf (*p++);
	if (t > z1) z1 += _w1 * (t - z1);
	if (t > z2) z2 += _w2 * (t - z2);
	t = fabsf (*p++);
	if (t > z1) z1 += _w1 * (t - z1);
	if (t > z2) z2 += _w2 * (t - z2);
	t = fabsf (*p++);
	if (t > z1) z1 += _w1 * (t - z1);
	if (t > z2) z2 += _w2 * (t - z2);
	t = z1 + z2;
	if (t > m) m = t;
    }

    _z1 = z1 + 1e-10f;
    _z2 = z2 + 1e-10f;
    _m = m;
}


float Iec2ppmdsp::read (void)
{
    _res = true;
    return _g * _m;
}

void Iec2ppmdsp::reset ()
{
    _z1 = _z2 = _m = .0f;
    _res = true;
}

void Iec2ppmdsp::init (float fsamp)
{
    _w1 = 200.0f / fsamp;
    _w2 = 860.0f / fsamp;
    _w3 = 1.0f - 4.0f / fsamp;
    _g = 0.5141f;
}

/* vi:set ts=8 sts=8 sw=4: */
