/*
    Copyright (C) 2003 Paul Davis

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

#include <cstdlib>
#include <cmath>

#include <algorithm>
#include <string>

#include <list>

#include "pbd/error.h"
#include "pbd/convert.h"

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/gtk_ui.h>

#include "public_editor.h"
#include "ardour_ui.h"
#include "gui_object.h"
#include "axis_view.h"
#include "utils.h"
#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

list<Gdk::Color> AxisView::used_colors;

AxisView::AxisView ()
{
}

AxisView::~AxisView()
{
}

Gdk::Color
AxisView::unique_random_color()
{
	return ::unique_random_color (used_colors);
}

string
AxisView::gui_property (const string& property_name) const
{
	if (property_hashtable.count(property_name)) {
		return property_hashtable[property_name];
	} else {
		string rv = gui_object_state().get_string (state_id(), property_name);
		property_hashtable.erase(property_name);
		property_hashtable.emplace(property_name, rv);
		return rv;
	}
}

bool
AxisView::get_gui_property (const std::string& property_name, std::string& value) const
{
	std::string str = gui_property(property_name);

	if (!str.empty()) {
		value = str;
		return true;
	}

	return false;
}

void
AxisView::set_gui_property (const std::string& property_name, const std::string& value)
{
	property_hashtable.erase (property_name);
	property_hashtable.emplace (property_name, value);
	gui_object_state ().set_property (state_id (), property_name, value);
}

bool
AxisView::marked_for_display () const
{
	string const v = gui_property ("visible");
	return (v == "" || PBD::string_is_affirmative (v));
}

bool
AxisView::set_marked_for_display (bool yn)
{
	string const v = gui_property ("visible");
	if (v == "" || yn != PBD::string_is_affirmative (v)) {
		set_gui_property ("visible", yn);
		return true; // things changed
	}
	return false;
}

GUIObjectState&
AxisView::gui_object_state()
{
	return *ARDOUR_UI::instance()->gui_object_state;
}

void
AxisView::set_selected (bool yn)
{
	if (selected() == yn) {
		return;
	}

	Selectable::set_selected (yn);

	boost::shared_ptr<Stripable> s = stripable ();

	if (s) {
		s->presentation_info().set_selected (yn);
	}
}
