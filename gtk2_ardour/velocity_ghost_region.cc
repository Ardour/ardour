/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
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

#include <utility>

#include "pbd/memento_command.h"

#include "ardour/automation_control.h"
#include "ardour/event_type_map.h"
#include "ardour/midi_automation_list_binder.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/session.h"

#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/utils.h"

#include "canvas/lollipop.h"

#include "velocity_ghost_region.h"
#include "editing.h"
#include "editor.h"
#include "editor_drag.h"
#include "gui_thread.h"
#include "midi_automation_line.h"
#include "midi_region_view.h"
#include "note_base.h"
#include "public_editor.h"
#include "ui_config.h"
#include "verbose_cursor.h"

#include "pbd/i18n.h"

using namespace Temporal;

static double const lollipop_radius = 8.0;

VelocityGhostRegion::VelocityGhostRegion (MidiRegionView& mrv, TimeAxisView& tv, TimeAxisView& source_tv, double initial_unit_pos)
	: MidiGhostRegion (mrv, tv, source_tv, initial_unit_pos)
	, dragging (false)
	, dragging_line (nullptr)
	, last_drag_x (-1)
	, drag_did_change (false)
	, selected (false)
{
	base_rect->set_data (X_("ghostregionview"), this);
	base_rect->Event.connect (sigc::mem_fun (*this, &VelocityGhostRegion::base_event));
	base_rect->set_fill_color (UIConfiguration::instance().color_mod ("ghost track base", "ghost track midi fill"));
	base_rect->set_outline_color (UIConfiguration::instance().color ("automation track outline"));
	base_rect->set_outline (true);
	base_rect->set_outline_what (ArdourCanvas::Rectangle::What (ArdourCanvas::Rectangle::LEFT|ArdourCanvas::Rectangle::RIGHT));
}

VelocityGhostRegion::~VelocityGhostRegion ()
{
}

bool
VelocityGhostRegion::line_draw_motion (ArdourCanvas::Duple const & d, ArdourCanvas::Rectangle const & r, double last_x)
{
	std::vector<NoteBase*> affected_lollis;
	MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (&parent_rv);

	if (last_x < 0) {
		lollis_close_to_x (d.x, 20., affected_lollis);
	} else if (last_x < d.x) {
		/* rightward, "later" motion */
		lollis_between (last_x, d.x, affected_lollis);
	} else {
		/* leftward, "earlier" motion */
		lollis_between (d.x, last_x, affected_lollis);
	}

	bool ret = false;

	if (!affected_lollis.empty()) {
		int velocity = y_position_to_velocity (r.height() - (r.y1() - d.y));
		ret = mrv->set_velocity_for_notes (affected_lollis, velocity);
		mrv->mid_drag_edit ();
	}

	return ret;
}

bool
VelocityGhostRegion::base_event (GdkEvent* ev)
{
	return trackview.editor().canvas_velocity_base_event (ev, base_rect);
}

void
VelocityGhostRegion::update_contents_height ()
{
	for (auto const & i : events) {
		set_size_and_position (*i.second);
	}
}

bool
VelocityGhostRegion::lollevent (GdkEvent* ev, MidiGhostRegion::GhostEvent* gev)
{
	return trackview.editor().canvas_velocity_event (ev, gev->item);
}

void
VelocityGhostRegion::add_note (NoteBase* nb)
{
	ArdourCanvas::Lollipop* l = new ArdourCanvas::Lollipop (_note_group);
	l->set_bounding_parent (base_rect);

	GhostEvent* event = new GhostEvent (nb, _note_group, l);
	events.insert (std::make_pair (nb->note(), event));
	_optimization_iterator = events.end();

	l->Event.connect (sigc::bind (sigc::mem_fun (*this, &VelocityGhostRegion::lollevent), event));
	l->set_ignore_events (true);
	l->raise_to_top ();
	l->set_data (X_("ghostregionview"), this);
	l->set_data (X_("note"), nb);
	l->set_fill_color (nb->base_color());
	l->set_outline_color (_outline);

	MidiStreamView* mv = midi_view();

	if (mv) {
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (&parent_rv);
		if (mrv->note_in_region_time_range (nb->note())) {
			set_size_and_position (*event);
		} else {
			l->hide();
		}
	}
}

void
VelocityGhostRegion::set_size_and_position (GhostEvent& ev)
{
	ArdourCanvas::Lollipop* l = dynamic_cast<ArdourCanvas::Lollipop*> (ev.item);
	const double available_height = base_rect->y1();
	const double actual_height = (ev.event->note()->velocity() / 127.0) * available_height;
	l->set (ArdourCanvas::Duple (ev.event->x0() + lollipop_radius, base_rect->y1() - actual_height), actual_height, lollipop_radius);
}

void
VelocityGhostRegion::update_note (GhostEvent* gev)
{
	set_size_and_position (*gev);
	gev->item->set_fill_color (gev->event->base_color());
}

void
VelocityGhostRegion::update_hit (GhostEvent* gev)
{
	set_size_and_position (*gev);
	gev->item->set_fill_color (gev->event->base_color());
}

void
VelocityGhostRegion::remove_note (NoteBase* nb)
{
	MidiGhostRegion::remove_note (nb);
}

void
VelocityGhostRegion::set_colors ()
{
	if (selected) {
		base_rect->set_fill_color (UIConfiguration::instance().color ("ghost track base"));
	} else {
		base_rect->set_fill_color (UIConfiguration::instance().color_mod ("ghost track base", "ghost track midi fill"));
	}

	for (auto & gev : events) {
		gev.second->item->set_fill_color (gev.second->event->base_color());
	}
}

void
VelocityGhostRegion::drag_lolli (ArdourCanvas::Lollipop* l, GdkEventMotion* ev)
{
	ArdourCanvas::Rect r (base_rect->item_to_canvas (base_rect->get()));

	/* translate event y-coord so that zero matches the top of base_rect
	 * (event coordinates use window coordinate space)
	 */

	ev->y -= r.y0;

	/* clamp y to be within the range defined by the base_rect height minus
	 * the lollipop radius at top and bottom
	 */

	const double effective_y = std::max (0.0, std::min (r.height(), ev->y));
	const double newlen = r.height() - effective_y;
	const double delta = newlen - l->length();

	MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (&parent_rv);
	assert (mrv);

	/* This will redraw the velocity bars for the selected notes, without
	 * changing the note velocities.
	 */

	const double factor = newlen / base_rect->height();
	mrv->sync_velocity_drag (factor);

	MidiRegionView::Selection const & sel (mrv->selection());
	int verbose_velocity = -1;
	GhostEvent* primary_ghost = 0;

	for (auto & s : sel) {
		GhostEvent* x = find_event (s->note());

		if (x) {
			ArdourCanvas::Lollipop* lolli = dynamic_cast<ArdourCanvas::Lollipop*> (x->item);
			lolli->set (ArdourCanvas::Duple (lolli->x(), lolli->y0() - delta), lolli->length() + delta, lollipop_radius);
			/* note: length is now set to the new value */
			const int newvel = floor (127. * (l->length() / r.height()));
			/* since we're not actually changing the note velocity
			   (yet), we have to use the static method to compute
			   the color.
			*/
			lolli->set_fill_color (NoteBase::base_color (newvel, mrv->color_mode(), mrv->midi_stream_view()->get_region_color(), x->event->note()->channel(), true));

			if (l == lolli) {
				/* This is the value we will display */
				verbose_velocity = newvel;
				primary_ghost = x;
			}
		}
	}

	assert (verbose_velocity >= 0);
	char buf[128];
	const int  oldvel = primary_ghost->event->note()->velocity();

	if (verbose_velocity > oldvel) {
		snprintf (buf, sizeof (buf), "Velocity %d (+%d)", verbose_velocity, verbose_velocity - oldvel);
	} else if (verbose_velocity == oldvel) {
		snprintf (buf, sizeof (buf), "Velocity %d", verbose_velocity);
	} else {
		snprintf (buf, sizeof (buf), "Velocity %d (%d)", verbose_velocity, verbose_velocity - oldvel);
	}

	trackview.editor().verbose_cursor()->set (buf);
	trackview.editor().verbose_cursor()->show ();
	trackview.editor().verbose_cursor()->set_offset (ArdourCanvas::Duple (10., 10.));
}

int
VelocityGhostRegion::y_position_to_velocity (double y) const
{
	const ArdourCanvas::Rect r (base_rect->get());
	int velocity;

	if (y >= r.height())  {
		velocity = 0;
	} else if (y <= 0.) {
		velocity = 127;
	} else {
		velocity = floor (127. * (1.0 - (y / r.height())));
	}

	return velocity;
}

void
VelocityGhostRegion::note_selected (NoteBase* ev)
{
	GhostEvent* gev = find_event (ev->note());

	if (!gev) {
		return;
	}

	ArdourCanvas::Lollipop* lolli = dynamic_cast<ArdourCanvas::Lollipop*> (gev->item);
	lolli->set_outline_color (ev->selected() ? UIConfiguration::instance().color ("midi note selected outline") : 0x000000ff);
	lolli->raise_to_top();
}

void
VelocityGhostRegion::lollis_between (int x0, int x1, std::vector<NoteBase*>& within)
{
	for (auto & gev : events) {
		ArdourCanvas::Lollipop* l = dynamic_cast<ArdourCanvas::Lollipop*> (gev.second->item);
		if (l) {
			ArdourCanvas::Duple pos = l->item_to_canvas (ArdourCanvas::Duple (l->x(), l->y0()));
			if (pos.x >= x0 && pos.x < x1) {
				within.push_back (gev.second->event);
			}
		}
	}
}

void
VelocityGhostRegion::lollis_close_to_x (int x, double distance, std::vector<NoteBase*>& within)
{
	for (auto & gev : events) {
		ArdourCanvas::Lollipop* l = dynamic_cast<ArdourCanvas::Lollipop*> (gev.second->item);
		if (l) {
			ArdourCanvas::Duple pos = l->item_to_canvas (ArdourCanvas::Duple (l->x(), l->y0()));
			if (std::abs (pos.x - x) < distance) {
				within.push_back (gev.second->event);
			}
		}
	}
}

void
VelocityGhostRegion::start_line_drag ()
{
	MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (&parent_rv);
	mrv->begin_drag_edit (_("draw velocities"));
	desensitize_lollis ();
}

void
VelocityGhostRegion::end_line_drag (bool did_change)
{
	MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (&parent_rv);
	mrv->end_drag_edit (did_change);
	sensitize_lollis ();
}

void
VelocityGhostRegion::desensitize_lollis ()
{
	for (auto & gev : events) {
		gev.second->item->set_ignore_events (true);
	}
}

void
VelocityGhostRegion::sensitize_lollis ()
{
	for (auto & gev : events) {
		gev.second->item->set_ignore_events (false);
	}
}

void
VelocityGhostRegion::set_selected (bool yn)
{
	selected = yn;
	set_colors ();

	if (yn) {
		group->raise_to_top ();
	}
}
