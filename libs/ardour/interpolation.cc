/*
    Copyright (C) 2012 Paul Davis 

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

#include <stdint.h>
#include <cstdio>

#include "ardour/interpolation.h"

using namespace ARDOUR;


framecnt_t
LinearInterpolation::interpolate (int channel, framecnt_t nframes, Sample *input, Sample *output)
{
	// index in the input buffers
	framecnt_t i = 0;

	double acceleration = 0;

	if (_speed != _target_speed) {
		acceleration = _target_speed - _speed;
	}

	for (framecnt_t outsample = 0; outsample < nframes; ++outsample) {
		double const d = phase[channel] + outsample * (_speed + acceleration);
		i = floor(d);
		Sample fractional_phase_part = d - i;
		if (fractional_phase_part >= 1.0) {
			fractional_phase_part -= 1.0;
			i++;
		}

		if (input && output) {
		// Linearly interpolate into the output buffer
			output[outsample] =
				input[i] * (1.0f - fractional_phase_part) +
				input[i+1] * fractional_phase_part;
		}
	}

	double const distance = phase[channel] + nframes * (_speed + acceleration);
	i = floor(distance);
	phase[channel] = distance - i;
	return i;
}

framecnt_t
CubicInterpolation::interpolate (int channel, framecnt_t nframes, Sample *input, Sample *output)
{
    // index in the input buffers
    framecnt_t   i = 0;

    double acceleration;
    double distance = 0.0;

    if (_speed != _target_speed) {
        acceleration = _target_speed - _speed;
    } else {
	    acceleration = 0.0;
    }

    distance = phase[channel];

    if (nframes < 3) {
	    /* no interpolation possible */

	    for (i = 0; i < nframes; ++i) {
		    output[i] = input[i];
	    }

	    return nframes;
    }

    /* keep this condition out of the inner loop */

    if (input && output) {

	    Sample inm1;

	    if (floor (distance) == 0.0) {
		    /* best guess for the fake point we have to add to be able to interpolate at i == 0:
		       .... maintain slope of first actual segment ...
		    */
		    inm1 = input[i] - (input[i+1] - input[i]);
	    } else {
		    inm1 = input[i-1];
	    }

	    for (framecnt_t outsample = 0; outsample < nframes; ++outsample) {

		    float f = floor (distance);
		    float fractional_phase_part = distance - f;

		    /* get the index into the input we should start with */

		    i = lrintf (f);

		    /* fractional_phase_part only reaches 1.0 thanks to float imprecision. In theory
		       it should always be < 1.0. If it ever >= 1.0, then bump the index we use
		       and back it off. This is the point where we "skip" an entire sample in the
		       input, because the phase part has accumulated so much error that we should
		       really be closer to the next sample. or something like that ...
		    */

		    if (fractional_phase_part >= 1.0) {
			    fractional_phase_part -= 1.0;
			    ++i;
		    }

		    // Cubically interpolate into the output buffer: keep this inlined for speed and rely on compiler
		    // optimization to take care of the rest
		    // shamelessly ripped from Steve Harris' swh-plugins (ladspa-util.h)

		    output[outsample] = input[i] + 0.5f * fractional_phase_part * (input[i+1] - inm1 +
							  fractional_phase_part * (4.0f * input[i+1] + 2.0f * inm1 - 5.0f * input[i] - input[i+2] +
								fractional_phase_part * (3.0f * (input[i] - input[i+1]) - inm1 + input[i+2])));

		    distance += _speed + acceleration;
		    inm1 = input[i];
	    }

	    i = floor(distance);
	    phase[channel] = distance - floor(distance);

    } else {
	    /* used to calculate play-distance with acceleration (silent roll)
	     * (use same algorithm as real playback for identical rounding/floor'ing)
	     */
	    for (framecnt_t outsample = 0; outsample < nframes; ++outsample) {
		    distance += _speed + acceleration;
	    }
	    i = floor(distance);
    }

    return i;
}
