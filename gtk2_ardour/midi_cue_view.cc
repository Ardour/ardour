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

#include "gtkmm2ext/utils.h"

#include "canvas/debug.h"

#include "editing_context.h"
#include "editor_drag.h"
#include "keyboard.h"
#include "midi_cue_view.h"

#include "pbd/i18n.h"

using namespace Gtkmm2ext;

MidiCueView::MidiCueView (std::shared_ptr<ARDOUR::MidiTrack> mt,
                          ArdourCanvas::Item&      parent,
                          EditingContext&          ec,
                          MidiViewBackground&      bg,
                          uint32_t                 basic_color)
	: MidiView (mt, parent, ec, bg, basic_color)
{
	CANVAS_DEBUG_NAME (_note_group, X_("note group for MIDI cue"));

	/* Containers don't get canvas events, so we need an invisible rect
	 * that will. It will be resized as needed sothat it always covers the
	 * entire canvas/view.
	 */

	event_rect = new ArdourCanvas::Rectangle (&parent);
	event_rect->set (ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, 10.));
	event_rect->Event.connect (sigc::mem_fun (*this, &MidiCueView::canvas_event));
	event_rect->set_fill (false);
	event_rect->set_outline (false);

	_note_group->raise_to_top ();
}

void
MidiCueView::set_height (double h)
{
	event_rect->set (ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, h));
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

	std::cerr << "for duration of " << duration << " pixels " << _editing_context.duration_to_pixels (duration) << std::endl;

	reset_width_dependent_items (_editing_context.duration_to_pixels (duration));
}
