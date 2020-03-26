/*
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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


/* This section is for actual panners to use. They will include this file,
 * declare ARDOURPANNER_DLL_EXPORTS during compilation, and ... voila.
 */

#ifdef ARDOURPANNER_DLL_EXPORTS // defined if we are building a panner implementation
    #define ARDOURPANNER_API LIBARDOUR_DLL_EXPORT
  #else
    #define ARDOURPANNER_API LIBARDOUR_DLL_IMPORT
  #endif
#define ARDOURPANNER_LOCAL LIBARDOUR_DLL_LOCAL

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

	/* azimut, width or elevation updated -> recalc signal_position ->  emit Changed */
	PBD::Signal0<void> SignalPositionChanged;

	/**
	 *  Pan some input buffers to a number of output buffers.
	 *
	 *  @param ibufs Input buffers (one per panner input)
	 *  @param obufs Output buffers (one per panner output).
	 *  @param gain_coeff fixed, additional gain coefficient to apply to output samples.
	 *  @param nframes Number of samples in the input.
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
	                                   samplepos_t start, samplepos_t end, pframes_t nframes,
	                                   pan_t** buffers);

	int set_state (const XMLNode&, int version);
	XMLNode& get_state ();

	boost::shared_ptr<Pannable> pannable() const { return _pannable; }

	virtual void freeze ();
	virtual void thaw ();

	const std::set<Evoral::Parameter>& what_can_be_automated() const {
		return _can_automate_list;
	}

	virtual std::string value_as_string (boost::shared_ptr<const AutomationControl>) const = 0;

protected:
	virtual void distribute_one (AudioBuffer&, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t which) = 0;
	virtual void distribute_one_automated (AudioBuffer&, BufferSet& obufs,
	                                       samplepos_t start, samplepos_t end, pframes_t nframes,
	                                       pan_t** buffers, uint32_t which) = 0;

	boost::shared_ptr<Pannable> _pannable;
	std::set<Evoral::Parameter> _can_automate_list;

	int32_t _frozen;
};

} // namespace

extern "C" {
struct LIBARDOUR_API PanPluginDescriptor {
	std::string name;
	std::string panner_uri;
	std::string gui_uri;
	int32_t in;
	int32_t out;
	uint32_t priority;
	ARDOUR::Panner* (*factory)(boost::shared_ptr<ARDOUR::Pannable>, boost::shared_ptr<ARDOUR::Speakers>);
};
}

#endif /* __ardour_panner_h__ */
