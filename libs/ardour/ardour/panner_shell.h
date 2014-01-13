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

#include "ardour/types.h"
#include "ardour/session_object.h"

namespace ARDOUR {

class Session;
class Route;
class Panner;
class BufferSet;
class AudioBuffer;
class Speakers;
class Pannable;

/** Class to manage panning by instantiating and controlling
 *  an appropriate Panner object for a given in/out configuration.
 */
class PannerShell : public SessionObject
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

	std::string current_panner_uri() const { return _current_panner_uri; }
	std::string user_selected_panner_uri() const { return _user_selected_panner_uri; }
	std::string panner_gui_uri() const { return _panner_gui_uri; }

	/* this function takes the process lock */
	bool select_panner_by_uri (std::string const uri);

  private:
	friend class Route;
	void distribute_no_automation (BufferSet& src, BufferSet& dest, pframes_t nframes, gain_t gain_coeff);
	bool set_user_selected_panner_uri (std::string const uri);

	boost::shared_ptr<Panner> _panner;
	boost::shared_ptr<Pannable> _pannable;
	bool _bypassed;

	std::string _current_panner_uri;
	std::string _user_selected_panner_uri;
	std::string _panner_gui_uri;
	bool _force_reselect;
};

} // namespace ARDOUR

#endif /* __ardour_panner_shell_h__ */
