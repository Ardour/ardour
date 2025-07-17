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

#include "gtkmm2ext/colors.h"

#include "actions.h"
#include "main_clock.h"
#include "public_editor.h"
#include "ardour_ui.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace std;

MainClock::MainClock (
	const std::string& clock_name,
	const std::string& widget_name,
	ClockDisposition d
	)
	: AudioClock (clock_name, false, widget_name, true, true, false, true)
	, _disposition(d)
	, _suspend_delta_mode_signal (false)
	, _widget_name(widget_name)
	, _delta_mode (NoDelta)
	, _layout_width (0)
	, _layout_height (0)
{
	ValueChanged.connect (sigc::mem_fun(*this, &MainClock::clock_value_changed));

	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &MainClock::parameter_changed));
	std::function<void (std::string)> pc (std::bind (&MainClock::parameter_changed, this, _1));
	UIConfiguration::instance().map_parameters (pc);

	_layout = Pango::Layout::create (get_pango_context());
	_layout->set_text ("\u0394");
}

void
MainClock::set_session (ARDOUR::Session *s)
{
	AudioClock::set_session (s);
	_left_btn.set_related_action (ActionManager::get_action (X_("Editor"), X_("edit-current-tempo")));
	_right_btn.set_related_action (ActionManager::get_action (X_("Editor"), X_("edit-current-meter")));
}

void
MainClock::parameter_changed (std::string p)
{
	if (p == "primary-clock-delta-mode" && _disposition == PrimaryClock) {
		set_display_delta_mode(UIConfiguration::instance().get_primary_clock_delta_mode());
	} else if (p == "secondary-clock-delta-mode" && _disposition == SecondaryClock) {
		set_display_delta_mode(UIConfiguration::instance().get_secondary_clock_delta_mode());
	}
}

void
MainClock::clock_value_changed ()
{
	if (_session) {
		_session->request_locate (last_when ().samples());
	}
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
MainClock::set (timepos_t const & when, bool force, bool round_to_beat)
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

	CanonicalClockChanged ();  //signal
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
	if (_delta_mode == m) {
		return;
	}
	_delta_mode = m;
	if (_delta_mode != NoDelta) {
		set_editable (false);
		set_widget_name (_widget_name + " delta");
	} else {
		set_editable (true);
		set_widget_name (_widget_name);
	}

	if (_session) {
		set(timepos_t(_session->audible_sample()), true);
	}
}

void
MainClock::on_size_request (Gtk::Requisition* req)
{
	AudioClock::on_size_request (req);

	Glib::RefPtr<Pango::Layout> tmp;
	Glib::RefPtr<Gtk::Style> style = get_style ();
	Pango::FontDescription font;

  if (!get_realized()) {
    font = ARDOUR_UI_UTILS::get_font_for_style (get_name());
  } else {
    font = style->get_font();
  }

  _layout->set_font_description (font);

	_layout->get_pixel_size (_layout_width, _layout_height);

	req->width += _layout_width * 1.5;
	req->height = std::max (req->height, _layout_height);
}

void
MainClock::render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t* rect)
{
	AudioClock::render (ctx, rect);

	if (!_delta_mode) {
		return;
	}

	cairo_t* cr = ctx->cobj();

	uint32_t text_color = UIConfiguration::instance().color (string_compose ("%1: text", get_name()));
	Gtkmm2ext::set_source_rgba (ctx, text_color);
	cairo_move_to (cr, get_width() - _layout_width * 1.5 , (get_height() - _layout_height) / 2.0);
	pango_cairo_show_layout (cr, _layout->gobj());
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


TransportClock::TransportClock (
	const std::string& clock_name,
	const std::string& widget_name,
	ClockDisposition d
	)
	: MainClock (clock_name, widget_name, d)
{
	if (_disposition==PrimaryClock) {
		ARDOUR_UI::instance()->primary_clock->CanonicalClockChanged.connect (sigc::mem_fun(*this, &TransportClock::follow_canonical_clock));
		ARDOUR_UI::instance()->primary_clock->mode_changed.connect (sigc::mem_fun(*this, &TransportClock::follow_canonical_clock));
		change_display_delta_mode_signal.connect (sigc::mem_fun(UIConfiguration::instance(), &UIConfiguration::set_primary_clock_delta_mode));
	} else {
		ARDOUR_UI::instance()->secondary_clock->CanonicalClockChanged.connect (sigc::mem_fun(*this, &TransportClock::follow_canonical_clock));
		ARDOUR_UI::instance()->secondary_clock->mode_changed.connect (sigc::mem_fun(*this, &TransportClock::follow_canonical_clock));
		change_display_delta_mode_signal.connect (sigc::mem_fun(UIConfiguration::instance(), &UIConfiguration::set_secondary_clock_delta_mode));
	}
}

void
TransportClock::set_mode(Mode m)
{
	//we don't set the mode for ourselves; instead we set the canonical clock and then 'follow' it

	if (_disposition==PrimaryClock) {
		ARDOUR_UI::instance()->primary_clock->set_mode(m);
	} else {
		ARDOUR_UI::instance()->secondary_clock->set_mode(m);
	}
}

void
TransportClock::follow_canonical_clock ()
{
	if (!_session) {
		return;
	}

	set(timepos_t(_session->audible_sample()));

	if (_disposition==PrimaryClock) {
		AudioClock::set_mode(ARDOUR_UI::instance()->primary_clock->mode());
		set_display_delta_mode(ARDOUR_UI::instance()->primary_clock->display_delta_mode());
	} else {
		AudioClock::set_mode(ARDOUR_UI::instance()->secondary_clock->mode());
		set_display_delta_mode(ARDOUR_UI::instance()->secondary_clock->display_delta_mode());
	}
}
