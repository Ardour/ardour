/*
 * Copyright (C) 2024 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_track.h"
#include "ardour/triggerbox.h"

#include "gtkmm2ext/utils.h"

#include "canvas/debug.h"

#include "editing_context.h"
#include "editor_drag.h"
#include "hit.h"
#include "keyboard.h"
#include "mergeable_line.h"
#include "midi_cue_automation_line.h"
#include "midi_cue_view.h"
#include "midi_cue_velocity.h"
#include "note.h"
#include "ui_config.h"
#include "velocity_display.h"

#include "pbd/i18n.h"

using namespace Gtkmm2ext;

MidiCueView::MidiCueView (std::shared_ptr<ARDOUR::MidiTrack> mt,
                          uint32_t                 slot_index,
                          ArdourCanvas::Item&      parent,
                          EditingContext&          ec,
                          MidiViewBackground&      bg,
                          uint32_t                 basic_color)
	: MidiView (mt, parent, ec, bg, basic_color)
	, velocity_base (nullptr)
	, velocity_display (nullptr)
	, _slot_index (slot_index)
{
	CANVAS_DEBUG_NAME (_note_group, X_("note group for MIDI cue"));

	_needs_active_notes_for_rec_enabled_track = true;

	/* Containers don't get canvas events, so we need an invisible rect
	 * that will. It will be resized as needed sothat it always covers the
	 * entire canvas/view.
	 */

	event_rect = new ArdourCanvas::Rectangle (&parent);
	event_rect->set (ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, 10.));
	event_rect->Event.connect (sigc::mem_fun (*this, &MidiCueView::canvas_event));
	event_rect->set_fill (false);
	event_rect->set_outline (false);
	CANVAS_DEBUG_NAME (event_rect, "cue event rect");

	_note_group->raise_to_top ();

	automation_group = new ArdourCanvas::Rectangle (&parent);
	CANVAS_DEBUG_NAME (automation_group, "cue automation group");
	automation_group->set_fill_color (UIConfiguration::instance().color ("midi automation track fill"));
	automation_group->set_data ("linemerger", this);

	velocity_base = new ArdourCanvas::Rectangle (&parent);
	CANVAS_DEBUG_NAME (velocity_base, "cue velocity base");
	velocity_display = new MidiCueVelocityDisplay (editing_context(), midi_context(), *this, *velocity_base, 0x312244ff);

	for (auto & ev : _events) {
		velocity_display->add_note (ev.second);
	}

	set_extensible (true);

	Evoral::Parameter fully_qualified_param (ARDOUR::MidiCCAutomation, 0, MIDI_CTL_MSB_MODWHEEL);
	show_automation (fully_qualified_param);
}

void
MidiCueView::set_height (double h)
{
	double note_area_height = ceil (h / 2.);
	double velocity_height = ceil ((h - note_area_height) / 2.);
	double automation_height = h - note_area_height - velocity_height;

	event_rect->set (ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, note_area_height));
	midi_context().set_size (midi_context().width(), note_area_height);

	velocity_base->set_position (ArdourCanvas::Duple (0., note_area_height));
	velocity_base->set (ArdourCanvas::Rect (0., 0., ArdourCanvas::COORD_MAX, velocity_height));

	automation_group->set_position (ArdourCanvas::Duple (0., note_area_height + velocity_height));
	automation_group->set (ArdourCanvas::Rect (0., 0., ArdourCanvas::COORD_MAX, automation_height));

	if (automation_line) {
		automation_line->set_height (automation_height);
	}

	view_changed ();
}

ArdourCanvas::Item*
MidiCueView::drag_group () const
{
	return event_rect;
}

bool
MidiCueView::canvas_event (GdkEvent* ev)
{
	return MidiView::canvas_group_event (ev);
}

bool
MidiCueView::scroll (GdkEventScroll* ev)
{
	if (_editing_context.drags()->active()) {
		return false;
	}

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier) ||
	    Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {

		switch (ev->direction) {
		case GDK_SCROLL_UP:
			_editing_context.reset_zoom (_editing_context.get_current_zoom() / 2);
			return true;
		case GDK_SCROLL_DOWN:
			_editing_context.reset_zoom (_editing_context.get_current_zoom() * 2);
			return true;
		default:
			return false;
		}
	}

	return MidiView::scroll (ev);
}

void
MidiCueView::set_samples_per_pixel (double spp)
{
	std::shared_ptr<Temporal::TempoMap> map;
	Temporal::timecnt_t duration;

	if (_midi_region) {
		duration = Temporal::timecnt_t (_midi_region->midi_source()->length().beats());
		map.reset (new Temporal::TempoMap (Temporal::Tempo (120, 4), Temporal::Meter (4, 4)));
	} else {
		duration = Temporal::timecnt_t (Temporal::Beats (4, 0));
		map.reset (new Temporal::TempoMap (Temporal::Tempo (120, 4), Temporal::Meter (4, 4)));
	}

	EditingContext::TempoMapScope tms (_editing_context, map);

	reset_width_dependent_items (_editing_context.duration_to_pixels (duration));
}

void
MidiCueView::clear_ghost_events ()
{
	if (velocity_display) {
		velocity_display->clear ();
	}
}

void
MidiCueView::ghosts_model_changed ()
{
	if (velocity_display) {
		velocity_display->clear ();
		for (auto & ev : _events) {
			velocity_display->add_note (ev.second);
		}
	}
}

void
MidiCueView::ghosts_view_changed ()
{
	if (velocity_display) {
		velocity_display->redisplay();
	}
}

void
MidiCueView::ghost_remove_note (NoteBase* nb)
{
	if (velocity_display) {
		velocity_display->remove_note (nb);
	}
}

void
MidiCueView::ghost_add_note (NoteBase* nb)
{
	if (velocity_display) {
		velocity_display->add_note (nb);
	}
}

void
MidiCueView::ghost_sync_selection (NoteBase* nb)
{
	if (velocity_display) {
		velocity_display->note_selected (nb);
	}
}

void
MidiCueView::update_sustained (Note* n)
{
	MidiView::update_sustained (n);
	if (velocity_display) {
		velocity_display->update_note (n);
	}
}

void
MidiCueView::update_hit (Hit* h)
{
	MidiView::update_hit (h);
	if (velocity_display) {
		velocity_display->update_note (h);
	}
}

void
MidiCueView::show_automation (Evoral::Parameter const & param)
{
	using namespace ARDOUR;

	if (!_midi_region) {
		return;
	}

	if (param.type() == NullAutomation) {
		return;
	}

//	if (automation_line && automation_line->param() == param) {
//		return;
//	}

	automation_line.reset ();
	automation_control.reset ();

	std::shared_ptr<AutomationControl> control;

	switch (param.type()) {

	case MidiCCAutomation:
	case MidiPgmChangeAutomation:
	case MidiPitchBenderAutomation:
	case MidiChannelPressureAutomation:
	case MidiNotePressureAutomation:
	case MidiSystemExclusiveAutomation: {
		/* These controllers are region "automation" - they are owned
		 * by regions (and their MidiModels), not by the track. As a
		 * result there is no AutomationList/Line for the track, but we create
		 * a controller for the user to write immediate events, so the editor
		 * can act as a control surface for the present MIDI controllers.
		 *
		 * TODO: Record manipulation of the controller to regions?
		 */

		std::shared_ptr<Evoral::Control> control = _midi_region->model()->control (param, true);
		automation_control = std::dynamic_pointer_cast<AutomationControl> (control);

		if (automation_control) {
			automation_line.reset (new MidiCueAutomationLine ("whatevs",
			                                                  _editing_context,
			                                                  *automation_group,
			                                                  automation_group,
			                                                  automation_control->alist(),
			                                                  automation_control->desc()));
			automation_line->set_height (automation_group->get().height());
		}
		break;
	}

	default:
		break;
	}
}

std::list<SelectableOwner*>
MidiCueView::selectable_owners()
{
	std::list<SelectableOwner*> sl;
	if (automation_line) {
		sl.push_back (automation_line.get());
	}
	return sl;
}

MergeableLine*
MidiCueView::make_merger ()
{
	return new MergeableLine (automation_line, automation_control,
	                          [](Temporal::timepos_t const& t) { return t; },
	                          nullptr, nullptr);
}

bool
MidiCueView::automation_rb_click (GdkEvent* event, Temporal::timepos_t const & pos)
{
	bool with_guard_points = Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier);
	automation_line->add (automation_control, event, pos, event->button.y, with_guard_points);
	return false;
}

void
MidiCueView::line_drag_click (GdkEvent* event, Temporal::timepos_t const & pos)
{
}


