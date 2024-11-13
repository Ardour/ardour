/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2024 Ben Loftis <ben@harrisonconsoles.com>
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

#include "pbd/compose.h"
#include <algorithm>

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour/location.h"
#include "ardour/profile.h"
#include "ardour/session.h"

#include "audio_clock.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor.h"
#include "region_view.h"

#include "route_properties_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;
using std::max;
using std::min;

RoutePropertiesBox::RoutePropertiesBox ()
{
	show_all();
}

RoutePropertiesBox::~RoutePropertiesBox ()
{
}

void
RoutePropertiesBox::set_route (std::shared_ptr<ARDOUR::Route> rt)
{
	//TODO: route properties
//	rt->PropertyChanged.connect (state_connection, invalidator (*this), boost::bind (&RoutePropertiesBox::region_changed, this, _1), gui_context ());
}

void
RoutePropertiesBox::property_changed (const PBD::PropertyChange& what_changed)
{


}
