/*
    Copyright (C) 2009 Paul Davis

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

#include "editor_locations.h"
#include "location_ui.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;

EditorLocations::EditorLocations (Editor* e)
	: EditorComponent (e)
{
	locations = new LocationUI;
}

void
EditorLocations::connect_to_session (ARDOUR::Session* s)
{
	EditorComponent::connect_to_session (s);
	locations->set_session (s);
}

Widget&
EditorLocations::widget() 
{
	return *locations;
}
