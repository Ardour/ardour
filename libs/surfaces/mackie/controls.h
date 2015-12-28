/*
	Copyright (C) 2006,2007 John Anderson
	Copyright (C) 2012 Paul Davis

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

#ifndef __mackie_controls_h__
#define __mackie_controls_h__

#include <map>
#include <vector>
#include <string>
#include <stdint.h>

#include <boost/smart_ptr.hpp>

#include "pbd/signals.h"

#include "ardour/types.h"

#include "mackie_control_exception.h"
#include "midi_byte_array.h"

namespace ARDOUR {
	class AutomationControl;
	class Route;
}

namespace ArdourSurface {

namespace Mackie {

class Strip;
class Group;
class Surface;

class Control {
public:
	Control (int id, std::string name, Group& group);
	virtual ~Control() {}

	int id() const { return _id; }
	const std::string & name() const  { return _name; }
	Group & group() const { return _group; }

	bool in_use () const;
	void set_in_use (bool);

	// Keep track of the timeout so it can be updated with more incoming events
	sigc::connection in_use_connection;

	virtual MidiByteArray zero() = 0;

	/* we keep a pointer to an AutomationControl so that we can convert
	 * easily between interface (GUI) values (normalized to 0..1.0) and
	 * internal values (whatever range the control itself might have).
	 */

	boost::shared_ptr<ARDOUR::AutomationControl> control () const { return normal_ac; }
	virtual void set_control (boost::shared_ptr<ARDOUR::AutomationControl>);

	/* this is the route (possibly null) which has some kind of parameter
	 * controlled by this Control
	 */

	boost::shared_ptr<ARDOUR::Route> route() const { return _route; }
	virtual void set_route (boost::shared_ptr<ARDOUR::Route>);

	/* this describes the parameter that we control */

	ARDOUR::AutomationType automation_type() const;

	float get_value ();
	void set_value (float val);

	virtual void start_touch (double when);
	virtual void stop_touch (bool mark, double when);

  protected:
	boost::shared_ptr<ARDOUR::AutomationControl> normal_ac;

  private:
	int _id; /* possibly device-dependent ID */
	std::string _name;
	Group& _group;
	bool _in_use;
	boost::shared_ptr<ARDOUR::Route> _route;
};

}
}

std::ostream & operator <<  (std::ostream & os, const ArdourSurface::Mackie::Control & control);

#endif /* __mackie_controls_h__ */
