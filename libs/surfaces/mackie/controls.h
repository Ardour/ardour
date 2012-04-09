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

#include "pbd/signals.h"

#include "mackie_control_exception.h"

namespace Mackie
{

class Strip;
class Group;
class Led;
class Surface;

class Control
{
public:
	enum type_t { 
		type_led, 
		type_led_ring, 
		type_fader = 0xe0, 
		type_button = 0x90, 
		type_pot = 0xb0,
		type_meter = 0xd0
	};

	enum base_id_t {
		fader_base_id = 0x0,
		pot_base_id = 0x10,
		jog_base_id = 0x3c,
		fader_touch_button_base_id = 0x68,
		vselect_button_base_id = 0x20,
		select_button_base_id = 0x18,
		mute_button_base_id = 0x10,
		solo_button_base_id = 0x08,
		recenable_button_base_id = 0x0,
		meter_base_id = 0xd0,
	};
	
	Control (int id, int ordinal, std::string name, Group& group);
	virtual ~Control() {}
	
	virtual const Led & led() const { throw MackieControlException ("no led available"); }

	/// type() << 8 + midi id of the control. This
	/// provides a unique id for any control on the surface.
	int id() const { return (type() << 8) + _id; }
	
	/// the value of the second bytes of the message. It's
	/// the id of the control, but only guaranteed to be
	/// unique within the control type.
	int raw_id() const { return _id; }
	
	/// The 1-based number of the control
	int ordinal() const { return _ordinal; }
	
	const std::string & name() const  { return _name; }
	const Group & group() const { return _group; }
	virtual bool accepts_feedback() const  { return true; }
	
	virtual type_t type() const = 0;
	
	/// Return true if this control is the one and only Jog Wheel
	virtual bool is_jog() const { return false; }

	bool in_use () const;
	void set_in_use (bool);
	
	/// Keep track of the timeout so it can be updated with more incoming events
	sigc::connection in_use_connection;

	/** If we are doing an in_use timeout for a fader without touch, this
	 *  is its touch button control; otherwise 0.
	 */
	Control* in_use_touch_control;

private:
	int _id;
	int _ordinal;
	std::string _name;
	Group& _group;
	bool _in_use;
};

std::ostream & operator <<  (std::ostream & os, const Control & control);

}

#endif /* __mackie_controls_h__ */
