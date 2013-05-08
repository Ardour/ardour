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
#include <sigc++/bind.h>

#include <gtkmm2ext/doi.h>

#include "ardour_dialog.h"
#include "ardour_ui.h"
#include "keyboard.h"
#include "splash.h"
#include "utils.h"
#include "window_manager.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

ArdourDialog::ArdourDialog (string title, bool modal, bool use_seperator)
	: Dialog (title, modal, use_seperator)
	, proxy (0)
        , _splash_pushed (false)
{
	init ();
	set_position (Gtk::WIN_POS_MOUSE);
}

ArdourDialog::ArdourDialog (Gtk::Window& parent, string title, bool modal, bool use_seperator)
	: Dialog (title, parent, modal, use_seperator)
        , _splash_pushed (false)
{
	init ();
	set_position (Gtk::WIN_POS_CENTER_ON_PARENT);
}

ArdourDialog::~ArdourDialog ()
{
        if (_splash_pushed) {
                Splash* spl = Splash::instance();
                
                if (spl) {
                        spl->pop_front();
                }
        }
	WM::Manager::instance().remove (proxy);
}

bool
ArdourDialog::on_key_press_event (GdkEventKey* ev)
{
	return relay_key_press (ev, this);
}

bool
ArdourDialog::on_enter_notify_event (GdkEventCrossing *ev)
{
	Keyboard::the_keyboard().enter_window (ev, this);
	return Dialog::on_enter_notify_event (ev);
}

bool
ArdourDialog::on_leave_notify_event (GdkEventCrossing *ev)
{
	Keyboard::the_keyboard().leave_window (ev, this);
	return Dialog::on_leave_notify_event (ev);
}

void
ArdourDialog::on_unmap ()
{
	Keyboard::the_keyboard().leave_window (0, this);
	Dialog::on_unmap ();
}

void
ArdourDialog::on_show ()
{
	Dialog::on_show ();

	// never allow the splash screen to obscure any dialog

	Splash* spl = Splash::instance();

	if (spl && spl->is_visible()) {
		spl->pop_back_for (*this);
                _splash_pushed = true;
	}
}

bool
ArdourDialog::on_delete_event (GdkEventAny*)
{
	hide ();
	return false;
}

void
ArdourDialog::init ()
{
	set_border_width (10);

	set_type_hint (Gdk::WINDOW_TYPE_HINT_DIALOG);

	Gtk::Window* parent = WM::Manager::instance().transient_parent();

	if (parent) {
		set_transient_for (*parent);
	}

	ARDOUR_UI::CloseAllDialogs.connect (sigc::bind (sigc::mem_fun (*this, &ArdourDialog::response), RESPONSE_CANCEL));

	proxy = new WM::ProxyTemporary (get_title(), this);
	WM::Manager::instance().register_window (proxy);
}
