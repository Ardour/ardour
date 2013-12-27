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

#ifndef __ardour_panner_h__
#define __ardour_panner_h__

#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <iostream>

#include "pbd/cartesian.h"
#include "pbd/signals.h"
#include "pbd/stateful.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/automation_control.h"
#include "ardour/automatable.h"

namespace ARDOUR {

class Session;
class Pannable;
class BufferSet;
class AudioBuffer;
class Speakers;

class LIBARDOUR_API Panner : public PBD::Stateful, public PBD::ScopedConnectionList
{
public:
	Panner (boost::shared_ptr<Pannable>);
	~Panner ();

	virtual boost::shared_ptr<Speakers> get_speakers() const { return boost::shared_ptr<Speakers>(); }

	virtual ChanCount in() const = 0;
	virtual ChanCount out() const = 0;

	virtual void configure_io (ARDOUR::ChanCount /*in*/, ARDOUR::ChanCount /*out*/) {}

	/* derived implementations of these methods must indicate
	   whether it is legal for a Controllable to use the
	   value of the argument (post-call) in a call to
	   Controllable::set_value().

	   they have a choice of:

	   * return true, leave argument unchanged
	   * return true, modify argument
	   * return false
	*/

	virtual bool clamp_position (double&) { return true; }
	virtual bool clamp_width (double&) { return true; }
	virtual bool clamp_elevation (double&) { return true; }

	virtual std::pair<double, double> position_range () const { return std::make_pair (-DBL_MAX, DBL_MAX); }
	virtual std::pair<double, double> width_range () const { return std::make_pair (-DBL_MAX, DBL_MAX); }
	virtual std::pair<double, double> elevation_range () const { return std::make_pair (-DBL_MAX, DBL_MAX); }

	virtual void set_position (double) { }
	virtual void set_width (double) { }
	virtual void set_elevation (double) { }

	virtual double position () const { return 0.0; }
	virtual double width () const { return 0.0; }
	virtual double elevation () const { return 0.0; }

	virtual PBD::AngularVector signal_position (uint32_t) const { return PBD::AngularVector(); }

	virtual void reset () = 0;

	void      set_automation_state (AutoState);
	AutoState automation_state() const;
	void      set_automation_style (AutoStyle);
	AutoStyle automation_style() const;

	virtual std::set<Evoral::Parameter> what_can_be_automated() const;
	virtual std::string describe_parameter (Evoral::Parameter);
	virtual std::string value_as_string (boost::shared_ptr<AutomationControl>) const;

	bool touching() const;

	static double azimuth_to_lr_fract (double azi) {
		/* 180.0 degrees=> left => 0.0 */
		/* 0.0 degrees => right => 1.0 */

		/* humans can only distinguish 1 degree of arc between two positions,
		   so force azi back to an integral value before computing
		*/

		return 1.0 - (rint(azi)/180.0);
	}

	static double lr_fract_to_azimuth (double fract) {
		/* fract = 0.0 => degrees = 180.0 => left */
		/* fract = 1.0 => degrees = 0.0 => right */

		/* humans can only distinguish 1 degree of arc between two positions,
		   so force azi back to an integral value after computing
		*/

		return rint (180.0 - (fract * 180.0));
	}

	/**
	 *  Pan some input buffers to a number of output buffers.
	 *
	 *  @param ibufs Input buffers (one per panner input)
	 *  @param obufs Output buffers (one per panner output).
	 *  @param gain_coeff fixed, additional gain coefficient to apply to output samples.
	 *  @param nframes Number of frames in the input.
	 *
	 *  Derived panners can choose to implement these if they need to gain more
	 *  control over the panning algorithm.  The default is to call
	 *  distribute_one() or distribute_one_automated() on each input buffer to
	 *  deliver it to each output buffer.
	 *
	 *  If a panner does not need to override this default behaviour, it can
	 *  just implement distribute_one() and distribute_one_automated() (below).
	 */
	virtual void distribute (BufferSet& ibufs, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes);
	virtual void distribute_automated (BufferSet& ibufs, BufferSet& obufs,
	                                   framepos_t start, framepos_t end, pframes_t nframes,
	                                   pan_t** buffers);

	int set_state (const XMLNode&, int version);
	XMLNode& get_state ();
	
	boost::shared_ptr<Pannable> pannable() const { return _pannable; }

	static bool equivalent (pan_t a, pan_t b) {
		return fabsf (a - b) < 0.002; // about 1 degree of arc for a stereo panner
	}

	static bool equivalent (const PBD::AngularVector& a, const PBD::AngularVector& b) {
		/* XXX azimuth only, at present */
		return fabs (a.azi - b.azi) < 1.0;
	}

        virtual void freeze ();
        virtual void thaw ();

protected:
	boost::shared_ptr<Pannable> _pannable;

	virtual void distribute_one (AudioBuffer&, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t which) = 0;
	virtual void distribute_one_automated (AudioBuffer&, BufferSet& obufs,
	                                       framepos_t start, framepos_t end, pframes_t nframes,
	                                       pan_t** buffers, uint32_t which) = 0;

        int32_t _frozen;
};

} // namespace

extern "C" {
struct LIBARDOUR_API PanPluginDescriptor {
	std::string name;
	int32_t in;
	int32_t out;
	ARDOUR::Panner* (*factory)(boost::shared_ptr<ARDOUR::Pannable>, boost::shared_ptr<ARDOUR::Speakers>);
};
}

#endif /* __ardour_panner_h__ */
