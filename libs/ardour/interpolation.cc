#include <stdint.h>
#include <cstdio>

#include "ardour/interpolation.h"

using namespace ARDOUR;


nframes_t
LinearInterpolation::interpolate (int channel, nframes_t nframes, Sample *input, Sample *output)
{
	// index in the input buffers
	nframes_t   i = 0;

	double acceleration;
	double distance = 0.0;

	if (_speed != _target_speed) {
		acceleration = _target_speed - _speed;
	} else {
		acceleration = 0.0;
	}

	distance = phase[channel];
	for (nframes_t outsample = 0; outsample < nframes; ++outsample) {
		i = floor(distance);
		Sample fractional_phase_part = distance - i;
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
		distance += _speed + acceleration;
	}

	i = floor(distance);
	phase[channel] = distance - floor(distance);

	return i;
}

nframes_t
CubicInterpolation::interpolate (int channel, nframes_t nframes, Sample *input, Sample *output)
{
    // index in the input buffers
    nframes_t   i = 0;

    double acceleration;
    double distance = 0.0;

    if (_speed != _target_speed) {
        acceleration = _target_speed - _speed;
    } else {
        acceleration = 0.0;
    }

    distance = phase[channel];
    for (nframes_t outsample = 0; outsample < nframes; ++outsample) {
        i = floor(distance);
        Sample fractional_phase_part = distance - i;
        if (fractional_phase_part >= 1.0) {
            fractional_phase_part -= 1.0;
            i++;
        }

        if (input && output) {
            // Cubically interpolate into the output buffer
            output[outsample] = cube_interp(fractional_phase_part, input[i-1], input[i], input[i+1], input[i+2]);
        }
        distance += _speed + acceleration;
    }

    i = floor(distance);
    phase[channel] = distance - floor(distance);

    return i;
}
