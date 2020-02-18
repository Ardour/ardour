/*
 * Copyright (C) 2008-2011 Fons Adriaensen <fons@linuxaudio.org>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
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

#include <math.h>
#include "ardour/kmeterdsp.h"

float  Kmeterdsp::_omega;

Kmeterdsp::Kmeterdsp (void)
	: _z1 (0)
	, _z2 (0)
	, _rms (0)
	, _flag (false)
{}

Kmeterdsp::~Kmeterdsp (void) {}

void
Kmeterdsp::init (int fsamp)
{
	_omega = 9.72f / fsamp; // ballistic filter coefficient
}

void
Kmeterdsp::process (float const* p, int n)
{
	float  s, z1, z2;

	// Get filter state.
	z1 = _z1 > 50 ? 50 : (_z1 < 0 ? 0 : _z1);
	z2 = _z2 > 50 ? 50 : (_z2 < 0 ? 0 : _z2);

	// Perform filtering. The second filter is evaluated
	// only every 4th sample - this is just an optimisation.
	n /= 4;  // Loop is unrolled by 4.
	while (n--) {
		s = *p++;
		s *= s;
		z1 += _omega * (s - z1);      // Update first filter.
		s = *p++;
		s *= s;
		z1 += _omega * (s - z1);      // Update first filter.
		s = *p++;
		s *= s;
		z1 += _omega * (s - z1);      // Update first filter.
		s = *p++;
		s *= s;
		z1 += _omega * (s - z1);      // Update first filter.
		z2 += 4 * _omega * (z1 - z2); // Update second filter.
	}

	if (isnan(z1)) z1 = 0;
	if (isnan(z2)) z2 = 0;

	// Save filter state. The added constants avoid denormals.
	_z1 = z1 + 1e-20f;
	_z2 = z2 + 1e-20f;

	s = sqrtf (2.0f * z2);

	if (_flag) {
		// Display thread has read the rms value.
		_rms  = s;
		_flag = false;
	} else {
		// Adjust RMS value and update maximum since last read().
		if (s > _rms) _rms = s;
	}
}

/* Returns highest _rms value since last call */
float
Kmeterdsp::read ()
{
	float rv= _rms;
	_flag = true; // Resets _rms in next process().
	return rv;
}

void
Kmeterdsp::reset ()
{
	_z1 = _z2 = _rms = .0f;
	_flag = false;
}
