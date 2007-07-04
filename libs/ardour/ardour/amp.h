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

#ifndef __ardour_amp_h__
#define __ardour_amp_h__

#include <ardour/types.h>

namespace ARDOUR {

class BufferSet;


/** Applies a declick operation to all audio inputs, passing the same number of
 * audio outputs, and passing through any other types unchanged.
 *
 * FIXME: make this a Processor.
 */
class Amp {
public:
	static void run_in_place (BufferSet& bufs, nframes_t nframes, gain_t initial, gain_t target, bool invert_polarity);

	static void apply_simple_gain(BufferSet& bufs, nframes_t nframes, gain_t target);
};


} // namespace ARDOUR

#endif // __ardour_amp_h__
