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
#include "ardour/midi_buffer.h"

using namespace ARDOUR;


samplecnt_t
LinearInterpolation::interpolate (int channel, samplecnt_t nframes, Sample *input, Sample *output)
{
	// index in the input buffers
	samplecnt_t i = 0;

	double acceleration = 0;

	if (_speed != _target_speed) {
		acceleration = _target_speed - _speed;
	}

	for (samplecnt_t outsample = 0; outsample < nframes; ++outsample) {
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

samplecnt_t
CubicInterpolation::interpolate (int channel, samplecnt_t nframes, Sample *input, Sample *output)
{
	// index in the input buffers
	samplecnt_t i = 0;

	double acceleration;
	double distance = phase[channel];

	if (_speed != _target_speed) {
		acceleration = _target_speed - _speed;
	} else {
		acceleration = 0.0;
	}

	if (nframes < 3) {
		/* no interpolation possible */

		if (input && output) {
			for (i = 0; i < nframes; ++i) {
				output[i] = input[i];
			}
		}

		phase[channel] = 0;
		return nframes;
	}

	/* keep this condition out of the inner loop */

	if (input && output) {
		/* best guess for the fake point we have to add to be able to interpolate at i == 0:
		 * .... maintain slope of first actual segment ...
		 */
		Sample inm1 = input[i] - (input[i+1] - input[i]);

		for (samplecnt_t outsample = 0; outsample < nframes; ++outsample) {
			/* get the index into the input we should start with */
			i = floor (distance);
			float fractional_phase_part = fmod (distance, 1.0);

			// Cubically interpolate into the output buffer: keep this inlined for speed and rely on compiler
			// optimization to take care of the rest
			// shamelessly ripped from Steve Harris' swh-plugins (ladspa-util.h)

			output[outsample] = input[i] + 0.5f * fractional_phase_part * (input[i+1] - inm1 +
					fractional_phase_part * (4.0f * input[i+1] + 2.0f * inm1 - 5.0f * input[i] - input[i+2] +
						fractional_phase_part * (3.0f * (input[i] - input[i+1]) - inm1 + input[i+2])));

			distance += _speed + acceleration;
			inm1 = input[i];
		}

		i = floor (distance);
		phase[channel] = fmod (distance, 1.0);

	} else {
		/* used to calculate play-distance with acceleration (silent roll)
		 * (use same algorithm as real playback for identical rounding/floor'ing)
		 */
		for (samplecnt_t outsample = 0; outsample < nframes; ++outsample) {
			distance += _speed + acceleration;
		}
		i = floor (distance);
		phase[channel] = fmod (distance, 1.0);
	}

	return i;
}

/* CubicMidiInterpolation::distance is identical to
 * return CubicInterpolation::interpolate (0, nframes, NULL, NULL);
 */
samplecnt_t
CubicMidiInterpolation::distance (samplecnt_t nframes, bool /*roll*/)
{
	assert (phase.size () == 1);

	samplecnt_t i = 0;

	double acceleration;
	double distance = phase[0];

	if (nframes < 3) {
		/* no interpolation possible */
		phase[0] = 0;
		return nframes;
	}

	if (_speed != _target_speed) {
		acceleration = _target_speed - _speed;
	} else {
		acceleration = 0.0;
	}

	for (samplecnt_t outsample = 0; outsample < nframes; ++outsample) {
		distance += _speed + acceleration;
	}

	i = floor (distance);
	phase[0] = fmod (distance, 1.0);

	return i;
}
