/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include "pbd/destructible.h"

#include "ardour/plugin.h"
#include "ardour/readonly_control.h"

using namespace ARDOUR;

ReadOnlyControl::ReadOnlyControl (boost::shared_ptr<Plugin> p, const ParameterDescriptor& desc, uint32_t pnum)
		: _plugin (boost::weak_ptr<Plugin> (p))
		, _desc (desc)
		, _parameter_num (pnum)
{ }

double
ReadOnlyControl::get_parameter () const
{
	boost::shared_ptr<Plugin> p = _plugin.lock();
	if (p) {
		return p->get_parameter (_parameter_num);
	}
	return 0;
}

std::string
ReadOnlyControl::describe_parameter ()
{
	boost::shared_ptr<Plugin> p = _plugin.lock();
	if (p) {
		return p->describe_parameter (Evoral::Parameter (ARDOUR::PluginAutomation, 0, _parameter_num));
	}
	return "";
}
