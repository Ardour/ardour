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

#ifndef __ardour_us2400_control_protocol_control_group_h__
#define __ardour_us2400_control_protocol_control_group_h__

#include <vector>

namespace ArdourSurface {
namespace US2400 {

class Control;

/**
	This is a loose group of controls, eg cursor buttons,
	transport buttons, functions buttons etc.
*/
class Group
{
public:
	Group (const std::string & name)
		: _name (name) {}

	virtual ~Group() {}

	virtual bool is_strip() const { return false; }
	virtual bool is_master() const { return false; }

	virtual void add (Control & control);

	const std::string & name() const { return _name; }
	void set_name (const std::string & rhs) { _name = rhs; }

	typedef std::vector<Control*> Controls;
	const Controls & controls() const { return _controls; }

protected:
	Controls _controls;

private:
	std::string _name;
};

}
}

#endif
