/*
    Copyright (C) 2002 Paul Davis

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

#include <gtkmm2ext/doi.h>

#include "ardour_dialog.h"
#include "keyboard.h"
#include "ardour_ui.h"


ArdourDialog::ArdourDialog (string name)
	: Gtk::Window (GTK_WINDOW_TOPLEVEL),
	  KeyboardTarget (*this, name)
{
	session = 0;
	kbd_input = false;
	running = false;
	_run_status = 0;
	_within_hiding = false;
	hide_on_stop = true;
}

ArdourDialog::~ArdourDialog ()
{
}

gint
ArdourDialog::on_enter_notify_event (GdkEventCrossing *ev)
{
	if (ev->detail != GDK_NOTIFY_INFERIOR) {
		Keyboard::the_keyboard().set_current_dialog (this);
	}
	return FALSE;
}

gint
ArdourDialog::on_leave_notify_event (GdkEventCrossing *ev)
{
	if (ev->detail != GDK_NOTIFY_INFERIOR) {
		Keyboard::the_keyboard().set_current_dialog (0);
	}
	return FALSE;
}

gint
ArdourDialog:on_unmap (GdkEventAny *ev)
{
	_within_hiding = true;
	 Hiding (); /* EMIT_SIGNAL */
	_within_hiding = false;
	return Gtk::Window::on_unmap (ev);
}

void
ArdourDialog::wm_close()
{
	stop (-1);
	ARDOUR_UI::instance()->allow_focus(false);
}

void
ArdourDialog::wm_doi ()
{
	if (!hide_on_stop) {
		Hiding (); /* EMIT_SIGNAL */
	}
	stop (-1);
	delete_when_idle (this);
}

gint
ArdourDialog::wm_close_event (GdkEventAny* ev)
{
	wm_close ();
	return TRUE;
}

gint
ArdourDialog::wm_doi_event (GdkEventAny* ev)
{
	wm_doi ();
	return TRUE;
}

gint
ArdourDialog::wm_doi_event_stop (GdkEventAny* ev)
{
	stop (-1);
	return TRUE;
}

void
ArdourDialog::set_hide_on_stop (bool yn)
{
	hide_on_stop = yn;
}

void
ArdourDialog::close ()
{
	hide_all ();

	if (kbd_input) {
		ARDOUR_UI::instance()->allow_focus (false);
	}
}

void
ArdourDialog::stop (int rr)
{
	_run_status = rr;

	if (hide_on_stop) {
		Hiding (); /* EMIT_SIGNAL */
		hide_all ();

		if (kbd_input) {
			ARDOUR_UI::instance()->allow_focus (false);
		}
	}

	if (running) {
		Gtk::Main::quit ();
		running = false;
	}
}

void
ArdourDialog::run ()
{
	show_all ();

	if (kbd_input) {
		ARDOUR_UI::instance()->allow_focus (true);
	}

	running = true;
	Gtk::Main::run ();
}

void
ArdourDialog::set_keyboard_input (bool yn)
{
	kbd_input = yn;
}

int
ArdourDialog::run_status ()
{
	return _run_status;
}
