/*
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk2_ardour_axis_provider_h__
#define __gtk2_ardour_axis_provider_h__

#include <boost/shared_ptr.hpp>

namespace ARDOUR {
	class Stripable;
	class AutomationControl;
}

class AxisView;

class AxisViewProvider
{
public:
	virtual ~AxisViewProvider () {}
	virtual AxisView* axis_view_by_stripable (boost::shared_ptr<ARDOUR::Stripable>) const = 0;
	virtual AxisView* axis_view_by_control (boost::shared_ptr<ARDOUR::AutomationControl>) const = 0;
};

#endif /* __gtk2_ardour_axis_provider_h__ */
