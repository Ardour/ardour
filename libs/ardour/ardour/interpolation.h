/*
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 Hans Baier <hansfbaier@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
	virtual ~Interpolation() {}

	void set_speed (double new_speed)          { _speed = new_speed; _target_speed = new_speed; }
	void set_target_speed (double new_speed)   { _target_speed = new_speed; }

	double target_speed()          const { return _target_speed; }
	double speed()                 const { return _speed; }

	void add_channel ()    { phase.push_back (0.0); }
	void remove_channel () { phase.pop_back (); }

	virtual void reset () {
		for (size_t i = 0; i < phase.size(); i++) {
			phase[i] = 0.0;
		}
	}
};

class LIBARDOUR_API CubicInterpolation : public Interpolation {
  public:
	CubicInterpolation ();
	samplecnt_t interpolate (int channel, samplecnt_t input_samples, Sample* input, samplecnt_t & output_samples, Sample* output);
	samplecnt_t distance (samplecnt_t nframes);
	void reset ();

  private:
	Sample z[4];
	char   valid_z_bits;

	bool is_valid (int n) const { return valid_z_bits & (1<<n); }
	bool invalid (int n) const  { return !is_valid (n); }
	void validate (int n)       { valid_z_bits |= (1<<n); }
	void invalidate (int n)     { valid_z_bits &= (1<<n); }
};

} // namespace ARDOUR

#endif
