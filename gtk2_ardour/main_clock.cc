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

#include "ardour_ui.h"
#include "main_clock.h"
#include "public_editor.h"

#include "i18n.h"

#include "ardour/tempo.h"

using namespace Gtk;

MainClock::MainClock (
	const std::string& clock_name,
	const std::string& widget_name,
	bool primary
	)
	: AudioClock (clock_name, false, widget_name, true, true, false, true)
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
		if (ARDOUR_UI::config()->get_primary_clock_delta_edit_cursor ()) {
			ARDOUR_UI::config()->set_primary_clock_delta_edit_cursor (false);
			c->set_active (true);
		}
	} else {
		if (ARDOUR_UI::config()->get_secondary_clock_delta_edit_cursor ()) {
			ARDOUR_UI::config()->set_secondary_clock_delta_edit_cursor (false);
			c->set_active (true);
		}
	}

	ops_items.push_back (SeparatorElem());
	ops_items.push_back (MenuElem (_("Edit Tempo"), sigc::mem_fun(*this, &MainClock::edit_current_tempo)));
	ops_items.push_back (MenuElem (_("Edit Meter"), sigc::mem_fun(*this, &MainClock::edit_current_meter)));
	ops_items.push_back (MenuElem (_("Insert Tempo Change"), sigc::mem_fun(*this, &MainClock::insert_new_tempo)));
	ops_items.push_back (MenuElem (_("Insert Meter Change"), sigc::mem_fun(*this, &MainClock::insert_new_meter)));
}

framepos_t
MainClock::absolute_time () const
{
	if (get_is_duration ()) {
		// delta to edit cursor
		return current_time () + PublicEditor::instance().get_preferred_edit_position (true);
	} else {
		return current_time ();
	}
}

void
MainClock::display_delta_to_edit_cursor ()
{
	if (_primary) {
		ARDOUR_UI::config()->set_primary_clock_delta_edit_cursor (!ARDOUR_UI::config()->get_primary_clock_delta_edit_cursor ());
	} else {
		ARDOUR_UI::config()->set_secondary_clock_delta_edit_cursor (!ARDOUR_UI::config()->get_secondary_clock_delta_edit_cursor ());
	}
}

void
MainClock::edit_current_tempo ()
{
	ARDOUR::TempoSection ts = PublicEditor::instance().session()->tempo_map().tempo_section_at (absolute_time());
	PublicEditor::instance().edit_tempo_section (&ts);
}

void
MainClock::edit_current_meter ()
{
	ARDOUR::MeterSection ms = PublicEditor::instance().session()->tempo_map().meter_section_at (absolute_time());
	PublicEditor::instance().edit_meter_section (&ms);
}

void
MainClock::insert_new_tempo ()
{
	PublicEditor::instance().mouse_add_new_tempo_event (absolute_time ());
}

void
MainClock::insert_new_meter ()
{
	PublicEditor::instance().mouse_add_new_meter_event (absolute_time ());
}

bool
MainClock::on_button_press_event (GdkEventButton *ev)
{
	if (ev->button == 1) {
		if (mode() == BBT) {
			if (is_lower_layout_click(ev->y)) {
				if (is_right_layout_click(ev->x)) {
					// meter on the right
					edit_current_meter();
				} else {
					// tempo on the left
					edit_current_tempo();
				}
				return true;
			}
		}
	}

	return AudioClock::on_button_press_event (ev);
}

