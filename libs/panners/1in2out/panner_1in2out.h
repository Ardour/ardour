/*
    Copyright (C) 2004-2011 Paul Davis

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

#ifndef __ardour_panner_1in2out_h__
#define __ardour_panner_1in2out_h__

#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <iostream>

#include "pbd/stateful.h"
#include "pbd/controllable.h"
#include "pbd/cartesian.h"

#include "ardour/types.h"
#include "ardour/panner.h"


namespace ARDOUR {

class Panner1in2out : public Panner
{
  public:
	Panner1in2out (boost::shared_ptr<Pannable>);
	~Panner1in2out ();

    void set_position (double);
    bool clamp_position (double&);
	std::pair<double, double> position_range () const;

        double position() const;

        ChanCount in() const { return ChanCount (DataType::AUDIO, 1); }
        ChanCount out() const { return ChanCount (DataType::AUDIO, 2); }

        std::set<Evoral::Parameter> what_can_be_automated() const;

        static Panner* factory (boost::shared_ptr<Pannable>, boost::shared_ptr<Speakers>);

        std::string describe_parameter (Evoral::Parameter);
        std::string value_as_string (boost::shared_ptr<AutomationControl>) const;

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
                                       framepos_t start, framepos_t end, pframes_t nframes,
                                       pan_t** buffers, uint32_t which);

        void update ();
};

} // namespace

#endif /* __ardour_panner_1in2out_h__ */
