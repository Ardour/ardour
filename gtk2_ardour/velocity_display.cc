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

#include "editing.h"
#include "editing_context.h"
#include "editor_drag.h"
#include "ghost_event.h"
#include "gui_thread.h"
#include "midi_automation_line.h"
#include "midi_region_view.h"
#include "midi_view.h"
#include "midi_view_background.h"
#include "note_base.h"
#include "ui_config.h"
#include "velocity_display.h"
#include "verbose_cursor.h"

#include "pbd/i18n.h"

using namespace Temporal;

static double const lollipop_radius = 6.0;

VelocityDisplay::VelocityDisplay (EditingContext& ec, MidiViewBackground& background, MidiView& mv, ArdourCanvas::Rectangle& base_rect, ArdourCanvas::Container& lc,
                                  GhostEvent::EventList& el, Gtkmm2ext::Color oc)
	: editing_context (ec)
	, bg (background)
	, view (mv)
	, base (base_rect)
	, lolli_container (&lc)
	, events (el)
	, _outline (oc)
	, dragging (false)
	, dragging_line (nullptr)
	, last_drag_x (-1)
	, drag_did_change (false)
	, selected (false)
	, _optimization_iterator (events.end())
	, _sensitive (false)
{
	base.set_data (X_("ghostregionview"), this);
	base.Event.connect (sigc::mem_fun (*this, &VelocityDisplay::base_event));
	base.set_fill_color (UIConfiguration::instance().color_mod ("ghost track base", "ghost track midi fill"));
	base.set_outline_color (UIConfiguration::instance().color ("automation track outline"));
	base.set_outline (true);
	base.set_outline_what (ArdourCanvas::Rectangle::What (ArdourCanvas::Rectangle::LEFT|ArdourCanvas::Rectangle::RIGHT));
}

VelocityDisplay::~VelocityDisplay ()
{
}

bool
VelocityDisplay::line_draw_motion (ArdourCanvas::Duple const & d, ArdourCanvas::Rectangle const & r, double last_x)
{
	std::vector<GhostEvent*> affected_lollis;

	if (last_x < 0) {
		lollis_close_to_x (d.x, 20., affected_lollis);
	} else if (last_x < d.x) {
		/* rightward, "later" motion */
		lollis_between (last_x, d.x, affected_lollis);
	} else {
		/* leftward, "earlier" motion */
		lollis_between (d.x, last_x, affected_lollis);
	}

	if (affected_lollis.empty()) {
		return false;
	}

	int velocity = y_position_to_velocity (r.height() - (r.y1() - d.y));

	for (auto & lolli : affected_lollis) {
		lolli->velocity_while_editing = velocity;
		set_size_and_position (*lolli);
	}

	return true;
}

bool
VelocityDisplay::line_extended (ArdourCanvas::Duple const & from, ArdourCanvas::Duple const & to, ArdourCanvas::Rectangle const & r, double last_x)
{
	std::vector<GhostEvent*> affected_lollis;

	lollis_between (from.x, to.x, affected_lollis);

	if (affected_lollis.empty()) {
		return false;
	}

	if (to.x == from.x) {
		/* no x-axis motion */
		return false;
	}

	double slope =  (to.y - from.y) / (to.x - from.x);

	for (auto const & lolli : affected_lollis) {
		ArdourCanvas::Item* item = lolli->item;
		ArdourCanvas::Duple pos = item->item_to_canvas (ArdourCanvas::Duple (lolli->event->x0(), 0.0));
		int y = from.y + (slope * (pos.x - from.x));
		lolli->velocity_while_editing = y_position_to_velocity (r.height() - (r.y1() - y));
		set_size_and_position (*lolli);
	}

	return true;
}

void
VelocityDisplay::redisplay ()
{
	for (auto const & i : events) {
		set_size_and_position (*i.second);
	}
}

void
VelocityDisplay::clear ()
{
	for (auto & ev : events) {
		delete ev.second;
	}
	events.clear ();
	_optimization_iterator = events.end();
}

void
VelocityDisplay::set_sensitive (bool yn)
{
	for (auto & ev : events) {
		ev.second->set_sensitive (yn);
	}

	_sensitive = yn;

	set_colors ();
}

bool
VelocityDisplay::sensitive () const
{
	return _sensitive;
}

void
VelocityDisplay::add_note (NoteBase* nb)
{
	ArdourCanvas::Lollipop* l = new ArdourCanvas::Lollipop (lolli_container);
	l->set_bounding_parent (&base);

	GhostEvent* event = new GhostEvent (nb, lolli_container, l);
	events.insert (std::make_pair (nb->note(), event));

	l->Event.connect (sigc::bind (sigc::mem_fun (*this, &VelocityDisplay::lollevent), event));
	l->set_ignore_events (!_sensitive);
	l->raise_to_top ();
	l->set_data (X_("ghostregionview"), this);
	l->set_data (X_("note"), nb);
	l->set_outline_color (_outline);
	color_ghost_event (event);

	if (view.note_in_region_time_range (nb->note())) {
		set_size_and_position (*event);
	} else {
		l->hide();
	}
}

void
VelocityDisplay::set_size_and_position (GhostEvent& gev)
{
	if (base.get().empty()) {
		return;
	}

	ArdourCanvas::Lollipop* l = dynamic_cast<ArdourCanvas::Lollipop*> (gev.item);
	const double available_height = base.height();
	const double actual_height = ((dragging ? gev.velocity_while_editing : gev.event->note()->velocity()) / 127.0) * available_height;
	const double scale  = UIConfiguration::instance ().get_ui_scale ();

	if (gev.is_hit) {
		/* compare to Hit::points , offset by w/2 */
		l->set (ArdourCanvas::Duple (gev.event->x0() + (gev.event->x1() - gev.event->x0()) / 2, base.y1() - actual_height), actual_height, lollipop_radius * scale);
	} else {
		l->set (ArdourCanvas::Duple (gev.event->x0(), base.y1() - actual_height), actual_height, lollipop_radius * scale);
	}
}

void
VelocityDisplay::update_note (NoteBase* nb)
{
	auto iter = events.end();

	GhostEvent* gev = GhostEvent::find (nb->note(), events, iter);

	if (!gev) {
		return;
	}

	_optimization_iterator = iter;
	update_ghost_event (gev);
}

void
VelocityDisplay::update_ghost_event (GhostEvent* gev)
{
	set_size_and_position (*gev);
	color_ghost_event (gev);
}

void
VelocityDisplay::color_ghost_event (GhostEvent* gev)
{
	if (sensitive()) {
		gev->item->set_fill_color (gev->event->base_color());
	} else {
		/* Note: notes may have different colors */
		gev->item->set_fill_color (Gtkmm2ext::change_alpha (gev->event->base_color(), 0.2));
	}
}

void
VelocityDisplay::set_colors ()
{
	base.set_fill_color (UIConfiguration::instance().color_mod ("ghost track base", "ghost track midi fill"));

	for (auto & gev : events) {
		color_ghost_event (gev.second);
	}
}

void
VelocityDisplay::drag_lolli (ArdourCanvas::Lollipop* l, GdkEventMotion* ev)
{
	ArdourCanvas::Rect r (base.item_to_canvas (base.get()));

	/* translate event y-coord so that zero matches the top of base
	 * (event coordinates use window coordinate space)
	 */

	ev->y -= r.y0;

	/* clamp y to be within the range defined by the base height minus
	 * the lollipop radius at top and bottom
	 */

	const double effective_y = std::max (0.0, std::min (r.height(), ev->y));
	const double newlen = r.height() - effective_y;
	const double delta = newlen - l->length();

	/* This will redraw the velocity bars for the selected notes, without
	 * changing the note velocities.
	 */

	const double factor = newlen / base.height();
	view.sync_velocity_drag (factor);

	MidiView::Selection const & sel (view.selection());
	int verbose_velocity = -1;
	GhostEvent* primary_ghost = 0;
	const double scale  = UIConfiguration::instance ().get_ui_scale ();

	for (auto & s : sel) {
		GhostEvent* x = GhostEvent::find (s->note(), events, _optimization_iterator);

		if (x) {
			ArdourCanvas::Lollipop* lolli = dynamic_cast<ArdourCanvas::Lollipop*> (x->item);
			lolli->set (ArdourCanvas::Duple (lolli->x(), lolli->y0() - delta), lolli->length() + delta, lollipop_radius * scale);
			/* note: length is now set to the new value */
			const int newvel = floor (127. * (l->length() / r.height()));
			/* since we're not actually changing the note velocity
			   (yet), we have to use the static method to compute
			   the color.
			*/
			lolli->set_fill_color (NoteBase::base_color (newvel, bg.color_mode(), bg.region_color(), x->event->note()->channel(), true));

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

	editing_context.verbose_cursor()->set (buf);
	editing_context.verbose_cursor()->show ();
	editing_context.verbose_cursor()->set_offset (ArdourCanvas::Duple (10., 10.));
}

int
VelocityDisplay::y_position_to_velocity (double y) const
{
	const ArdourCanvas::Rect r (base.get());
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
VelocityDisplay::note_selected (NoteBase* ev)
{
	auto ignore_optiter = events.end();

	GhostEvent* gev = GhostEvent::find (ev->note(), events, ignore_optiter);

	if (!gev) {
		return;
	}

	ArdourCanvas::Lollipop* lolli = dynamic_cast<ArdourCanvas::Lollipop*> (gev->item);
	lolli->set_outline_color (ev->selected() ? UIConfiguration::instance().color ("midi note selected outline") : 0x000000ff);
	lolli->raise_to_top();
}

void
VelocityDisplay::lollis_between (int x0, int x1, std::vector<GhostEvent*>& within)
{
	MidiView::Selection const & sel (view.selection());
	bool only_selected = !sel.empty();

	for (auto & gev : events) {
		if (only_selected) {
			if (!gev.second->event->selected()) {
				continue;
			}
		}
		ArdourCanvas::Lollipop* l = dynamic_cast<ArdourCanvas::Lollipop*> (gev.second->item);
		if (l) {
			ArdourCanvas::Duple pos = l->item_to_canvas (ArdourCanvas::Duple (l->x(), l->y0()));
			if (pos.x >= x0 && pos.x < x1) {
				within.push_back (gev.second);
			}
		}
	}
}

void
VelocityDisplay::lollis_close_to_x (int x, double distance, std::vector<GhostEvent*>& within)
{
	for (auto & gev : events) {
		ArdourCanvas::Lollipop* l = dynamic_cast<ArdourCanvas::Lollipop*> (gev.second->item);
		if (l) {
			ArdourCanvas::Duple pos = l->item_to_canvas (ArdourCanvas::Duple (l->x(), l->y0()));
			if (std::abs (pos.x - x) < distance) {
				within.push_back (gev.second);
			}
		}
	}
}

void
VelocityDisplay::start_line_drag ()
{
	view.begin_drag_edit (_("draw velocities"));

	for (auto & e : events) {
		GhostEvent* gev (e.second);
		gev->velocity_while_editing = gev->event->note()->velocity();
	}

	dragging = true;
	desensitize_lollis ();
}

void
VelocityDisplay::end_line_drag (bool did_change)
{
	dragging = false;

	if (did_change) {
		std::vector<NoteBase*> notes;
		std::vector<int> velocities;

		for (auto & e : events) {
			GhostEvent* gev (e.second);
			if (gev->event->note()->velocity() != gev->velocity_while_editing) {
				notes.push_back (gev->event);
				velocities.push_back (gev->velocity_while_editing);
			}
		}

		view.set_velocities_for_notes (notes, velocities);
	}

	view.end_drag_edit ();
	sensitize_lollis ();
}

void
VelocityDisplay::desensitize_lollis ()
{
	for (auto & gev : events) {
		gev.second->item->set_ignore_events (true);
	}
}

void
VelocityDisplay::sensitize_lollis ()
{
	for (auto & gev : events) {
		gev.second->item->set_ignore_events (false);
	}
}

void
VelocityDisplay::set_selected (bool yn)
{
	selected = yn;
	set_colors ();

	if (yn) {
		base.parent()->raise_to_top ();
	}
}

void
VelocityDisplay::hide ()
{
	if (lolli_container) {
		lolli_container->hide ();
		lolli_container->set_ignore_events (true);
	}

}

void
VelocityDisplay::show ()
{
	if (lolli_container) {
		lolli_container->show ();
		lolli_container->set_ignore_events (false);
	}
}
