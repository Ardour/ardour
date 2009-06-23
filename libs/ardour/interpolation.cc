#include <stdint.h>

#include "ardour/interpolation.h"

using namespace ARDOUR;

LibSamplerateInterpolation::LibSamplerateInterpolation()  : _speed (1.0L), state (0)
{
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
	
	data[channel]->data_in       = input;
	data[channel]->data_out      = output;
	
	data[channel]->input_frames  = nframes * _speed;
	data[channel]->output_frames = nframes;
	data[channel]->src_ratio     = 1.0/_speed; 

	if ((error = src_process (state[channel], data[channel]))) {	
		printf ("\nError : %s\n\n", src_strerror (error));
		exit (1);
	}
	
	//printf("INTERPOLATION: channel %d input_frames_used: %d\n", channel, data[channel]->input_frames_used);
	
	return data[channel]->input_frames_used;
}
