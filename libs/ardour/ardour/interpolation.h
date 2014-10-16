/*
    Copyright (C) 1999-2010 Paul Davis

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

#include <math.h>
#include <samplerate.h>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

#ifndef __interpolation_h__
#define __interpolation_h__

namespace ARDOUR {

class LIBARDOUR_API Interpolation {
protected:
	double _speed;
	double _target_speed;

	// the idea is that when the speed is not 1.0, we have to
	// interpolate between samples and then we have to store where we thought we were.
	// rather than being at sample N or N+1, we were at N+0.8792922
	std::vector<double> phase;

public:
	Interpolation ()  { _speed = 1.0; _target_speed = 1.0; }
	~Interpolation () { phase.clear(); }

	void set_speed (double new_speed)          { _speed = new_speed; _target_speed = new_speed; }
	void set_target_speed (double new_speed)   { _target_speed = new_speed; }

	double target_speed()          const { return _target_speed; }
	double speed()                 const { return _speed; }

	void add_channel_to (int /*input_buffer_size*/, int /*output_buffer_size*/) { phase.push_back (0.0); }
	void remove_channel_from () { phase.pop_back (); }

	void reset () {
		for (size_t i = 0; i < phase.size(); i++) {
			phase[i] = 0.0;
		}
	}
};

class LIBARDOUR_API LinearInterpolation : public Interpolation {
public:
	framecnt_t interpolate (int channel, framecnt_t nframes, Sample* input, Sample* output);
};

class LIBARDOUR_API CubicInterpolation : public Interpolation {
public:
	framecnt_t interpolate (int channel, framecnt_t nframes, Sample* input, Sample* output);
};

} // namespace ARDOUR

#endif
