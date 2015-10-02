/*
    Copyright (C) 2014 Sebastian Reichelt

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

#include "ardour/pan_distribution_buffer.h"
#include "ardour/runtime_functions.h"

using namespace ARDOUR;

void
DummyPanDistributionBufferImpl::mix_buffers(Sample *dst, const Sample *src, pframes_t nframes, float gain)
{
	if (gain == 1.0f) {
		/* gain is 1 so we can just copy the input samples straight in */
		mix_buffers_no_gain(dst, src, nframes);
	} else if (gain != 0.0f) {
		/* gain is not 1 but also not 0, so we must do it "properly" */
		mix_buffers_with_gain(dst, src, nframes, gain);
	}
}
