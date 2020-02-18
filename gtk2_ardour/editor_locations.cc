/*
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
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

#include "editor_locations.h"
#include "location_ui.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtk;

EditorLocations::EditorLocations (Editor* e)
	: EditorComponent (e)
{
	_locations = new LocationUI (X_("EditorLocations"));
	_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_NEVER);
	_scroller.add (*_locations);
}

void
EditorLocations::set_session (ARDOUR::Session* s)
{
	SessionHandlePtr::set_session (s);
	_locations->set_session (s);
}

Widget&
EditorLocations::widget()
{
	return _scroller;
}

XMLNode&
EditorLocations::get_state () const
{
	return _locations->get_state();
}

int
EditorLocations::set_state (const XMLNode& node)
{
	return _locations->set_state(node);
}
