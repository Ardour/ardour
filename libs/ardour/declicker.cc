/*
    Copyright (C) 2006 Paul Davis 
    
    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
    
    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.
    
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <ardour/declicker.h>

#include <algorithm>
#include <cmath>
#include <ardour/buffer_set.h>
#include <ardour/buffer.h>

namespace ARDOUR {


/** Apply a declick operation to the audio buffers of @a bufs */
void
Declicker::run (BufferSet& bufs, jack_nframes_t nframes, gain_t initial, gain_t target, bool invert_polarity)
{
	if (bufs.count().get(DataType::AUDIO) == 0)
		return;

	assert(bufs.buffer_capacity(DataType::AUDIO) >= nframes);

	const jack_nframes_t declick = std::min ((jack_nframes_t)4096, nframes);
	gain_t         delta;
	double         fractional_shift = -1.0/declick;
	double         fractional_pos;
	gain_t         polscale = invert_polarity ? -1.0f : 1.0f;

	if (nframes == 0)
		return;

	if (target < initial) {
		/* fade out: remove more and more of delta from initial */
		delta = -(initial - target);
	} else {
		/* fade in: add more and more of delta from initial */
		delta = target - initial;
	}

	for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
		Sample* const buffer = i->data(nframes);

		fractional_pos = 1.0;

		for (jack_nframes_t nx = 0; nx < declick; ++nx) {
			buffer[nx] *= polscale * (initial + (delta * (0.5 + 0.5 * cos (M_PI * fractional_pos))));
			fractional_pos += fractional_shift;
		}
		
		/* now ensure the rest of the buffer has the target value applied, if necessary. */
		
		if (declick != nframes) {

			if (invert_polarity) {
				target = -target;
			}

			if (target == 0.0) {
				memset (&buffer[declick], 0, sizeof (Sample) * (nframes - declick));
			} else if (target != 1.0) {
				for (jack_nframes_t nx = declick; nx < nframes; ++nx) {
					buffer[nx] *= target;
				}
			}
		}
	}
}


} // namespace ARDOUR
