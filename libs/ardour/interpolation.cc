#include <stdint.h>

#include "ardour/interpolation.h"

using namespace ARDOUR;

Interpolation::Interpolation()  : _speed (1.0L), state (0)
{
}

Interpolation::~Interpolation() 
{
	state = src_delete (state);
}

void
Interpolation::set_speed (double new_speed)
{ 
	_speed = new_speed; 
	src_set_ratio (state, 1.0/_speed); 
}

void
Interpolation::reset_state ()
{
	if (state) {
		src_reset (state);
	} else {
		state = src_new (SRC_LINEAR, 1, &error);
	}
}

void
Interpolation::add_channel_to (int input_buffer_size, int output_buffer_size) 
{
	SRC_DATA newdata;
	
	/* Set up sample rate converter info. */
	newdata.end_of_input = 0 ; 

	newdata.input_frames  = input_buffer_size;
	newdata.output_frames = output_buffer_size;

	newdata.input_frames_used = 0 ;
	newdata.output_frames_gen = 0 ;

	newdata.src_ratio = 1.0/_speed;
	
	data.push_back (newdata);
	
	reset_state ();
}

void
Interpolation::remove_channel_from () 
{
	data.pop_back ();
	reset_state ();
}

nframes_t
Interpolation::interpolate (int channel, nframes_t nframes, Sample *input, Sample *output)
{	
	data[channel].data_in       = input;
	data[channel].data_out      = output;
	
	data[channel].output_frames = nframes;
	data[channel].src_ratio     = 1.0/_speed; 

	if ((error = src_process (state, &data[channel]))) {	
		printf ("\nError : %s\n\n", src_strerror (error));
		exit (1);
	}

	return data[channel].input_frames_used;
}
