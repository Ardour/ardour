/*
    Copyright (C) 2011 Paul Davis

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

#ifndef __libardour_pannable_h__
#define __libardour_pannable_h__

#include <string>

#include <boost/shared_ptr.hpp>

#include "evoral/Parameter.hpp"

#include "ardour/automatable.h"
#include "ardour/session_handle.h"

namespace ARDOUR {

class Session;
class AutomationControl;

struct Pannable : public Automatable, public SessionHandleRef {
        Pannable (Session& s);

        boost::shared_ptr<AutomationControl> pan_azimuth_control;
        boost::shared_ptr<AutomationControl> pan_elevation_control;
        boost::shared_ptr<AutomationControl> pan_width_control;
        boost::shared_ptr<AutomationControl> pan_frontback_control;
        boost::shared_ptr<AutomationControl> pan_lfe_control;

        Session& session() { return _session; }

	void set_automation_state (AutoState);
	AutoState automation_state() const { return _auto_state; }
	PBD::Signal1<void, AutoState> automation_state_changed;

	void set_automation_style (AutoStyle m);
	AutoStyle automation_style() const { return _auto_style; }
	PBD::Signal0<void> automation_style_changed;

	bool automation_playback() const {
		return (_auto_state & Play) || ((_auto_state & Touch) && !touching());
	}
	bool automation_write () const {
                return ((_auto_state & Write) || ((_auto_state & Touch) && touching()));
        }

	void start_touch (double when);
	void stop_touch (bool mark, double when);
	bool touching() const { return g_atomic_int_get (&_touching); }
	bool writing() const { return _auto_state == Write; }
        bool touch_enabled() const { return _auto_state == Touch; }

  protected:
        AutoState _auto_state;
        AutoStyle _auto_style;
        gint      _touching;
};

} // namespace 

#endif /* __libardour_pannable_h__ */
