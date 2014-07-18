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
#include "gtkmm2ext/keyboard.h"

#include "ardour/rc_configuration.h" // for widget prelight preference

#include "ardour_display.h"
#include "ardour_ui.h"
#include "global_signals.h"

#include "i18n.h"

using namespace Gtkmm2ext;
using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using std::max;
using std::min;
using namespace std;


ArdourDisplay::ArdourDisplay (Element e)
{
	signal_button_press_event().connect (sigc::mem_fun(*this, &ArdourDisplay::on_mouse_pressed));

	add_elements(e);
	add_elements(ArdourButton::Menu);
	add_elements(ArdourButton::Text);
}

ArdourDisplay::~ArdourDisplay ()
{
}

bool
ArdourDisplay::on_mouse_pressed (GdkEventButton*)
{
	_menu.popup (1, gtk_get_current_event_time());	
	return true;
}

bool
ArdourDisplay::on_scroll_event (GdkEventScroll* ev)
{
	/* mouse wheel */

	float scale = 1.0;
	if (ev->state & Keyboard::GainFineScaleModifier) {
		if (ev->state & Keyboard::GainExtraFineScaleModifier) {
			scale *= 0.01;
		} else {
			scale *= 0.10;
		}
	}

	boost::shared_ptr<PBD::Controllable> c = binding_proxy.get_controllable();
	if (c) {
		float val = c->get_interface();
	
		if ( ev->direction == GDK_SCROLL_UP )
			val += 0.05 * scale;  //by default, we step in 1/20ths of the knob travel
		else
			val -= 0.05 * scale;
			
		c->set_interface(val);
	}

	return true;
}


void
ArdourDisplay::add_controllable_preset (const char *txt, float val)
{
	using namespace Menu_Helpers;

	MenuList& items = _menu.items ();

	items.push_back (MenuElem (txt, sigc::bind (sigc::mem_fun(*this, &ArdourDisplay::handle_controllable_preset), val)));
}


void
ArdourDisplay::handle_controllable_preset (float p)
{
	boost::shared_ptr<PBD::Controllable> c = binding_proxy.get_controllable();

	if (!c) return;

	c->set_user(p);
}


void
ArdourDisplay::set_controllable (boost::shared_ptr<Controllable> c)
{
    watch_connection.disconnect ();  //stop watching the old controllable

	if (!c) return;

	binding_proxy.set_controllable (c);

	c->Changed.connect (watch_connection, invalidator(*this), boost::bind (&ArdourDisplay::controllable_changed, this), gui_context());

	controllable_changed();
}

void
ArdourDisplay::controllable_changed ()
{
	boost::shared_ptr<PBD::Controllable> c = binding_proxy.get_controllable();

	if (!c) return;

	set_text(c->get_user_string());

	set_dirty();
}
