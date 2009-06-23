#include <stdint.h>

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
	uint64_t	phase = last_phase[channel];
	
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
		i = phase >> 24;
		Sample fractional_phase_part = (phase & fractional_part_mask) / binary_scaling_factor;
		
		if (input && output) {
			// Linearly interpolate into the output buffer
			// using fixed point math
			output[outsample] = 
				input[i] * (1.0f - fractional_phase_part) +
				input[i+1] * fractional_phase_part;
		}
		
		phase += phi + phi_delta;
	}

	last_phase[channel] = (phase & fractional_part_mask);
	
	// playback distance
	return i;
}

void 
FixedPointLinearInterpolation::add_channel_to (int input_buffer_size, int output_buffer_size)
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
	for(int i = 0; i <= last_phase.size(); i++) {
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
	
	printf("phase before: %lf\n", phase[channel]);
	distance = phase[channel];
	for (nframes_t outsample = 0; outsample < nframes; ++outsample) {
		i = distance;
		Sample fractional_phase_part = distance - i;
		if (fractional_phase_part >= 1.0) {
			fractional_phase_part -= 1.0;
			i++;
		}
		//printf("I: %u, distance: %lf, fractional_phase_part: %lf\n", i, distance, fractional_phase_part);
		
		if (input && output) {
		// Linearly interpolate into the output buffer
			output[outsample] = 
				input[i] * (1.0f - fractional_phase_part) +
				input[i+1] * fractional_phase_part;
		}
		//printf("distance before: %lf\n", distance);
		distance += _speed + acceleration;
		//printf("distance after: %lf, _speed: %lf\n", distance, _speed);
	}
	
	printf("before assignment: i: %d, distance: %lf\n", i, distance);
	i = floor(distance);
	printf("after assignment: i: %d, distance: %16lf\n", i, distance);
	phase[channel] = distance - floor(distance);
	printf("speed: %16lf, i after: %d, distance after: %16lf, phase after: %16lf\n", _speed, i, distance, phase[channel]);
	
	return i;
}

void 
LinearInterpolation::add_channel_to (int input_buffer_size, int output_buffer_size)
{
	phase.push_back (0.0);
}

void 
LinearInterpolation::remove_channel_from ()
{
	phase.pop_back ();
}


void
LinearInterpolation::reset() 
{
	for(int i = 0; i <= phase.size(); i++) {
		phase[i] = 0.0;
	}
}

LibSamplerateInterpolation::LibSamplerateInterpolation() : state (0)
{
	_speed = 1.0;
}

LibSamplerateInterpolation::~LibSamplerateInterpolation() 
{
	for (int i = 0; i < state.size(); i++) {
		state[i] = src_delete (state[i]);
	}
}

void
LibSamplerateInterpolation::set_speed (double new_speed)
{ 
	_speed = new_speed; 
	for (int i = 0; i < state.size(); i++) {
		src_set_ratio (state[i], 1.0/_speed);
	}
}

void
LibSamplerateInterpolation::reset_state ()
{
	printf("INTERPOLATION: reset_state()\n");
	for (int i = 0; i < state.size(); i++) {
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
	delete data.back ();
	data.pop_back ();
	delete state.back ();
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
