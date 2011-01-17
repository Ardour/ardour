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
#include "ardour/automation_control.h"
#include "ardour/automatable.h"

namespace ARDOUR {

class PannerStereoBase : public class Panner
{
  public:
	PannerStereoBase (Panner&);
	~PannerStereoBase ();

        void set_position (double);

        ChanCount in() const { return ChanCount (DataType::AUDIO, 1); }
        ChanCount out() const { return ChanCount (DataType::AUDIO, 2); }

	/* this class just leaves the pan law itself to be defined
	   by the update(), do_distribute_automated()
	   methods. derived classes also need a factory method
	   and a type name. See EqualPowerStereoPanner as an example.
	*/

	void do_distribute (AudioBuffer& src, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes);

  protected:
        boost::shared_ptr<AutomationControl> _position;
	float left;
	float right;
	float desired_left;
	float desired_right;
	float left_interp;
	float right_interp;
};

}

#endif /* __ardour_panner_1in2out_h__ */
