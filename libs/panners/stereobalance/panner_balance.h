/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_panner_balance_h__
#define __ardour_panner_balance_h__

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
class Pannerbalance : public Panner
{
public:
	Pannerbalance (boost::shared_ptr<Pannable>);
	~Pannerbalance ();

	ChanCount in () const
	{
		return ChanCount (DataType::AUDIO, 2);
	}
	ChanCount out () const
	{
		return ChanCount (DataType::AUDIO, 2);
	}

	void                      set_position (double);
	bool                      clamp_position (double&);
	std::pair<double, double> position_range () const;
	double                    position () const;

	static Panner* factory (boost::shared_ptr<Pannable>, boost::shared_ptr<Speakers>);

	std::string value_as_string (boost::shared_ptr<const AutomationControl>) const;

	XMLNode& get_state ();

	void reset ();
	void thaw ();

protected:
	float pos[2];
	float desired_pos[2];
	float pos_interp[2];

	void update ();

private:
	void distribute_one (AudioBuffer& srcbuf, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t which);
	void distribute_one_automated (AudioBuffer& srcbuf, BufferSet& obufs,
	                               samplepos_t start, samplepos_t end, pframes_t nframes,
	                               pan_t** buffers, uint32_t which);
};

} // namespace ARDOUR

#endif /* __ardour_panner_balance_h__ */
