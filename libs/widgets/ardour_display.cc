/*
 * Copyright (C) 2014 Paul Davis <paul@linuxaudiosystems.com>
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

#include <iostream>
#include <cmath>
#include <algorithm>

#include <pangomm/layout.h>

#include "pbd/compose.h"
#include "pbd/controllable.h"
#include "pbd/error.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"

#include "widgets/ardour_display.h"

#include "pbd/i18n.h"

using namespace Gtkmm2ext;
using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using namespace ArdourWidgets;
using std::max;
using std::min;
using namespace std;

ArdourDisplay::ArdourDisplay (Element e)
{
	add_elements(ArdourButton::Text);
}

ArdourDisplay::~ArdourDisplay ()
{
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
	AddMenuElem(MenuElem (txt, sigc::bind (sigc::mem_fun(*this, &ArdourDisplay::handle_controllable_preset), val)));
}

static inline float dB_to_coefficient (float dB) {
	return dB > -318.8f ? pow (10.0f, dB * 0.05f) : 0.0f;
}

void
ArdourDisplay::handle_controllable_preset (float p)
{
	boost::shared_ptr<PBD::Controllable> c = binding_proxy.get_controllable();

	if (!c) return;

	/* This should not use dB_to_coefficient(), but the Controllable's value.
	 *
	 * The only user of this API is currently monitor_section.cc which conveniently
	 * binds dB values. Once there are other use-cases, for this, this (GUI only) API
	 * needs fixing.
	 */
	c->set_value(dB_to_coefficient (p), Controllable::NoGroup);
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
