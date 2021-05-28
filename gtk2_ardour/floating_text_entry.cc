/*
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include "gtkmm2ext/doi.h"
#include "gtkmm2ext/utils.h"

#include "floating_text_entry.h"
#include "public_editor.h"

#include "pbd/i18n.h"

FloatingTextEntry::FloatingTextEntry (Gtk::Window* parent, const std::string& initial_contents)
	: Gtk::Window ()
	, entry_changed (false)
{
	//set_name (X_("FloatingTextEntry"));
	set_position (Gtk::WIN_POS_MOUSE);
	set_border_width (0);
	set_type_hint(Gdk::WINDOW_TYPE_HINT_POPUP_MENU);
	set_resizable (false);

	if (!initial_contents.empty()) {
		entry.set_text (initial_contents);
	}

	entry.show ();
	_connections.push_back (entry.signal_changed().connect (sigc::mem_fun (*this, &FloatingTextEntry::changed)));
	_connections.push_back (entry.signal_activate().connect (sigc::mem_fun (*this, &FloatingTextEntry::activated)));
	_connections.push_back (entry.signal_key_press_event().connect (sigc::mem_fun (*this, &FloatingTextEntry::key_press), false));
	_connections.push_back (entry.signal_key_release_event().connect (sigc::mem_fun (*this, &FloatingTextEntry::key_release), false));
	_connections.push_back (entry.signal_button_press_event().connect (sigc::mem_fun (*this, &FloatingTextEntry::button_press)));

	entry.select_region (0, -1);

	if (parent) {
		set_transient_for (*parent);
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
	set_keep_above (true);
	entry.add_modal_grab ();
}

bool
FloatingTextEntry::entry_focus_out (GdkEventFocus* ev)
{
	entry.remove_modal_grab ();
	if (entry_changed) {
		disconect_signals ();
		use_text (entry.get_text (), 0); /* EMIT SIGNAL */
	}

	idle_delete_self ();
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
		disconect_signals ();
		use_text (entry.get_text (), 0); /* EMIT SIGNAL */
	}

	idle_delete_self ();

	return false;
}

void
FloatingTextEntry::activated ()
{
	disconect_signals ();
	use_text (entry.get_text(), 0); // EMIT SIGNAL
	idle_delete_self ();
}

bool
FloatingTextEntry::key_press (GdkEventKey* ev)
{
	/* steal escape, tabs from GTK */

	switch (ev->keyval) {
	case GDK_Escape:
	case GDK_ISO_Left_Tab:
	case GDK_Tab:
		return true;
	}
	return false;
}

bool
FloatingTextEntry::key_release (GdkEventKey* ev)
{
	switch (ev->keyval) {
	case GDK_Escape:
		/* cancel edit */
		idle_delete_self ();
		return true;

	case GDK_ISO_Left_Tab:
		/* Shift+Tab Keys Pressed. Note that for Shift+Tab, GDK actually
		 * generates a different ev->keyval, rather than setting
		 * ev->state.
		 */
		disconect_signals ();
		use_text (entry.get_text(), -1); // EMIT SIGNAL, move to prev
		idle_delete_self ();
		return true;

	case GDK_Tab:
		disconect_signals ();
		use_text (entry.get_text(), 1); // EMIT SIGNAL, move to next
		idle_delete_self ();
		return true;
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
	 * so being hidden is equivalent to the Escape key or any other
	 * method of cancelling the edit.
	 *
	 * This is also used during disconect_signals() before calling
	 * use_text (). see note below.
	 *
	 * If signals are already disconnected, idle-delete must be
	 * in progress already.
	 */
	if (!_connections.empty ()) {
		idle_delete_self ();
	}
	Gtk::Window::on_hide ();
}

void
FloatingTextEntry::disconect_signals ()
{
	for (std::list<sigc::connection>::iterator i = _connections.begin(); i != _connections.end(); ++i) {
		i->disconnect ();
	}
	 _connections.clear ();
	/* the entry is floating on-top, emitting use_text()
	 * may result in another dialog being shown (cannot rename track)
	 * which would
	 *  - be stacked below the floating text entry
	 *  - return focus to the entry when closedA
	 * so we hide the entry here.
	 */
	hide ();
}

void
FloatingTextEntry::idle_delete_self ()
{
	disconect_signals ();
	delete_when_idle (this);
}
