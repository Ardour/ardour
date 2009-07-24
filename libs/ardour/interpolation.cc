#include <stdint.h>
#include <cstdio>

#include "ardour/interpolation.h"

using namespace ARDOUR;

nframes_t
FixedPointLinearInterpolation::interpolate (int channel, nframes_t nframes, Sample *input, Sample *output)
{
	// the idea behind phase is that when the speed is not 1.0, we have to 
	// interpolate between samples and then we have to store where we thought we were. 
	// rather than being at sample N or N+1, we were at N+0.8792922
	// so the "phase" element, if you want to think about this way, 
	// varies from 0 to 1, representing the "offset" between samples
	uint64_t	the_phase = last_phase[channel];
	
	// acceleration
	int64_t	 phi_delta;

	// phi = fixed point speed
	if (phi != target_phi) {
		phi_delta = ((int64_t)(target_phi - phi)) / nframes;
	} else {
		phi_delta = 0;
	}
	
	// index in the input buffers
	nframes_t   i = 0;

	for (nframes_t outsample = 0; outsample < nframes; ++outsample) {
		i = the_phase >> 24;
		Sample fractional_phase_part = (the_phase & fractional_part_mask) / binary_scaling_factor;
		
		if (input && output) {
			// Linearly interpolate into the output buffer
			output[outsample] = 
				input[i] * (1.0f - fractional_phase_part) +
				input[i+1] * fractional_phase_part;
		}
		
		the_phase += phi + phi_delta;
	}

	last_phase[channel] = (the_phase & fractional_part_mask);
	
	// playback distance
	return i;
}

void 
FixedPointLinearInterpolation::add_channel_to (int /*input_buffer_size*/, int /*output_buffer_size*/)
{
	last_phase.push_back (0);
}

void 
FixedPointLinearInterpolation::remove_channel_from ()
{
	last_phase.pop_back ();
}

void
FixedPointLinearInterpolation::reset() 
{
	for (size_t i = 0; i <= last_phase.size(); i++) {
		last_phase[i] = 0;
	}
}


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

SplineInterpolation::SplineInterpolation()
{
    reset ();
}

void SplineInterpolation::reset()
{
    Interpolation::reset();
    M[0] = 0.0;
    M[1] = 0.0;
    M[2] = 0.0;
}

nframes_t
SplineInterpolation::interpolate (int channel, nframes_t nframes, Sample *input, Sample *output)
{
    
    // now interpolate
    // index in the input buffers
    nframes_t   i = 0, delta_i = 0;
    
    double acceleration;
    double distance = 0.0;
    
    if (_speed != _target_speed) {
        acceleration = _target_speed - _speed;
    } else {
        acceleration = 0.0;
    }
    
    distance = phase[channel];
    assert(distance >= 0.0 && distance < 1.0);
    
    for (nframes_t outsample = 0; outsample < nframes; outsample++) {
        i = floor(distance);
        
        double x = double(distance) - double(i);
        
        // if distance is something like 0.999999999999
        // it will get rounded to 1 in the conversion to float above
        while (x >= 1.0) {
            x -= 1.0;
            i++;
        } 
        
        assert(x >= 0.0 && x < 1.0);
        
        if (input && output) {
            // if i changed, recalculate coefficients
            if (delta_i == 1) {
                // if i changed, rotate the M's
                M[0] = M[1];
                M[1] = M[2];
                M[2] = 6.0 * (input[i] - 2.0*input[i+1] + input[i+2]) - 4.0*M[1] - M[0];
                printf("\ny[%d] = %lf\n", i, input[i]);
                printf("y[%d] = %lf\n", i+1, input[i+1]);
                printf("y[%d] = %lf\n\n", i+2, input[i+2]);
                printf("M[2] = %lf  M[1] = %lf  M[0] = %lf y-term: %lf M-term: %lf\n", 
                        M[2], M[1], M[0],  6.0 * (input[i] - 2.0*input[i+1] + input[i+2]),
                        - 4.0*M[1] - M[0]);
            }
            double a3 = (M[1] - M[0]) / 6.0;
            double a2 = M[0] / 2.0;
            double a1 = input[i+1] - input[i] - (M[1] + 2.0*M[0]) / 6.0;
            double a0 = input[i];
            // interpolate into the output buffer
            output[outsample] = ((a3*x + a2)*x + a1)*x + a0;
            //printf( "input[%d/%d] = %lf/%lf  distance: %lf output[%d] = %lf\n", i, i+1, input[i], input[i+1], distance, outsample, output[outsample]);
            
        }
        distance += _speed + acceleration;

        delta_i = floor(distance) - i;
    }
    
    i = floor(distance);
    phase[channel] = distance - floor(distance);
    assert (phase[channel] >= 0.0 && phase[channel] < 1.0);
    
    return i;
}

LibSamplerateInterpolation::LibSamplerateInterpolation() : state (0)
{
	_speed = 1.0;
}

LibSamplerateInterpolation::~LibSamplerateInterpolation() 
{
	for (size_t i = 0; i < state.size(); i++) {
		state[i] = src_delete (state[i]);
	}
}

void
LibSamplerateInterpolation::set_speed (double new_speed)
{ 
	_speed = new_speed; 
	for (size_t i = 0; i < state.size(); i++) {
		src_set_ratio (state[i], 1.0/_speed);
	}
}

void
LibSamplerateInterpolation::reset_state ()
{
	printf("INTERPOLATION: reset_state()\n");
	for (size_t i = 0; i < state.size(); i++) {
		if (state[i]) {
			src_reset (state[i]);
		} else {
			state[i] = src_new (SRC_SINC_FASTEST, 1, &error);
		}
	}
}

void
LibSamplerateInterpolation::add_channel_to (int input_buffer_size, int output_buffer_size) 
{
	SRC_DATA* newdata = new SRC_DATA;
	
	/* Set up sample rate converter info. */
	newdata->end_of_input = 0 ; 

	newdata->input_frames  = input_buffer_size;
	newdata->output_frames = output_buffer_size;

	newdata->input_frames_used = 0 ;
	newdata->output_frames_gen = 0 ;

	newdata->src_ratio = 1.0/_speed;
	
	data.push_back (newdata);
	state.push_back (0);
	
	reset_state ();
}

void
LibSamplerateInterpolation::remove_channel_from () 
{
	SRC_DATA* d = data.back ();
	delete d;
	data.pop_back ();
	if (state.back ()) {
		src_delete (state.back ());
	}
	state.pop_back ();
	reset_state ();
}

nframes_t
LibSamplerateInterpolation::interpolate (int channel, nframes_t nframes, Sample *input, Sample *output)
{	
	if (!data.size ()) {
		printf ("ERROR: trying to interpolate with no channels\n");
		return 0;
	}
	
	data[channel]->data_in	   = input;
	data[channel]->data_out	  = output;
	
	data[channel]->input_frames  = nframes * _speed;
	data[channel]->output_frames = nframes;
	data[channel]->src_ratio	 = 1.0/_speed; 

	if ((error = src_process (state[channel], data[channel]))) {	
		printf ("\nError : %s\n\n", src_strerror (error));
		exit (1);
	}
	
	//printf("INTERPOLATION: channel %d input_frames_used: %d\n", channel, data[channel]->input_frames_used);
	
	return data[channel]->input_frames_used;
}
