#include <stdint.h>
#include "ardour/interpolation.h"

using namespace ARDOUR;

nframes_t
LinearInterpolation::interpolate (nframes_t nframes, Sample *input, Sample *output)
{
	// the idea is that when the speed is not 1.0, we have to 
	// interpolate between samples and then we have to store where we thought we were. 
	// rather than being at sample N or N+1, we were at N+0.8792922
	
	// index in the input buffers
	nframes_t   i = 0;
	
	double acceleration;
	double distance = 0.0;
	
	if (_speed != _target_speed) {
		acceleration = _target_speed - _speed;
	} else {
		acceleration = 0.0;
	}

	for (nframes_t outsample = 0; outsample < nframes; ++outsample) {
		i = distance;
		Sample fractional_phase_part = distance - i;
		
		if (input && output) {
	        // Linearly interpolate into the output buffer
			output[outsample] = 
				input[i] * (1.0f - fractional_phase_part) +
				input[i+1] * fractional_phase_part;
		}
		distance   += _speed + acceleration;
	}
	
	i = (distance + 0.5L);
	// playback distance
	return i;
}
