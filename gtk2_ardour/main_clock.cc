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

#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "actions.h"
#include "main_clock.h"
#include "public_editor.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;

MainClock::MainClock (
	const std::string& clock_name,
	const std::string& widget_name
	)
	: AudioClock (clock_name, false, widget_name, true, true, false, true)
	, _suspend_delta_mode_signal (false)
	, _widget_name(widget_name)
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
	ops_items.push_back (RadioMenuElem (group, _("Display absolute time"), sigc::bind (sigc::mem_fun (*this, &MainClock::change_display_delta_mode), NoDelta)));
	if (_delta_mode == NoDelta) {
		RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&ops_items.back ());
		i->set_active (true);
	}
	ops_items.push_back (RadioMenuElem (group, _("Display delta to edit cursor"), sigc::bind (sigc::mem_fun (*this, &MainClock::change_display_delta_mode), DeltaEditPoint)));
	if (_delta_mode == DeltaEditPoint) {
		RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&ops_items.back ());
		i->set_active (true);
	}
	ops_items.push_back (RadioMenuElem (group, _("Display delta to origin marker"), sigc::bind (sigc::mem_fun (*this, &MainClock::change_display_delta_mode), DeltaOriginMarker)));
	if (_delta_mode == DeltaOriginMarker) {
		RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&ops_items.back ());
		i->set_active (true);
	}

	ops_items.push_back (SeparatorElem());

	ops_items.push_back (MenuElem (_("Edit Tempo"), sigc::mem_fun(*this, &MainClock::edit_current_tempo)));
	ops_items.push_back (MenuElem (_("Edit Time Signature"), sigc::mem_fun(*this, &MainClock::edit_current_meter)));
	ops_items.push_back (MenuElem (_("Insert Tempo Change"), sigc::mem_fun(*this, &MainClock::insert_new_tempo)));
	ops_items.push_back (MenuElem (_("Insert Time Signature Change"), sigc::mem_fun(*this, &MainClock::insert_new_meter)));
}

void
MainClock::set (timepos_t const & when, bool force)
{
	if (!AudioEngine::instance()->session()) {
		_delta_mode = NoDelta;
	}

	switch (_delta_mode) {
		case NoDelta:
			AudioClock::set (when, force);
			break;
		case DeltaEditPoint:
			set_duration (when.distance (PublicEditor::instance().get_preferred_edit_position (Editing::EDIT_IGNORE_PHEAD)), force);
			break;
		case DeltaOriginMarker:
			{
				Location* loc = AudioEngine::instance()->session()->locations()->clock_origin_location ();
				set_duration (-when.distance (loc ? loc->start() : timepos_t (when.time_domain())), force);
			}
			break;
	}
}

void
MainClock::change_display_delta_mode (ClockDeltaMode m)
{
	if (_suspend_delta_mode_signal) {
		return;
	}
	change_display_delta_mode_signal (m);
}

void
MainClock::set_display_delta_mode (ClockDeltaMode m)
{
	_delta_mode = m;
	if (_delta_mode != NoDelta) {
		set_editable (false);
		set_widget_name (_widget_name + " delta");
	} else {
		set_editable (true);
		set_widget_name (_widget_name);
	}
}

void
MainClock::edit_current_tempo ()
{
	if (!PublicEditor::instance().session()) return;
	PublicEditor::instance().edit_tempo_section (Temporal::TempoMap::use()->metric_at (last_when()).get_editable_tempo());
}

void
MainClock::edit_current_meter ()
{
	if (!PublicEditor::instance().session()) return;
	PublicEditor::instance().edit_meter_section (Temporal::TempoMap::use()->metric_at (last_when()).get_editable_meter());
}

void
MainClock::insert_new_tempo ()
{
	PublicEditor::instance().mouse_add_new_tempo_event (last_when ());
}

void
MainClock::insert_new_meter ()
{
	PublicEditor::instance().mouse_add_new_meter_event (last_when ());
}
