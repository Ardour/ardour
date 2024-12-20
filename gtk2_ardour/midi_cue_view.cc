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
#include "ardour/types.h"

#include "gtkmm2ext/utils.h"

#include "canvas/box.h"
#include "canvas/button.h"
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

using namespace ARDOUR;
using namespace Gtkmm2ext;

MidiCueView::MidiCueView (std::shared_ptr<ARDOUR::MidiTrack> mt,
                          ArdourCanvas::Item&      parent,
                          ArdourCanvas::Item&      noscroll_parent,
                          EditingContext&          ec,
                          MidiViewBackground&      bg,
                          uint32_t                 basic_color)
	: MidiView (mt, parent, ec, bg, basic_color)
	, active_automation (nullptr)
	, velocity_display (nullptr)
	, _height (0.)
{
	CANVAS_DEBUG_NAME (_note_group, X_("note group for MIDI cue"));

	/* Containers don't get canvas events, so we need an invisible rect
	 * that will. It will be resized as needed sothat it always covers the
	 * entire canvas/view.
	 */

	event_rect = new ArdourCanvas::Rectangle (&parent);
	event_rect->set (ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, 10.));
	event_rect->set_fill (false);
	event_rect->set_outline (false);
	CANVAS_DEBUG_NAME (event_rect, "cue event rect");

	parent.Event.connect (sigc::mem_fun (*this, &MidiView::canvas_group_event));

	_note_group->raise_to_top ();

	automation_group = new ArdourCanvas::Rectangle (&parent);
	CANVAS_DEBUG_NAME (automation_group, "cue automation group");
	automation_group->set_fill_color (UIConfiguration::instance().color ("midi automation track fill"));
	automation_group->set_data ("linemerger", this);

	_show_source = true;
	set_extensible (true);
}

MidiCueView::~MidiCueView ()
{
	delete velocity_display;
}

void
MidiCueView::set_height (double h)
{
	_height = h;

	double note_area_height;
	double automation_height;

	if (automation_map.empty()) {
		note_area_height = h;
		automation_height = 0.;
	} else {
		note_area_height = ceil (h / 2.);
		automation_height = ceil (h - note_area_height);
	}

	event_rect->set (ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, note_area_height));
	midi_context().set_size (midi_context().width(), note_area_height);

	automation_group->set_position (ArdourCanvas::Duple (0., note_area_height));
	automation_group->set (ArdourCanvas::Rect (0., 0., ArdourCanvas::COORD_MAX, automation_height));

	for (auto & ads : automation_map) {
		ads.second.set_height (automation_height);
	}

	view_changed ();
}

ArdourCanvas::Item*
MidiCueView::drag_group () const
{
	return event_rect;
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
MidiCueView::update_automation_display (Evoral::Parameter const & param, SelectionOperation op)
{
	using namespace ARDOUR;

	if (!_midi_region) {
		return;
	}

	switch (param.type()) {
	case MidiCCAutomation:
	case MidiPgmChangeAutomation:
	case MidiPitchBenderAutomation:
	case MidiChannelPressureAutomation:
	case MidiNotePressureAutomation:
	case MidiSystemExclusiveAutomation:
	case MidiVelocityAutomation:
		break;
	default:
		return;
	}

	CueAutomationMap::iterator i = automation_map.find (param);
	AutomationDisplayState* ads = nullptr;

	if (i != automation_map.end()) {

		ads = &i->second;

	} else {

		if (op == SelectionRemove) {
			/* remove it, but it doesn't exist yet, no worries */
			return;
		}

		if (param.type() == MidiVelocityAutomation) {


			if (!velocity_display) {

				/* Create and add to automation display map */

				velocity_display = new MidiCueVelocityDisplay (editing_context(), midi_context(), *this, *automation_group, 0x312244ff);
				auto res = automation_map.insert (std::make_pair (Evoral::Parameter (ARDOUR::MidiVelocityAutomation, 0, 0), AutomationDisplayState (*velocity_display, true)));

				ads = &((*res.first).second);

				for (auto & ev : _events) {
					velocity_display->add_note (ev.second);
				}
			}

		} else {

			std::shared_ptr<Evoral::Control> control = _midi_region->model()->control (param, true);
			CueAutomationControl ac = std::dynamic_pointer_cast<AutomationControl> (control);

			if (!ac) {
				return;
			}

			CueAutomationLine line (new MidiCueAutomationLine (ARDOUR::EventTypeMap::instance().to_symbol (param),
			                                                   _editing_context,
			                                                   *automation_group,
			                                                   automation_group,
			                                                   ac->alist(),
			                                                   ac->desc()));
			line->set_line_color ("midi line inactive");

			AutomationDisplayState cad (ac, line, true);

			auto res = automation_map.insert (std::make_pair (param, cad));

			ads = &((*res.first).second);
		}
	}

	switch (op) {
	case SelectionSet:
		/* hide the rest */
		for (auto & as : automation_map) {
			as.second.hide ();
		}
		ads->set_height (automation_group->get().height());
		ads->show ();
		internal_set_active_automation (param);
		break;

	case SelectionAdd:
		ads->set_height (automation_group->get().height());
		ads->show ();
		break;

	case SelectionRemove:
		ads->hide ();
		if (active_automation == ads) {
			unset_active_automation ();
		}
		break;

	case SelectionToggle:
		if (ads->visible) {
			ads->hide ();
			if (active_automation == ads) {
				unset_active_automation ();
			}
		} else {
			ads->set_height (automation_group->get().height());
			ads->show ();
			internal_set_active_automation (param);
		}
		break;

	case SelectionExtend:
		/* undefined in this context */
		break;
	}

	set_height (_height);
}

void
MidiCueView::set_active_automation (Evoral::Parameter const & param)
{
	if (!internal_set_active_automation (param)) {
		update_automation_display (param, SelectionSet);
	}
}

void
MidiCueView::unset_active_automation ()
{
	for (CueAutomationMap::iterator i = automation_map.begin(); i != automation_map.end(); ++i) {
		i->second.line->set_line_color ("midi line inactive");
	}

	active_automation = nullptr;
	AutomationStateChange(); /* EMIT SIGNAL */
}

bool
MidiCueView::internal_set_active_automation (Evoral::Parameter const & param)
{
	bool exists = false;

	for (CueAutomationMap::iterator i = automation_map.begin(); i != automation_map.end(); ++i) {
		if (i->first == param) {
			i->second.line->set_line_color ("gain line");
			active_automation = &i->second;
			exists = true;
		} else {
			i->second.line->set_line_color ("midi line inactive");
		}
	}

	if (exists) {
		AutomationStateChange(); /* EMIT SIGNAL */
	}

	return exists;
}

bool
MidiCueView::is_active_automation (Evoral::Parameter const & param) const
{
	CueAutomationMap::const_iterator i = automation_map.find (param);

	if (i == automation_map.end()) {
		return false;
	}

	return (&i->second == active_automation);
}

bool
MidiCueView::is_visible_automation (Evoral::Parameter const & param) const
{
	CueAutomationMap::const_iterator i = automation_map.find (param);

	if (i == automation_map.end()) {
		return false;
	}

	return (i->second.visible);
}


std::list<SelectableOwner*>
MidiCueView::selectable_owners()
{
	std::list<SelectableOwner*> sl;
	if (active_automation && active_automation->line) {
		sl.push_back (active_automation->line.get());
	}
	return sl;
}

MergeableLine*
MidiCueView::make_merger ()
{
	if (active_automation && active_automation->line) {
		std::cerr << "Mergable will use active automation @ " << active_automation << std::endl;

		return new MergeableLine (active_automation->line, active_automation->control,
		                          [](Temporal::timepos_t const& t) { return t; },
		                          nullptr, nullptr);
	}

	return nullptr;
}

bool
MidiCueView::automation_rb_click (GdkEvent* event, Temporal::timepos_t const & pos)
{
	if (!active_automation || !active_automation->control || !active_automation->line) {
		return false;
	}

	bool with_guard_points = Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier);
	active_automation->line->add (active_automation->control, event, pos, event->button.y, with_guard_points);
	return false;
}

void
MidiCueView::line_drag_click (GdkEvent* event, Temporal::timepos_t const & pos)
{
}

MidiCueView::AutomationDisplayState::~AutomationDisplayState()
{
	/* We do not own the velocity_display */
}

void
MidiCueView::AutomationDisplayState::hide ()
{
	if (velocity_display) {
		velocity_display->hide ();
	} else if (line) {
		line->hide_all ();
	}
	visible = false;
}

void
MidiCueView::AutomationDisplayState::show ()
{
	if (velocity_display) {
		velocity_display->show ();
	} else if (line) {
		line->show ();
	}
	visible = true;
}

void
MidiCueView::AutomationDisplayState::set_height (double h)
{
	if (velocity_display) {
		// velocity_display->set_height (h);
	} else if (line) {
		line->set_height (h);
	}
}

void
MidiCueView::automation_entry ()
{
	if (active_automation && active_automation->line) {
		active_automation->line->track_entered ();
	}
}


void
MidiCueView::automation_leave ()
{
	if (active_automation && active_automation->line) {
		active_automation->line->track_entered ();
	}
}
