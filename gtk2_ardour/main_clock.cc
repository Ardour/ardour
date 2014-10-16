/*
    Copyright (C) 2012 Paul Davis

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

#include "ardour/rc_configuration.h"
#include "main_clock.h"
#include "i18n.h"

using namespace Gtk;

MainClock::MainClock (
	const std::string& clock_name,
	bool is_transient,
	const std::string& widget_name,
	bool editable,
	bool follows_playhead,
	bool primary,
	bool duration,
	bool with_info
	)
	: AudioClock (clock_name, is_transient, widget_name, editable, follows_playhead, duration, with_info)
	  , _primary (primary)
{

}

void
MainClock::build_ops_menu ()
{
	using namespace Menu_Helpers;

	AudioClock::build_ops_menu ();

	MenuList& ops_items = ops_menu->items();
	ops_items.push_back (SeparatorElem ());
	ops_items.push_back (CheckMenuElem (_("Display delta to edit cursor"), sigc::mem_fun (*this, &MainClock::display_delta_to_edit_cursor)));
	Gtk::CheckMenuItem* c = dynamic_cast<Gtk::CheckMenuItem *> (&ops_items.back());
	if (_primary) {
		if (ARDOUR::Config->get_primary_clock_delta_edit_cursor ()) {
			ARDOUR::Config->set_primary_clock_delta_edit_cursor (false);
			c->set_active (true);
		}
	} else {
		if (ARDOUR::Config->get_secondary_clock_delta_edit_cursor ()) {
			ARDOUR::Config->set_secondary_clock_delta_edit_cursor (false);
			c->set_active (true);
		}
	}
}

void
MainClock::display_delta_to_edit_cursor ()
{
	if (_primary) {
		ARDOUR::Config->set_primary_clock_delta_edit_cursor (!ARDOUR::Config->get_primary_clock_delta_edit_cursor ());
	} else {
		ARDOUR::Config->set_secondary_clock_delta_edit_cursor (!ARDOUR::Config->get_secondary_clock_delta_edit_cursor ());
	}
}
