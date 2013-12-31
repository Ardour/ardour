/*
    Copyright (C) 2004-2014 Paul Davis
    adopted from 2in2out panner by Robin Gareus <robin@gareus.org>

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

#ifndef __ardour_panner_balance_h__
#define __ardour_panner_balance_h__

#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <iostream>

#include "pbd/stateful.h"
#include "pbd/controllable.h"
#include "pbd/cartesian.h"

#include "ardour/automation_control.h"
#include "ardour/automatable.h"
#include "ardour/panner.h"
#include "ardour/pan_distribution_buffer.h"
#include "ardour/types.h"

#ifdef ENABLE_PANNING_DELAY
#define Pannerbalance PannerbalanceDelay
#include "ardour/pan_delay_buffer.h"
#define PanDistributionBuffer PanDelayBuffer
#else /* !defined(ENABLE_PANNING_DELAY) */
#define PanDistributionBuffer DummyPanDistributionBuffer
#endif /* !defined(ENABLE_PANNING_DELAY) */

namespace ARDOUR {

class Pannerbalance : public Panner
{
  public:
	Pannerbalance (boost::shared_ptr<Pannable>);
	~Pannerbalance ();

	ChanCount in() const { return ChanCount (DataType::AUDIO, 2); }
	ChanCount out() const { return ChanCount (DataType::AUDIO, 2); }

	void set_position (double);
	bool clamp_position (double&);
	std::pair<double, double> position_range () const;
	double position () const;

	std::set<Evoral::Parameter> what_can_be_automated() const;

	static Panner* factory (boost::shared_ptr<Pannable>, boost::shared_ptr<Speakers>);

	std::string describe_parameter (Evoral::Parameter);
	std::string value_as_string (boost::shared_ptr<AutomationControl>) const;

	XMLNode& get_state ();

	void reset ();
	void thaw ();

  protected:
	float gain[2];
	float desired_gain[2];

	PanDistributionBuffer dist_buf_0;
	PanDistributionBuffer dist_buf_1;

	/* Pointers to the two buffers arranged as an array, for convenience.
	 * (The members above are only needed because PanDistributionBuffer is not
	 * default-constructible.) */
	PanDistributionBuffer* dist_buf[2];

	void update ();

  private:
	void distribute_one (AudioBuffer& srcbuf, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t which);
	void distribute_one_automated (AudioBuffer& srcbuf, BufferSet& obufs,
			framepos_t start, framepos_t end, pframes_t nframes,
			pan_t** buffers, uint32_t which);
};

} // namespace

#endif /* __ardour_panner_balance_h__ */
