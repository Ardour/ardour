/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#include "ardour_message.h"
#include "splash.h"

using namespace Gtk;

ArdourMessageDialog::ArdourMessageDialog (const Glib::ustring& message,
	                                        bool use_markup,
	                                        Gtk::MessageType type,
	                                        Gtk::ButtonsType buttons,
	                                        bool modal)
	: Gtk::MessageDialog (message, use_markup, type, buttons, modal)
	, _splash_pushed (false)
{
	set_position (WIN_POS_MOUSE);
}

ArdourMessageDialog::ArdourMessageDialog (Gtk::Window& parent,
	                                        const Glib::ustring& message,
	                                        bool use_markup,
	                                        Gtk::MessageType type,
	                                        Gtk::ButtonsType buttons,
	                                        bool modal)
	: Gtk::MessageDialog (parent, message, use_markup, type, buttons, modal)
	, _splash_pushed (false)
{
	set_transient_for (parent);
	set_position (WIN_POS_MOUSE);
}

ArdourMessageDialog::~ArdourMessageDialog ()
{
	pop_splash ();
}

int
ArdourMessageDialog::run ()
{
	push_splash ();
	int rv = Gtk::MessageDialog::run ();
	pop_splash ();
	return rv;
}

void
ArdourMessageDialog::show ()
{
	push_splash ();
	Gtk::MessageDialog::show ();
}

void
ArdourMessageDialog::hide ()
{
	Gtk::MessageDialog::hide ();
	pop_splash ();
}

void
ArdourMessageDialog::pop_splash ()
{
	if (_splash_pushed) {
		Splash* spl = Splash::exists () ? Splash::instance() : NULL;
		if (spl) {
			spl->pop_front_for (*this);
		}
		_splash_pushed = false;
	}
}

void
ArdourMessageDialog::push_splash ()
{
	if (Splash::exists()) {
		Splash* spl = Splash::instance();
		if (spl->is_visible()) {
			spl->pop_back_for (*this);
			_splash_pushed = true;
		}
	}
}
