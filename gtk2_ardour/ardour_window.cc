/*
    Copyright (C) 2002-2011 Paul Davis

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
#include <sigc++/bind.h>

#include <gtkmm2ext/doi.h>

#include "ardour_window.h"
#include "ardour_ui.h"
#include "keyboard.h"
#include "utils.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

ArdourWindow::ArdourWindow (string title)
	: Window ()
	, VisibilityTracker (*((Gtk::Window*)this))
{
	set_title (title);
	init ();
	set_position (Gtk::WIN_POS_MOUSE);
}

ArdourWindow::ArdourWindow (Gtk::Window& parent, string /*title*/)
	: Window ()
	, VisibilityTracker (*((Gtk::Window*)this))
{
	init ();
	set_transient_for (parent);
	set_position (Gtk::WIN_POS_CENTER_ON_PARENT);
}

ArdourWindow::~ArdourWindow ()
{
}

bool
ArdourWindow::on_key_press_event (GdkEventKey* ev)
{
	return relay_key_press (ev, this);
}

bool
ArdourWindow::on_enter_notify_event (GdkEventCrossing *ev)
{
	Keyboard::the_keyboard().enter_window (ev, this);
	return Window::on_enter_notify_event (ev);
}

bool
ArdourWindow::on_leave_notify_event (GdkEventCrossing *ev)
{
	Keyboard::the_keyboard().leave_window (ev, this);
	return Window::on_leave_notify_event (ev);
}

void
ArdourWindow::on_unmap ()
{
	Keyboard::the_keyboard().leave_window (0, this);
	Window::on_unmap ();
}

bool
ArdourWindow::on_delete_event (GdkEventAny*)
{
	hide ();
	return false;
}

void
ArdourWindow::init ()
{
	set_border_width (10);

        /* ArdourWindows are not dialogs (they have no "OK" or "Close" button) but
           they should be considered part of the same "window level" as a dialog. This
           works on X11 and Quartz, in that:
           
           (a) utility & dialog windows are considered to be part of the same level
           (b) they will float above normal windows without any particular effort
	   (c) present()-ing them will make a utility float over a dialog or
               vice versa.
        */

        set_type_hint (Gdk::WINDOW_TYPE_HINT_UTILITY);

	ARDOUR_UI::CloseAllDialogs.connect (sigc::mem_fun (*this, &ArdourWindow::hide));
}

