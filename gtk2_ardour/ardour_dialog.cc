/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Nick Mainsbridge <mainsbridge@gmail.com>
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
#include <sigc++/bind.h>

#include <gtkmm2ext/doi.h>

#include "ardour_dialog.h"
#include "ardour_ui.h"
#include "keyboard.h"
#include "splash.h"
#include "ui_config.h"
#include "utils.h"
#include "window_manager.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR_UI_UTILS;

ArdourDialog::ArdourDialog (string title, bool modal, bool use_seperator)
	: Dialog (title, modal, use_seperator)
	, _sensitive (true)
	, proxy (0)
	, _splash_pushed (false)
{
	init ();
	set_position (Gtk::WIN_POS_MOUSE);
}

ArdourDialog::ArdourDialog (Gtk::Window& parent, string title, bool modal, bool use_seperator)
	: Dialog (title, parent, modal, use_seperator)
	, _sensitive (true)
	, proxy (0)
	, _splash_pushed (false)
{
	init ();
	set_position (Gtk::WIN_POS_CENTER_ON_PARENT);
}

ArdourDialog::~ArdourDialog ()
{
	pop_splash ();
	Keyboard::the_keyboard().focus_out_window (0, this);
	WM::Manager::instance().remove (proxy);
	proxy->explicit_delete ();
}

void
ArdourDialog::on_response (int response_id)
{
	pop_splash ();
	hide ();
	ARDOUR::GUIIdle ();
	Gtk::Dialog::on_response (response_id);
}

void
ArdourDialog::close_self ()
{
	/* Don't call Idle, don't pop splash.
	 * This is used at exit and session-close and invoked
	 * via close_all_dialogs.
	 */
	hide ();
	Gtk::Dialog::on_response (RESPONSE_CANCEL);
}

void
ArdourDialog::pop_splash ()
{
	if (_splash_pushed) {
		Splash* spl = Splash::exists () ? Splash::instance() : NULL;

		if (spl) {
			spl->pop_front_for (*this);
		}
		_splash_pushed = false;
	}
}

bool
ArdourDialog::on_focus_in_event (GdkEventFocus *ev)
{
	Keyboard::the_keyboard().focus_in_window (ev, this);
	return Dialog::on_focus_in_event (ev);
}

bool
ArdourDialog::on_focus_out_event (GdkEventFocus *ev)
{
	if (!get_modal()) {
		Keyboard::the_keyboard().focus_out_window (ev, this);
	}
	return Dialog::on_focus_out_event (ev);
}

void
ArdourDialog::on_unmap ()
{
	Keyboard::the_keyboard().leave_window (0, this);
	pop_splash ();
	Dialog::on_unmap ();
}

void
ArdourDialog::on_show ()
{
	Dialog::on_show ();

	// never allow the splash screen to obscure any dialog

	if (Splash::exists()) {
		Splash* spl = Splash::instance();

		spl->pop_back_for (*this);
		_splash_pushed = true;
	}

	_sensitive = true;
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
	add_events (Gdk::FOCUS_CHANGE_MASK);

#ifdef __APPLE__
	set_type_hint (Gdk::WINDOW_TYPE_HINT_DIALOG);
#else
	if (UIConfiguration::instance().get_all_floating_windows_are_dialogs () || get_modal ()) {
		set_type_hint (Gdk::WINDOW_TYPE_HINT_DIALOG);
	} else {
		set_type_hint (Gdk::WINDOW_TYPE_HINT_UTILITY);
	}
#endif

	Gtk::Window* parent = WM::Manager::instance().transient_parent();

	if (parent) {
		set_transient_for (*parent);
	}

	ARDOUR_UI::CloseAllDialogs.connect (sigc::mem_fun (*this, &ArdourDialog::close_self)); /* send a RESPONSE_CANCEL to self */

	proxy = new WM::ProxyTemporary (get_title(), this);
	WM::Manager::instance().register_window (proxy);
}

void
ArdourDialog::set_ui_sensitive (bool yn)
{
	_sensitive = yn;
}
