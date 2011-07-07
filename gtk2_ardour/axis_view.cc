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

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/gtk_ui.h>

#include "ardour/session.h"
#include "ardour/utils.h"

#include "public_editor.h"
#include "ardour_ui.h"
#include "gui_object.h"
#include "axis_view.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;

list<Gdk::Color> AxisView::used_colors;

AxisView::AxisView (ARDOUR::Session* sess)
	: SessionHandlePtr (sess)
{
	_selected = false;
}

AxisView::~AxisView()
{
}

Gdk::Color
AxisView::unique_random_color()
{
  	Gdk::Color newcolor;

	while (1) {

		/* avoid neon/glowing tones by limiting them to the
		   "inner section" (paler) of a color wheel/circle.
		*/

		const int32_t max_saturation = 48000; // 65535 would open up the whole color wheel

		newcolor.set_red (random() % max_saturation);
		newcolor.set_blue (random() % max_saturation);
		newcolor.set_green (random() % max_saturation);

		if (used_colors.size() == 0) {
			used_colors.push_back (newcolor);
			return newcolor;
		}

		for (list<Gdk::Color>::iterator i = used_colors.begin(); i != used_colors.end(); ++i) {
		  Gdk::Color c = *i;
			float rdelta, bdelta, gdelta;

			rdelta = newcolor.get_red() - c.get_red();
			bdelta = newcolor.get_blue() - c.get_blue();
			gdelta = newcolor.get_green() - c.get_green();

			if (sqrt (rdelta*rdelta + bdelta*bdelta + gdelta*gdelta) > 25.0) {
				used_colors.push_back (newcolor);
				return newcolor;
			}
		}

		/* XXX need throttle here to make sure we don't spin for ever */
	}
}

void
AxisView::set_gui_property (const string& property_name, const string& value)
{
	ARDOUR_UI::instance()->gui_object_state->set (state_id(), property_name, value);
}

void
AxisView::set_gui_property (const string& property_name, int value)
{
	ARDOUR_UI::instance()->gui_object_state->set (state_id(), property_name, value);
}

string
AxisView::gui_property (const string& property_name) const
{
	return ARDOUR_UI::instance()->gui_object_state->get_string (state_id(), property_name);
}

bool
AxisView::marked_for_display () const
{
	return string_is_affirmative (gui_property ("visible"));
}

bool
AxisView::set_marked_for_display (bool yn)
{
	if (yn != marked_for_display()) {
		if (yn) {
			set_gui_property ("visible", "yes");
		} else {
			set_gui_property ("visible", "no");
		}
		return true; // things changed
	}

	return false;
}

