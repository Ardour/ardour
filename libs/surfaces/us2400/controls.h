/*
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __us2400_controls_h__
#define __us2400_controls_h__

#include <map>
#include <vector>
#include <string>
#include <stdint.h>

#include <boost/smart_ptr.hpp>

#include "pbd/controllable.h"
#include "pbd/signals.h"

#include "us2400_control_exception.h"
#include "midi_byte_array.h"

namespace ARDOUR {
	class AutomationControl;
}

namespace Temporal {
	class timepos_t;
}

namespace ArdourSurface {

namespace US2400 {

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

	/** If we are doing an in_use timeout for a fader without touch, this
	 *  is its touch button control; otherwise 0.
	 */
	Control* in_use_touch_control;

	boost::shared_ptr<ARDOUR::AutomationControl> control () const { return normal_ac; }
	virtual void set_control (boost::shared_ptr<ARDOUR::AutomationControl>);
	virtual void reset_control () { normal_ac.reset(); } 

	virtual void mark_dirty() = 0;

	float get_value ();
	void set_value (float val, PBD::Controllable::GroupControlDisposition gcd = PBD::Controllable::UseGroup);

	virtual void start_touch (Temporal::timepos_t const & when);
	virtual void stop_touch (Temporal::timepos_t const & when);

  protected:
	boost::shared_ptr<ARDOUR::AutomationControl> normal_ac;

  private:
	int _id; /* possibly device-dependent ID */
	std::string _name;
	Group& _group;
	bool _in_use;
};

}
}

std::ostream & operator <<  (std::ostream & os, const ArdourSurface::US2400::Control & control);

#endif /* __us2400_controls_h__ */
