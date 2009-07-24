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
    // precompute LU-factorization of matrix A
    // see "Teubner Taschenbuch der Mathematik", p. 1105
    // We only need to calculate up to 20, because they
    // won't change any more above that
    _m[0] = 4.0;
    for (int i = 0; i <= 20 - 2; i++) {
        _l[i] = 1.0 / _m[i];
        _m[i+1] = 4.0 - _l[i];
    }
}

nframes_t
SplineInterpolation::interpolate (int channel, nframes_t nframes, Sample *input, Sample *output)
{
    // How many input samples we need
    nframes_t n = ceil (double(nframes) * _speed + phase[channel]);
    
    // hans - we run on 64bit systems too .... no casting pointer to a sized integer, please
    printf("======== n: %u nframes: %u input: %p, output: %p\n", n, nframes, input, output);
    
    if (n <= 3) {
        return 0;
    }
    
    double M[n], t[n-2];
    
    // natural spline: boundary conditions
    M[0]     = 0.0;
    M[n - 1] = 0.0;
    
    if (input) {
        // solve L * t = d
        t[0] = 6.0 * (input[0] - 2*input[1] + input[2]); 
        for (nframes_t i = 1; i <= n - 3; i++) {
            t[i] = 6.0 * (input[i] - 2*input[i+1] + input[i+2])
                   - l(i-1) * t[i-1];
        }
        
        // solve U * M = t
        M[n-2] = t[n-3] / m(n-3);
        //printf(" M[%d] = %lf \n", n-1 ,M[n-1]);
        //printf(" M[%d] = %lf \n", n-2 ,M[n-2]);
        for (nframes_t i = n-4;; i--) {
            M[i+1] = (t[i]-M[i+2])/m(i);
            //printf(" M[%d] = %lf\n", i+1 ,M[i+1]);
            if ( i == 0 ) break;
        }
        M[1]     = 0.0;
        M[n - 2] = 0.0;
        //printf(" M[%d] = %lf \n", 0 ,M[0]);
    }
    
    assert (M[0] == 0.0 && M[n-1] == 0.0);
    
    // now interpolate
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
            assert (i <= n-1);
            double a3 = (M[i+1] - M[i]) / 6.0;
            double a2 = M[i] / 2.0;
            double a1 = input[i+1] - input[i] - (M[i+1] + 2.0*M[i])/6.0;
            double a0 = input[i];
            // interpolate into the output buffer
            output[outsample] = ((a3*x + a2)*x + a1)*x + a0;
            //std::cout << "input[" << i << "/" << i+1 << "] = " << input[i] << "/" << input[i+1] <<  " distance: " << distance << " output[" << outsample << "] = " << output[outsample] << std::endl;
        }
        distance += _speed + acceleration;
    }
    
    i = floor(distance);
    phase[channel] = distance - floor(distance);
    assert (phase[channel] >= 0.0 && phase[channel] < 1.0);
    printf("Moved input frames: %u ", i);
    
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
