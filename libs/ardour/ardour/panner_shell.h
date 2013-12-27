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

#ifndef __ardour_panner_shell_h__
#define __ardour_panner_shell_h__

#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <iostream>

#include <boost/noncopyable.hpp>

#include "pbd/cartesian.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/session_object.h"

namespace ARDOUR {

class Session;
class Panner;
class BufferSet;
class AudioBuffer;
class Speakers;
class Pannable;

/** Class to manage panning by instantiating and controlling
 *  an appropriate Panner object for a given in/out configuration.
 */
class LIBARDOUR_API PannerShell : public SessionObject
{
public:
	PannerShell (std::string name, Session&, boost::shared_ptr<Pannable>);
	virtual ~PannerShell ();

	std::string describe_parameter (Evoral::Parameter param);

	bool can_support_io_configuration (const ChanCount& /*in*/, ChanCount& /*out*/) { return true; };
	void configure_io (ChanCount in, ChanCount out);

	/// The fundamental Panner function
	void run (BufferSet& src, BufferSet& dest, framepos_t start_frame, framepos_t end_frames, pframes_t nframes);

	XMLNode& get_state ();
	int      set_state (const XMLNode&, int version);

	PBD::Signal0<void> Changed; /* panner and/or outputs count and/or bypass state changed */

	boost::shared_ptr<Panner> panner() const { return _panner; }
	boost::shared_ptr<Pannable> pannable() const { return _pannable; }

	bool bypassed () const;
	void set_bypassed (bool);

  private:
	void distribute_no_automation (BufferSet& src, BufferSet& dest, pframes_t nframes, gain_t gain_coeff);
	boost::shared_ptr<Panner> _panner;
	boost::shared_ptr<Pannable> _pannable;
	bool _bypassed;
};

} // namespace ARDOUR

#endif /* __ardour_panner_shell_h__ */
