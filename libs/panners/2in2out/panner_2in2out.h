/*
 * Copyright (C) 2011-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_panner_2in2out_h__
#define __ardour_panner_2in2out_h__

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "pbd/cartesian.h"
#include "pbd/controllable.h"
#include "pbd/stateful.h"

#include "ardour/automatable.h"
#include "ardour/automation_control.h"
#include "ardour/panner.h"
#include "ardour/types.h"

namespace ARDOUR
{
class Panner2in2out : public Panner
{
public:
	Panner2in2out (boost::shared_ptr<Pannable>);
	~Panner2in2out ();

	ChanCount in () const
	{
		return ChanCount (DataType::AUDIO, 2);
	}
	ChanCount out () const
	{
		return ChanCount (DataType::AUDIO, 2);
	}

	bool clamp_position (double&);
	bool clamp_width (double&);

	std::pair<double, double> position_range () const;
	std::pair<double, double> width_range () const;

	void set_position (double);
	void set_width (double);

	double position () const;
	double width () const;

	static Panner* factory (boost::shared_ptr<Pannable>, boost::shared_ptr<Speakers>);

	std::string value_as_string (boost::shared_ptr<const AutomationControl>) const;

	XMLNode& get_state ();

	void update ();

	void reset ();
	void thaw ();

protected:
	float left[2];
	float right[2];
	float desired_left[2];
	float desired_right[2];
	float left_interp[2];
	float right_interp[2];

private:
	bool clamp_stereo_pan (double& direction_as_lr_fract, double& width);

	void distribute_one (AudioBuffer& srcbuf, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t which);
	void distribute_one_automated (AudioBuffer& srcbuf, BufferSet& obufs,
	                               samplepos_t start, samplepos_t end, pframes_t nframes,
	                               pan_t** buffers, uint32_t which);
};

} // namespace ARDOUR

#endif /* __ardour_panner_2in2out_h__ */
