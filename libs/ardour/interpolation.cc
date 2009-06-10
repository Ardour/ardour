#include <stdint.h>
#include "ardour/interpolation.h"

nframes_t
LinearInterpolation::interpolate (nframes_t nframes, Sample *input, Sample *output)
{
	// the idea behind phase is that when the speed is not 1.0, we have to 
	// interpolate between samples and then we have to store where we thought we were. 
	// rather than being at sample N or N+1, we were at N+0.8792922
	// so the "phase" element, if you want to think about this way, 
	// varies from 0 to 1, representing the "offset" between samples
	uint64_t    phase = last_phase;
	
	// acceleration
	int64_t     phi_delta;

	// phi = fixed point speed
	if (phi != target_phi) {
		phi_delta = ((int64_t)(target_phi - phi)) / nframes;
	} else {
		phi_delta = 0;
	}
	
	// index in the input buffers
	nframes_t   i = 0;

	for (nframes_t outsample = 0; outsample < nframes; ++outsample) {
		i = phase >> 24;
		Sample fractional_phase_part = (phase & fractional_part_mask) / binary_scaling_factor;
		
        // Linearly interpolate into the output buffer
        // using fixed point math
		output[outsample] = 
			input[i] * (1.0f - fractional_phase_part) +
			input[i+1] * fractional_phase_part;
		phase += phi + phi_delta;
	}

	last_phase = (phase & fractional_part_mask);
	
	// playback distance
	return i;
}
