/*
    Copyright (C) 2014 Paul Davis

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

#include <iostream>
#include <cmath>
#include <algorithm>

#include <pangomm/layout.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/stacktrace.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/gui_thread.h"

#include "ardour/rc_configuration.h" // for widget prelight preference

#include "ardour_dropdown.h"
#include "ardour_ui.h"
#include "global_signals.h"

#include "i18n.h"

#define REFLECTION_HEIGHT 2

using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using std::max;
using std::min;
using namespace std;


ArdourDropdown::ArdourDropdown (Element e)
{
	signal_button_press_event().connect (sigc::mem_fun(*this, &ArdourDropdown::on_mouse_pressed));

	add_elements(e);
	add_elements(ArdourButton::Menu);
}

ArdourDropdown::~ArdourDropdown ()
{
}

bool
ArdourDropdown::on_mouse_pressed (GdkEventButton*)
{
	_menu.popup (1, gtk_get_current_event_time());	
	return true;
}


void
ArdourDropdown::AddMenuElem (Menu_Helpers::Element e)
{
	using namespace Menu_Helpers;

	MenuList& items = _menu.items ();
	
	items.push_back (e);
}


