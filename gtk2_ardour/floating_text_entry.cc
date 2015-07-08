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

#include "pbd/stacktrace.h"
#include "public_editor.h"


#include "floating_text_entry.h"
#include "gtkmm2ext/doi.h"
#include "gtkmm2ext/utils.h"

#include "i18n.h"

FloatingTextEntry::FloatingTextEntry (Gtk::Window* parent, const std::string& initial_contents)
	: Gtk::Window (Gtk::WINDOW_POPUP)
        , entry_changed (false)
{
	set_name (X_("FloatingTextEntry"));
	set_position (Gtk::WIN_POS_MOUSE);
	set_border_width (0);

	if (!initial_contents.empty()) {
		entry.set_text (initial_contents);
	}

	entry.show ();
	entry.signal_changed().connect (sigc::mem_fun (*this, &FloatingTextEntry::changed));
	entry.signal_activate().connect (sigc::mem_fun (*this, &FloatingTextEntry::activated));
	entry.signal_key_press_event().connect (sigc::mem_fun (*this, &FloatingTextEntry::key_press));
	entry.signal_button_press_event().connect (sigc::mem_fun (*this, &FloatingTextEntry::button_press));

	if (parent) {
		parent->signal_focus_out_event().connect (sigc::mem_fun (*this, &FloatingTextEntry::entry_focus_out));
	}

	add (entry);
}

void
FloatingTextEntry::changed ()
{
	entry_changed = true;
}

void
FloatingTextEntry::on_realize ()
{
	Gtk::Window::on_realize ();
	get_window()->set_decorations (Gdk::WMDecoration (0));
	entry.add_modal_grab ();
}

bool
FloatingTextEntry::entry_focus_out (GdkEventFocus* ev)
{
	entry.remove_modal_grab ();
	if (entry_changed) {
		use_text (entry.get_text ());
	}

	delete_when_idle ( this);
	return false;
}

bool
FloatingTextEntry::button_press (GdkEventButton* ev)
{
	if (Gtkmm2ext::event_inside_widget_window (*this, (GdkEvent*) ev)) {
		return true;
	}

	/* Clicked outside widget window - edit is done */
	entry.remove_modal_grab ();

	/* arrange re-propagation of the event once we go idle */
	Glib::signal_idle().connect (sigc::bind_return (sigc::bind (sigc::ptr_fun (gtk_main_do_event), gdk_event_copy ((GdkEvent*) ev)), false));

	if (entry_changed) {
		use_text (entry.get_text ());
	}

	delete_when_idle ( this);

	return false;
}

void
FloatingTextEntry::activated ()
{
	use_text (entry.get_text()); // EMIT SIGNAL
	delete_when_idle (this);
}

bool
FloatingTextEntry::key_press (GdkEventKey* ev)
{
	switch (ev->keyval) {
	case GDK_Escape:
		delete_when_idle (this);
		return true;
		break;
	default:
		break;
	}
	return false;
}

void
FloatingTextEntry::on_hide ()
{
	entry.remove_modal_grab ();

	/* No hide button is shown (no decoration on the window),
	   so being hidden is equivalent to the Escape key or any other
	   method of cancelling the edit.
	*/

	delete_when_idle (this);
	Gtk::Window::on_hide ();
}
