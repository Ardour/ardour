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
#include "midi_byte_array.h"

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
	
	/// the value of the second bytes of the message. It's
	/// the id of the control, but only guaranteed to be
	/// unique within the control type.
	int raw_id() const { return _id; }

	const std::string & name() const  { return _name; }
	Group & group() const { return _group; }

	virtual bool accepts_feedback() const  { return true; }
	
	bool in_use () const;
	void set_in_use (bool);
	
	/// Keep track of the timeout so it can be updated with more incoming events
	sigc::connection in_use_connection;

	virtual MidiByteArray zero() = 0;

	/** If we are doing an in_use timeout for a fader without touch, this
	 *  is its touch button control; otherwise 0.
	 */
	Control* in_use_touch_control;

private:
	int _id;
	std::string _name;
	Group& _group;
	bool _in_use;
};

std::ostream & operator <<  (std::ostream & os, const Control & control);

}

#endif /* __mackie_controls_h__ */
