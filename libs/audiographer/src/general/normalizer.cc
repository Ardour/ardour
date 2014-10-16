/*
    Copyright (C) 2012 Paul Davis 
    Author: Sakari Bergen

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

#include "audiographer/general/normalizer.h"

namespace AudioGrapher
{

Normalizer::Normalizer (float target_dB)
	  : enabled (false)
	  , buffer (0)
	  , buffer_size (0)
{
	target = pow (10.0f, target_dB * 0.05f);
}
	
Normalizer::~Normalizer()
{
	delete [] buffer;
}

/// Sets the peak found in the material to be normalized \see PeakReader \n RT safe
void Normalizer::set_peak (float peak)
{
	if (peak == 0.0f || peak == target) {
		/* don't even try */
		enabled = false;
	} else {
		enabled = true;
		gain = target / peak;
	}
}

/** Allocates a buffer for using with const ProcessContexts
  * This function does not need to be called if
  * non-const ProcessContexts are given to \a process() .
  * \n Not RT safe
  */
void Normalizer::alloc_buffer(framecnt_t frames)
{
	delete [] buffer;
	buffer = new float[frames];
	buffer_size = frames;
}

/// Process a const ProcessContext \see alloc_buffer() \n RT safe
void Normalizer::process (ProcessContext<float> const & c)
{
	if (throw_level (ThrowProcess) && c.frames() > buffer_size) {
		throw Exception (*this, "Too many frames given to process()");
	}
	
	if (enabled) {
		memcpy (buffer, c.data(), c.frames() * sizeof(float));
		Routines::apply_gain_to_buffer (buffer, c.frames(), gain);
	}
	
	ProcessContext<float> c_out (c, buffer);
	ListedSource<float>::output (c_out);
}

/// Process a non-const ProcsesContext in-place \n RT safe
void Normalizer::process (ProcessContext<float> & c)
{
	if (enabled) {
		Routines::apply_gain_to_buffer (c.data(), c.frames(), gain);
	}
	ListedSource<float>::output(c);
}

} // namespace
