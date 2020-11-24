/*
 * Copyright (C) 2012-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2016 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include "pbd/unwind.h"
#include "ardour/tempo.h"

#include "actions.h"
#include "main_clock.h"
#include "ui_config.h"
#include "public_editor.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;

MainClock::MainClock (
	const std::string& clock_name,
	const std::string& widget_name,
	bool primary
	)
	: AudioClock (clock_name, false, widget_name, true, true, false, true)
	, _primary (primary)
	, _suspend_delta_mode_signal (false)
{
}

void
MainClock::set_session (ARDOUR::Session *s)
{
	AudioClock::set_session (s);
	_left_btn.set_related_action (ActionManager::get_action (X_("Editor"), X_("edit-current-tempo")));
	_right_btn.set_related_action (ActionManager::get_action (X_("Editor"), X_("edit-current-meter")));
}

void
MainClock::build_ops_menu ()
{
	using namespace Menu_Helpers;

	AudioClock::build_ops_menu ();

	MenuList& ops_items = ops_menu->items();
	ops_items.push_back (SeparatorElem ());
	RadioMenuItem::Group group;
	PBD::Unwinder<bool> uw (_suspend_delta_mode_signal, true);
	ClockDeltaMode mode;
	if (_primary) {
		mode = UIConfiguration::instance().get_primary_clock_delta_mode ();
	} else {
		mode = UIConfiguration::instance().get_secondary_clock_delta_mode ();
	}

	ops_items.push_back (RadioMenuElem (group, _("Display absolute time"), sigc::bind (sigc::mem_fun (*this, &MainClock::set_display_delta_mode), NoDelta)));
	if (mode == NoDelta) {
		RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&ops_items.back ());
		i->set_active (true);
	}
	ops_items.push_back (RadioMenuElem (group, _("Display delta to edit cursor"), sigc::bind (sigc::mem_fun (*this, &MainClock::set_display_delta_mode), DeltaEditPoint)));
	if (mode == DeltaEditPoint) {
		RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&ops_items.back ());
		i->set_active (true);
	}
	ops_items.push_back (RadioMenuElem (group, _("Display delta to origin marker"), sigc::bind (sigc::mem_fun (*this, &MainClock::set_display_delta_mode), DeltaOriginMarker)));
	if (mode == DeltaOriginMarker) {
		RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&ops_items.back ());
		i->set_active (true);
	}

	ops_items.push_back (SeparatorElem());

	ops_items.push_back (MenuElem (_("Edit Tempo"), sigc::mem_fun(*this, &MainClock::edit_current_tempo)));
	ops_items.push_back (MenuElem (_("Edit Meter"), sigc::mem_fun(*this, &MainClock::edit_current_meter)));
	ops_items.push_back (MenuElem (_("Insert Tempo Change"), sigc::mem_fun(*this, &MainClock::insert_new_tempo)));
	ops_items.push_back (MenuElem (_("Insert Meter Change"), sigc::mem_fun(*this, &MainClock::insert_new_meter)));
}

timepos_t
MainClock::absolute_time () const
{
	if (get_is_duration ()) {
		return current_time () + offset ();
	} else {
		return current_time ();
	}
}

void
MainClock::set (timepos_t const & when, bool force, timecnt_t const & /*offset*/)
{
	ClockDeltaMode mode;
	if (_primary) {
		mode = UIConfiguration::instance().get_primary_clock_delta_mode ();
	} else {
		mode = UIConfiguration::instance().get_secondary_clock_delta_mode ();
	}
	if (!PublicEditor::instance().session()) {
		mode = NoDelta;
	}

	switch (mode) {
		case NoDelta:
			AudioClock::set (when, force);
			break;
		case DeltaEditPoint:
			AudioClock::set (when, force, timecnt_t (PublicEditor::instance().get_preferred_edit_position (Editing::EDIT_IGNORE_PHEAD)));
			break;
		case DeltaOriginMarker:
			{
				Location* loc = PublicEditor::instance().session()->locations()->clock_origin_location ();
				AudioClock::set (when, force, loc ? timecnt_t (loc->start()) : timecnt_t());
			}
			break;
	}
}

void
MainClock::set_display_delta_mode (ClockDeltaMode m)
{
	if (_suspend_delta_mode_signal) {
		return;
	}
	if (_primary) {
		UIConfiguration::instance().set_primary_clock_delta_mode (m);
	} else {
		UIConfiguration::instance().set_secondary_clock_delta_mode (m);
	}
}

void
MainClock::edit_current_tempo ()
{
	if (!PublicEditor::instance().session()) return;
	PublicEditor::instance().edit_tempo_section (PublicEditor::instance().session()->tempo_map().tempo_at (absolute_time()));
}

void
MainClock::edit_current_meter ()
{
	if (!PublicEditor::instance().session()) return;
	PublicEditor::instance().edit_meter_section (PublicEditor::instance().session()->tempo_map().meter_at (absolute_time()));
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
