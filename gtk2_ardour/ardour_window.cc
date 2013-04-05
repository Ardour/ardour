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
#include "keyboard.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

ArdourWindow::ArdourWindow (string title)
	: Window ()
	, VisibilityTracker (*((Gtk::Window*)this))
{
	set_title (title);
	init ();
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

void
ArdourWindow::init ()
{
	set_border_width (10);
}

