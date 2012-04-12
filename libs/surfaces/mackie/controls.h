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

#include "mackie_control_exception.h"
#include "midi_byte_array.h"

namespace ARDOUR {
	class AutomationControl;
}

namespace Mackie
{

class Strip;
class Group;
class Surface;

class Control
{
public:
	Control (int id, std::string name, Group& group);
	virtual ~Control() {}
	
	int id() const { return _id; }
	const std::string & name() const  { return _name; }
	Group & group() const { return _group; }

	bool in_use () const;
	void set_in_use (bool);
	
	/// Keep track of the timeout so it can be updated with more incoming events
	sigc::connection in_use_connection;

	virtual MidiByteArray zero() = 0;

	/** If we are doing an in_use timeout for a fader without touch, this
	 *  is its touch button control; otherwise 0.
	 */
	Control* in_use_touch_control;

	boost::shared_ptr<ARDOUR::AutomationControl> control (bool modified) const { return modified ? modified_ac : normal_ac; }

	virtual void set_normal_control (boost::shared_ptr<ARDOUR::AutomationControl>);
	virtual void set_modified_control (boost::shared_ptr<ARDOUR::AutomationControl>);

	float get_value (bool modified = false);
	void set_value (float val, bool modified = false);
	
	virtual void start_touch (double when, bool modified);
	virtual void stop_touch (double when, bool mark, bool modified);

  protected:
	/* a control can operate up to 2 different AutomationControls
	   in any given mode. both of them may be unset at any time.
	*/
	boost::shared_ptr<ARDOUR::AutomationControl> normal_ac;
	boost::shared_ptr<ARDOUR::AutomationControl> modified_ac;

  private:
	int _id;
	std::string _name;
	Group& _group;
	bool _in_use;
};

std::ostream & operator <<  (std::ostream & os, const Control & control);

}

#endif /* __mackie_controls_h__ */
