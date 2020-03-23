/*
 * Copyright (C) 2011-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
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

#ifndef __ardour_panner_1in2out_h__
#define __ardour_panner_1in2out_h__

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "pbd/cartesian.h"
#include "pbd/controllable.h"
#include "pbd/stateful.h"

#include "ardour/panner.h"
#include "ardour/types.h"

namespace ARDOUR
{

class Panner1in2out : public Panner
{
public:
	Panner1in2out (boost::shared_ptr<Pannable>);
	~Panner1in2out ();

	void                      set_position (double);
	bool                      clamp_position (double&);
	std::pair<double, double> position_range () const;

	double position () const;

	ChanCount in () const
	{
		return ChanCount (DataType::AUDIO, 1);
	}

	ChanCount out () const
	{
		return ChanCount (DataType::AUDIO, 2);
	}

	static Panner* factory (boost::shared_ptr<Pannable>, boost::shared_ptr<Speakers>);

	std::string value_as_string (boost::shared_ptr<const AutomationControl>) const;

	XMLNode& get_state ();

	void reset ();

protected:
	float left;
	float right;
	float desired_left;
	float desired_right;
	float left_interp;
	float right_interp;

	void distribute_one (AudioBuffer& src, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t which);
	void distribute_one_automated (AudioBuffer& srcbuf, BufferSet& obufs,
	                               samplepos_t start, samplepos_t end, pframes_t nframes,
	                               pan_t** buffers, uint32_t which);

	void update ();
};

} // namespace ARDOUR

#endif /* __ardour_panner_1in2out_h__ */
