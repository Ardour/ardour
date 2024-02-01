/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015 John Emmas <john@creativepost.co.uk>
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

//#define CANVAS_PROFILE

/** @file  canvas/canvas.cc
 *  @brief Implementation of the main canvas classes.
 */

#include <list>
#include <cassert>
#include <gtkmm/adjustment.h>
#include <gtkmm/label.h>
#include <gtkmm/window.h>

#include "gtkmm2ext/persistent_tooltip.h"

#include "pbd/compose.h"

#include "canvas/canvas.h"
#include "gtkmm2ext/colors.h"
#include "canvas/debug.h"
#include "canvas/line.h"
#include "canvas/scroll_group.h"

#ifdef __APPLE__
#include <gdk/gdk.h>
#include "gtkmm2ext/nsglview.h"
#endif

using namespace std;
using namespace ArdourCanvas;

uint32_t Canvas::tooltip_timeout_msecs = 750;

/** Construct a new Canvas */
Canvas::Canvas ()
	: _root (this)
	, _queue_draw_frozen (0)
	, _bg_color (Gtkmm2ext::rgba_to_color (0, 1.0, 0.0, 1.0))
	, _debug_render (false)
	, _last_render_start_timestamp(0)
	, _use_intermediate_surface (false)
{
#ifdef __APPLE__
	_use_intermediate_surface = true;
#else
	_use_intermediate_surface = NULL != g_getenv("ARDOUR_INTERMEDIATE_SURFACE");
#endif

	if (g_getenv ("ARDOUR_ITEM_CAIRO_SAVE_RESTORE")) {
		item_save_restore = true;
	} else {
		item_save_restore = false;
	}

	set_epoch ();
}

void
Canvas::use_intermediate_surface (bool yn)
{
	if (_use_intermediate_surface == yn) {
		return;
	}
	_use_intermediate_surface = yn;
}

void
Canvas::scroll_to (Coord x, Coord y)
{
	/* We do things this way because we do not want to recurse through
	   the canvas for every scroll. In the presence of large MIDI
	   tracks this means traversing item lists that include
	   thousands of items (notes).

	   This design limits us to moving only those items (groups, typically)
	   that should move in certain ways as we scroll. In other terms, it
	   becomes O(1) rather than O(N).
	*/

	for (list<ScrollGroup*>::iterator i = scrollers.begin(); i != scrollers.end(); ++i) {
		(*i)->scroll_to (Duple (x, y));
	}

	pick_current_item (0); // no current mouse position
}

void
Canvas::add_scroller (ScrollGroup& i)
{
	scrollers.push_back (&i);
}

void
Canvas::zoomed ()
{
	pick_current_item (0); // no current mouse position
}

#ifndef NDEBUG
#ifdef CANVAS_DEBUG
#undef CANVAS_DEBUG
#define CANVAS_DEBUG
#endif
#endif

/** Render an area of the canvas.
 *  @param area Area in window coordinates.
 *  @param context Cairo context to render to.
 */
void
Canvas::render (Rect const & area, Cairo::RefPtr<Cairo::Context> const & context) const
{
#ifdef CANVAS_PROFILE
	const int64_t start = g_get_monotonic_time ();
#endif

	PreRender (); // emit signal

	_last_render_start_timestamp = g_get_monotonic_time();

#ifdef CANVAS_DEBUG
	if (_debug_render || DEBUG_ENABLED(PBD::DEBUG::CanvasRender)) {
		cerr << this << " RENDER: " << area << endl;
		 cerr << "CANVAS @ " << this << endl;
		 dump (cerr);
		 cerr << "-------------------------\n";
	}
#endif

	render_count = 0;

	Rect root_bbox = _root.bounding_box();
	if (!root_bbox) {
		/* the root has no bounding box, so there's nothing to render */
		cerr << "no bbox\n";
		return;
	}

	Rect draw = root_bbox.intersection (area);
	if (draw) {

		/* there's a common area between the root and the requested
		   area, so render it.
		*/

		_root.render (draw, context);

#if defined CANVAS_DEBUG && !PLATFORM_WINDOWS
		if (getenv ("CANVAS_HARLEQUIN_DEBUGGING")) {
			// This transparently colors the rect being rendered, after it has been drawn.
			double r = (random() % 65536) /65536.0;
			double g = (random() % 65536) /65536.0;
			double b = (random() % 65536) /65536.0;
			context->rectangle (draw.x0, draw.y0, draw.x1 - draw.x0, draw.y1 - draw.y0);
			context->set_source_rgba (r, g, b, 0.25);
			context->fill ();
		}
#endif
	}

#ifdef CANVAS_PROFILE
	const int64_t end = g_get_monotonic_time ();
	const int64_t elapsed = end - start;
	std::cout << "GtkCanvas::render " << area << " " << (elapsed / 1000.f) << " ms\n";
#endif

}

void
Canvas::prepare_for_render (Rect const & area) const
{
	Rect root_bbox = _root.bounding_box();
	if (!root_bbox) {
		/* the root has no bounding box, so there's nothing to render */
		return;
	}

	Rect draw = root_bbox.intersection (area);

	if (draw) {
		_root.prepare_for_render (draw);
	}
}

gint64
Canvas::get_microseconds_since_render_start () const
{
	gint64 timestamp = g_get_monotonic_time();

	if (_last_render_start_timestamp == 0 || timestamp <= _last_render_start_timestamp) {
		return 0;
	}

	return timestamp - _last_render_start_timestamp;
}

ostream&
operator<< (ostream& o, Canvas& c)
{
	c.dump (o);
	return o;
}

std::string
Canvas::indent() const
{
	string s;

	for (int n = 0; n < ArdourCanvas::dump_depth; ++n) {
		s += '\t';
	}

	return s;
}

std::string
Canvas::render_indent() const
{
	string s;

	for (int n = 0; n < ArdourCanvas::render_depth; ++n) {
		s += ' ';
	}

	return s;
}

void
Canvas::dump (ostream& o) const
{
	dump_depth = 0;
	_root.dump (o);
}

void
Canvas::freeze_queue_draw ()
{
	_queue_draw_frozen++;
}

void
Canvas::thaw_queue_draw ()
{
	if (_queue_draw_frozen) {
		_queue_draw_frozen--;

		if (_queue_draw_frozen == 0 && !frozen_area.empty()) {
			request_redraw (frozen_area);
			frozen_area = Rect();
		}
	}
}

/** Called when an item has been shown or hidden.
 *  @param item Item that has been shown or hidden.
 */
void
Canvas::item_shown_or_hidden (Item* item)
{
	Rect bbox = item->bounding_box ();
	if (bbox) {
		if (_queue_draw_frozen) {
			frozen_area = frozen_area.extend (compute_draw_item_area (item, bbox));
			return;
		}

		if (item->item_to_window (bbox).intersection (visible_area ())) {
			queue_draw_item_area (item, bbox);
		}
	}
}

/** Called when an item has a change to its visual properties
 *  that do NOT affect its bounding box.
 *  @param item Item that has been modified.
 */
void
Canvas::item_visual_property_changed (Item* item)
{
	Rect bbox = item->bounding_box ();
	if (bbox) {
		if (item->item_to_window (bbox).intersection (visible_area ())) {
			queue_draw_item_area (item, bbox);
		}
	}
}

/** Called when an item has changed, but not moved.
 *  @param item Item that has changed.
 *  @param pre_change_bounding_box The bounding box of item before the change,
 *  in the item's coordinates.
 */
void
Canvas::item_changed (Item* item, Rect pre_change_bounding_box)
{
	Rect window_bbox = visible_area ();

	if (pre_change_bounding_box) {
		if (item->item_to_window (pre_change_bounding_box).intersection (window_bbox)) {
			/* request a redraw of the item's old bounding box */
			queue_draw_item_area (item, pre_change_bounding_box);
		}
	}

	Rect post_change_bounding_box = item->bounding_box ();

	if (post_change_bounding_box) {
		Rect const window_intersection =
		    item->item_to_window (post_change_bounding_box).intersection (window_bbox);

		if (window_intersection) {
			/* request a redraw of the item's new bounding box */
			queue_draw_item_area (item, post_change_bounding_box);

			// Allow item to do any work necessary to prepare for being rendered.
			item->prepare_for_render (window_intersection);
		} else {
			// No intersection with visible window area
		}
	}
}

Duple
Canvas::window_to_canvas (Duple const & d) const
{
	ScrollGroup* best_group = 0;
	ScrollGroup* sg = 0;

	/* if the coordinates are negative, clamp to zero and find the item
	 * that covers that "edge" position.
	 */

	Duple in_window (d);

	if (in_window.x < 0) {
		in_window.x = 0;
	}
	if (in_window.y < 0) {
		in_window.y = 0;
	}

	for (list<ScrollGroup*>::const_iterator s = scrollers.begin(); s != scrollers.end(); ++s) {

		if ((*s)->covers_window (in_window)) {
			sg = *s;

			/* XXX January 22nd 2015: leaving this in place for now
			 * but I think it fixes a bug that really should be
			 * fixed in a different way (and will be) by my next
			 * commit. But it may still be relevant.
			 */

			/* If scroll groups overlap, choose the one with the highest sensitivity,
			   that is, choose an HV scroll group over an H or V
			   only group.
			*/
			if (!best_group || sg->sensitivity() > best_group->sensitivity()) {

				best_group = sg;

				if (sg->sensitivity() == (ScrollGroup::ScrollsVertically | ScrollGroup::ScrollsHorizontally)) {
					/* Can't do any better than this. */
					break;
				}
			}
		}
	}

	if (best_group && (!have_grab() || grab_can_translate ())) {
		return d.translate (best_group->scroll_offset());
	}

	return d;
}

Duple
Canvas::canvas_to_window (Duple const & d, bool rounded) const
{
	/* Find the scroll group that covers d (a canvas coordinate). Scroll groups are only allowed
	 * as children of the root group, so we just scan its first level
	 * children and see what we can find.
	 */

	std::list<Item*> const& root_children (_root.items());
	ScrollGroup* sg = 0;
	Duple wd;

	for (std::list<Item*>::const_iterator i = root_children.begin(); i != root_children.end(); ++i) {
		if (((sg = dynamic_cast<ScrollGroup*>(*i)) != 0) && sg->covers_canvas (d)) {
			break;
		}
	}

	if (sg) {
		wd = d.translate (-sg->scroll_offset());
	} else {
		wd = d;
	}

	/* Note that this intentionally almost always returns integer coordinates */

	if (rounded) {
		wd.x = round (wd.x);
		wd.y = round (wd.y);
	}

	return wd;
}

/** Called when an item has moved.
 *  @param item Item that has moved.
 *  @param pre_change_parent_bounding_box The bounding box of the item before
 *  the move, in its parent's coordinates.
 */
void
Canvas::item_moved (Item* item, Rect pre_change_parent_bounding_box)
{
	if (pre_change_parent_bounding_box) {
		/* request a redraw of where the item used to be. The box has
		 * to be in parent coordinate space since the bounding box of
		 * an item does not change when moved. If we use
		 * item->item_to_canvas() on the old bounding box, we will be

		 * using the item's new position, and so will compute the wrong
		 * invalidation area. If we use the parent (which has not
		 * moved, then this will work.
		 */
		queue_draw_item_area (item->parent(), pre_change_parent_bounding_box);
	}

	Rect post_change_bounding_box = item->bounding_box ();
	if (post_change_bounding_box) {
		/* request a redraw of where the item now is */
		queue_draw_item_area (item, post_change_bounding_box);
	}
}

/** Request a redraw of a particular area in an item's coordinates.
 *  @param item Item.
 *  @param area Area to redraw in the item's coordinates.
 */
void
Canvas::queue_draw_item_area (Item* item, Rect area)
{
	request_redraw (compute_draw_item_area (item, area));
}

Rect
Canvas::compute_draw_item_area (Item* item, Rect area)
{
	Rect r;

	if ((area.width()) > 1.0 && (area.height() > 1.0)) {
		/* item has a rectangular bounding box, which may fall
		 * on non-integer locations. Expand it appropriately.
		 */
		r = item->item_to_window (area, false);
		r.x0 = floor (r.x0);
		r.y0 = floor (r.y0);
		r.x1 = ceil (r.x1);
		r.y1 = ceil (r.y1);
		//std::cerr << "redraw box, adjust from " << area << " to " << r << std::endl;
	} else if (area.width() > 1.0 && area.height() == 1.0) {
		/* horizontal line, which may fall on non-integer
		 * coordinates.
		 */
		r = item->item_to_window (area, false);
		r.y0 = floor (r.y0);
		r.y1 = ceil (r.y1);
		//std::cerr << "redraw HLine, adjust from " << area << " to " << r << std::endl;
	} else if (area.width() == 1.0 && area.height() > 1.0) {
		/* vertical single pixel line, which may fall on non-integer
		 * coordinates
		 */
		r = item->item_to_window (area, false);
		r.x0 = floor (r.x0);
		r.x1 = ceil (r.x1);
		//std::cerr << "redraw VLine, adjust from " << area << " to " << r << std::endl;

	} else {
		/* impossible? one of width or height must be zero ... */
		//std::cerr << "redraw IMPOSSIBLE of " << area  << std::endl;
		r =  item->item_to_window (area, false);
	}

	return r;
}

void
Canvas::set_tooltip_timeout (uint32_t msecs)
{
	tooltip_timeout_msecs = msecs;
}

void
Canvas::set_background_color (Gtkmm2ext::Color c)
{
	_bg_color = c;

	Rect r = _root.bounding_box();

	if (r) {
		request_redraw (_root.item_to_window (r));
	}
}

void
GtkCanvas::re_enter ()
{
	DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, "re-enter canvas by request\n");
	_current_item = 0;
	pick_current_item (0);
	PBD::stacktrace (std::cerr, 20);
}

/** Construct a GtkCanvas */
GtkCanvas::GtkCanvas ()
	: _current_item (0)
	, _new_current_item (0)
	, _grabbed_item (0)
	, _focused_item (0)
	, _single_exposure (true)
	, _use_image_surface (false)
	, current_tooltip_item (0)
	, tooltip_window (0)
	, _in_dtor (false)
	, resize_queued (false)
	, _nsglview (0)
{
#ifdef USE_CAIRO_IMAGE_SURFACE /* usually Windows builds */
	_use_image_surface = true;
#else
	_use_image_surface = NULL != g_getenv("ARDOUR_IMAGE_SURFACE");
#endif

	/* these are the events we want to know about */
	add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK |
		    Gdk::SCROLL_MASK | Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK |
		    Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
}

void
GtkCanvas::set_single_exposure (bool yn)
{
	if (g_getenv ("ARDOUR_CANVAS_SINGLE_EXPOSE_ALWAYS")) {
		yn = true;
	}

	_single_exposure = yn;
}

void
GtkCanvas::use_nsglview (bool retina)
{
	assert (!_nsglview);
	assert (!get_realized());
#ifdef ARDOUR_CANVAS_NSVIEW_TAG // patched gdkquartz.h
	_nsglview = Gtkmm2ext::nsglview_create (this, retina);
#endif
}

void
GtkCanvas::pick_current_item (int state)
{
	int x;
	int y;

	/* this version of ::pick_current_item() is called after an item is
	 * added or removed, so we have no coordinates to work from as is the
	 * case with a motion event. Find out where the mouse is and use that.
	 */

	Glib::RefPtr<const Gdk::Window> pointer_window = Gdk::Display::get_default()->get_window_at_pointer (x, y);

	if (pointer_window != get_window()) {
		return;
	}

	pick_current_item (Duple (x, y), state);
}

/** Given @p point (a position in window coordinates)
 *  and mouse state @p state, check to see if _current_item
 *  (which will be used to deliver events) should change.
 */
void
GtkCanvas::pick_current_item (Duple const & point, int state)
{
	/* we do not enter/leave items during a drag/grab */

	if (_grabbed_item) {
		return;
	}

	/* find the items at the given window position */

	vector<Item const *> items;
	_root.add_items_at_point (point, items);

	DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, string_compose ("%1 covers %2 items\n", point, items.size()));

#ifndef NDEBUG
	if (DEBUG_ENABLED(PBD::DEBUG::CanvasEnterLeave)) {
		for (auto const & item : items) {
			std::cerr << "\tItem " << item->whoami() << " ignore events ? " << item->ignore_events() << " vis ? " << item->visible() << std::endl;
		}
	}
#endif

	/* put all items at point that are event-sensitive and visible and NOT
	   groups into within_items. Note that items is sorted from bottom to
	   top, but we're going to reverse that for within_items so that its
	   first item is the upper-most item that can be chosen as _current_item.
	*/

	vector<Item const *>::const_iterator i;
	list<Item const *> within_items;

	for (i = items.begin(); i != items.end(); ++i) {

		Item const * possible_item = *i;

		/* We ignore invisible items, containers and items that ignore events */

		if (!possible_item->visible() || possible_item->ignore_events() || dynamic_cast<ArdourCanvas::Container const *>(possible_item) != 0) {
			continue;
		}
		within_items.push_front (possible_item);
	}

	DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, string_compose ("after filtering insensitive + containers, we have  %1 items\n", within_items.size()));

	if (within_items.empty()) {

		/* no items at point, do not send a LEAVE event in this case */
		_new_current_item = 0;

	} else {

		if (within_items.front() == _current_item) {
			/* uppermost item at point is already _current_item */
			DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, string_compose ("CURRENT ITEM %1/%2\n", _new_current_item->whatami(), _current_item->name));
			return;
		}

		_new_current_item = const_cast<Item*> (within_items.front());

		if (_new_current_item != _current_item) {
			deliver_enter_leave (point, state);
		}
	}

	if (_current_item) {
		DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, string_compose ("CURRENT ITEM %1/%2\n", _current_item->whatami(), _current_item->name));
	} else {
		DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, "--- no current item\n");
	}

}

/** Deliver a series of enter & leave events based on the pointer position being at window
 * coordinate @p point, and pointer @p state (modifier keys, etc)
 */
void
GtkCanvas::deliver_enter_leave (Duple const & point, int state)
{
	/* setup enter & leave event structures */

	Glib::RefPtr<Gdk::Window> win = get_window();

	if (!win) {
		return;
	}

	GdkEventCrossing enter_event;
	enter_event.type = GDK_ENTER_NOTIFY;
	enter_event.window = win->gobj();
	enter_event.send_event = 0;
	enter_event.subwindow = 0;
	enter_event.mode = GDK_CROSSING_NORMAL;
	enter_event.focus = FALSE;
	enter_event.state = state;

	/* Events delivered to canvas items are expected to be in canvas
	 * coordinates but @p point is in window coordinates.
	 */

	Duple c = window_to_canvas (point);
	enter_event.x = c.x;
	enter_event.y = c.y;

	GdkEventCrossing leave_event = enter_event;
	leave_event.type = GDK_LEAVE_NOTIFY;

	Item* i;
	GdkNotifyType enter_detail = GDK_NOTIFY_UNKNOWN;
	GdkNotifyType leave_detail = GDK_NOTIFY_UNKNOWN;
	vector<Item*> items_to_leave_virtual;
	vector<Item*> items_to_enter_virtual;

	if (_new_current_item == 0) {

		leave_detail = GDK_NOTIFY_UNKNOWN;

		if (_current_item) {

			/* no current item, so also send virtual leave events to the
			 * entire hierarchy for the current item
			 */

			for (i = _current_item->parent(); i ; i = i->parent()) {
				items_to_leave_virtual.push_back (i);
			}
		}

	} else if (_current_item == 0) {

		enter_detail = GDK_NOTIFY_UNKNOWN;

		/* no current item, so also send virtual enter events to the
		 * entire hierarchy for the new item
		 */

		for (i = _new_current_item->parent(); i ; i = i->parent()) {
			items_to_enter_virtual.push_back (i);
		}

	} else if (_current_item->is_descendant_of (*_new_current_item)) {

		/* move from descendant to ancestor (X: "_current_item is an
		 * inferior ("child") of _new_current_item")
		 *
		 * Deliver "virtual" leave notifications to all items in the
		 * hierarchy between current and new_current.
		 */

		for (i = _current_item->parent(); i && i != _new_current_item; i = i->parent()) {
			items_to_leave_virtual.push_back (i);
		}

		enter_detail = GDK_NOTIFY_INFERIOR;
		leave_detail = GDK_NOTIFY_ANCESTOR;

	} else if (_new_current_item->is_descendant_of (*_current_item)) {
		/* move from ancestor to descendant (X: "_new_current_item is
		 * an inferior ("child") of _current_item")
		 *
		 * Deliver "virtual" enter notifications to all items in the
		 * hierarchy between current and new_current.
		 */

		for (i = _new_current_item->parent(); i && i != _current_item; i = i->parent()) {
			items_to_enter_virtual.push_back (i);
		}

		enter_detail = GDK_NOTIFY_ANCESTOR;
		leave_detail = GDK_NOTIFY_INFERIOR;

	} else {

		Item const * common_ancestor = _current_item->closest_ancestor_with (*_new_current_item);

		/* deliver virtual leave events to everything between _current
		 * and common_ancestor.
		 */

		for (i = _current_item->parent(); i && i != common_ancestor; i = i->parent()) {
			items_to_leave_virtual.push_back (i);
		}

		/* deliver virtual enter events to everything between
		 * _new_current and common_ancestor.
		 */

		for (i = _new_current_item->parent(); i && i != common_ancestor; i = i->parent()) {
			items_to_enter_virtual.push_back (i);
		}

		enter_detail = GDK_NOTIFY_NONLINEAR;
		leave_detail = GDK_NOTIFY_NONLINEAR;
	}


	if (_current_item && !_current_item->ignore_events ()) {
		leave_event.detail = leave_detail;
		_current_item->Event ((GdkEvent*)&leave_event);
		DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, string_compose ("LEAVE %1/%2\n", _current_item->whatami(), _current_item->name));
	}

	if (_current_item == current_tooltip_item) {
		hide_tooltip ();
	}

	leave_event.detail = GDK_NOTIFY_VIRTUAL;
	enter_event.detail = GDK_NOTIFY_VIRTUAL;

	for (vector<Item*>::iterator it = items_to_leave_virtual.begin(); it != items_to_leave_virtual.end(); ++it) {
		if (!(*it)->ignore_events()) {
			DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, string_compose ("leave %1/%2\n", (*it)->whatami(), (*it)->name));
			(*it)->Event ((GdkEvent*)&leave_event);
		}
	}

	for (vector<Item*>::iterator it = items_to_enter_virtual.begin(); it != items_to_enter_virtual.end(); ++it) {
		if (!(*it)->ignore_events()) {
			DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, string_compose ("enter %1/%2\n", (*it)->whatami(), (*it)->name));
			(*it)->Event ((GdkEvent*)&enter_event);
			// std::cerr << "enter " << (*it)->whatami() << '/' << (*it)->name << std::endl;
		}
	}

	if (_new_current_item && !_new_current_item->ignore_events()) {
		enter_event.detail = enter_detail;
		DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, string_compose ("ENTER %1/%2\n", _new_current_item->whatami(), _new_current_item->name));
		start_tooltip_timeout (_new_current_item);
		_new_current_item->Event ((GdkEvent*)&enter_event);
	}

	_current_item = _new_current_item;
}


/** Deliver an event to the appropriate item; either the grabbed item, or
 *  one of the items underneath the event.
 *  @param point Position that the event has occurred at, in canvas coordinates.
 *  @param event The event.
 */
bool
GtkCanvas::deliver_event (GdkEvent* event)
{
	/* Point in in canvas coordinate space */

	const Item* event_item;

	if (_grabbed_item) {
		/* we have a grabbed item, so everything gets sent there */
		DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("%1 %2 (%3) was grabbed, send event there\n",
								       _grabbed_item, _grabbed_item->whatami(), _grabbed_item->name));
		event_item = _grabbed_item;
	} else {
		event_item = _current_item;
	}

	if (!event_item) {
		return false;
	}

	/* run through the items from child to parent, until one claims the event */

	Item* item = const_cast<Item*> (event_item);

	while (item) {

		Item* parent = item->parent ();

		if (!item->ignore_events () &&
		    item->Event (event)) {
			/* this item has just handled the event */
			DEBUG_TRACE (
				PBD::DEBUG::CanvasEvents,
				string_compose ("canvas event handled by %1 %2\n", item->whatami(), item->name.empty() ? "[unknown]" : item->name)
				);

			return true;
		}

		DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("canvas event %3 left unhandled by %1 %2\n", item->whatami(), item->name.empty() ? "[unknown]" : item->name, event_type_string (event->type)));

		if ((item = parent) == 0) {
			break;
		}

	}

	return false;
}

void
GtkCanvas::item_shown_or_hidden (Item* item)
{
	if (item == current_tooltip_item) {
		stop_tooltip_timeout ();
	}
	Canvas::item_shown_or_hidden (item);
}

/** Called when an item is being destroyed.
 *  @param item Item being destroyed.
 *  @param bounding_box Last known bounding box of the item.
 */
void
GtkCanvas::item_going_away (Item* item, Rect bounding_box)
{
	if (bounding_box) {
		queue_draw_item_area (item, bounding_box);
	}

	if (_new_current_item == item) {
		_new_current_item = 0;
	}

	if (_grabbed_item == item) {
		_grabbed_item = 0;
	}

	if (_focused_item == item) {
		_focused_item = 0;
	}

	if (current_tooltip_item) {
		current_tooltip_item = 0;
		stop_tooltip_timeout ();
	}

	ScrollGroup* sg = dynamic_cast<ScrollGroup*>(item);
	if (sg) {
		scrollers.remove (sg);
	}

	if (_current_item == item) {
		/* no need to send a leave event to this item, since it is going away
		 */
		_current_item = 0;
		pick_current_item (0); // no mouse state
	}

}

void
GtkCanvas::on_realize ()
{
	Gtk::EventBox::on_realize();
#ifdef __APPLE__
	if (_nsglview) {
		Gtkmm2ext::nsglview_overlay (_nsglview, get_window()->gobj());
	}
#endif

	_root.set_fill (false);
	_root.set_outline (false);
}

void
GtkCanvas::on_size_allocate (Gtk::Allocation& a)
{
	EventBox::on_size_allocate (a);

	if (_use_image_surface) {
		_canvas_image.clear ();
		_canvas_image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, a.get_width(), a.get_height());
	}

#ifdef __APPLE__
	if (_nsglview) {
		gint xx, yy;
		gtk_widget_translate_coordinates(
				GTK_WIDGET(gobj()),
				GTK_WIDGET(get_toplevel()->gobj()),
				0, 0, &xx, &yy);
		Gtkmm2ext::nsglview_resize (_nsglview, xx, yy, a.get_width(), a.get_height());
	}
#endif

	/* call to ensure that entire canvas is marked in the invalidation region */
	queue_draw ();

	/* x, y in a are relative to the parent. When passing this down to the
	   root group, this origin is effectively 0,0
	*/

	Rect r (0., 0., a.get_width(), a.get_height());
	_root.size_allocate (r);
}

/** Handler for GDK expose events.
 *  @param ev Event.
 *  @return true if the event was handled.
 */
bool
GtkCanvas::on_expose_event (GdkEventExpose* ev)
{
	if (_in_dtor) {
		return true;
	}
#ifdef __APPLE__
	if (_nsglview) {
		return true;
	}
#endif

#ifdef CANVAS_PROFILE
	const int64_t start = g_get_monotonic_time ();
#endif

	Cairo::RefPtr<Cairo::Context> draw_context;
	if (_use_image_surface) {
		if (!_canvas_image) {
			_canvas_image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, get_width(), get_height());
		}
		draw_context = Cairo::Context::create (_canvas_image);
	} else {
		draw_context = get_window()->create_cairo_context ();
	}

	draw_context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	draw_context->clip();

	/* (this comment applies to macOS, but is other platforms
	 * also benefit from using CPU-rendering on a image-surface
	 * with a final bitblt).
	 *
	 * group calls cairo_quartz_surface_create() which
	 * effectively uses a CGBitmapContext + image-surface
	 *
	 * This avoids expensive argb32_image_mark_image() during drawing.
	 * Although the final paint() operation still takes the slow path
	 * through image_mark_image instead of ColorMaskCopyARGB888_sse :(
	 *
	 * profiling indicates a speed up of factor 2. (~ 5-10ms render time,
	 * instead of 10-20ms, which is still slow compared to XCB and win32 surfaces (~0.2 ms)
	 *
	 * Fixing this for good likely involves changes to GdkQuartzWindow, GdkQuartzView
	 */
	if (_use_intermediate_surface && !_use_image_surface) {
		draw_context->push_group ();
	}

	/* render canvas */
	if (_single_exposure) {

		/* draw background color */
		draw_context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
		Gtkmm2ext::set_source_rgba (draw_context, _bg_color);
		draw_context->fill ();

		Canvas::render (Rect (ev->area.x, ev->area.y, ev->area.x + ev->area.width, ev->area.y + ev->area.height), draw_context);

	} else {
		GdkRectangle* rects;
		gint nrects;

		gdk_region_get_rectangles (ev->region, &rects, &nrects);

		for (gint n = 0; n < nrects; ++n) {
			draw_context->set_identity_matrix();  //reset the cairo matrix, just in case someone left it transformed after drawing ( cough )

			/* draw background color */
			draw_context->rectangle (rects[n].x, rects[n].y, rects[n].x + rects[n].width, rects[n].y + rects[n].height);
			Gtkmm2ext::set_source_rgba (draw_context, _bg_color);
			draw_context->fill ();

			Canvas::render (Rect (rects[n].x, rects[n].y, rects[n].x + rects[n].width, rects[n].y + rects[n].height), draw_context);
		}

		g_free (rects);
	}

	if (_use_image_surface) {
		_canvas_image->flush ();
		Cairo::RefPtr<Cairo::Context> window_context = get_window()->create_cairo_context ();
		window_context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
		window_context->clip ();
		window_context->set_source (_canvas_image, 0, 0);
		window_context->set_operator (Cairo::OPERATOR_SOURCE);
		window_context->paint ();
	} else if (_use_intermediate_surface) {
		draw_context->pop_group_to_source ();
		draw_context->paint ();
	}


#ifdef CANVAS_PROFILE
	const int64_t end = g_get_monotonic_time ();
	const int64_t elapsed = end - start;
	printf ("GtkCanvas::on_expose_event %f ms\n", elapsed / 1000.f);
#endif

	return true;
}

void
GtkCanvas::prepare_for_render () const
{
	Rect window_bbox = visible_area ();
	Canvas::prepare_for_render (window_bbox);
}

/** Handler for GDK scroll events.
 *  @param ev Event.
 *  @return true if the event was handled.
 */
bool
GtkCanvas::on_scroll_event (GdkEventScroll* ev)
{
	/* translate event coordinates from window to canvas */

	GdkEvent copy = *((GdkEvent*)ev);
	Duple winpos = Duple (ev->x, ev->y);
	Duple where = window_to_canvas (winpos);

	pick_current_item (winpos, ev->state);

	copy.button.x = where.x;
	copy.button.y = where.y;

	/* Coordinates in the event will be canvas coordinates, correctly adjusted
	   for scroll if this GtkCanvas is in a GtkCanvasViewport.
	*/

	DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("canvas scroll @ %1, %2 => %3\n", ev->x, ev->y, where));
	return deliver_event (reinterpret_cast<GdkEvent*>(&copy));
}

void
GtkCanvas::on_style_changed (const Glib::RefPtr<Gtk::Style>& style)
{
	EventBox::on_style_changed (style);
	/* call to ensure that entire canvas is marked in the invalidation region */
	queue_draw ();
}

bool
GtkCanvas::on_visibility_notify_event (GdkEventVisibility* ev)
{
	bool ret = EventBox::on_visibility_notify_event (ev);
	/* call to ensure that entire canvas is marked in the invalidation region */
	queue_draw ();
	return ret;
}

/** Handler for GDK key press events.
 *  @param ev Event.
 *  @return true if the event was handled.
 */
bool
GtkCanvas::on_key_press_event (GdkEventKey* ev)
{
	DEBUG_TRACE (PBD::DEBUG::CanvasEvents, "canvas key press\n");
	return deliver_event (reinterpret_cast<GdkEvent*>(ev));
}

/** Handler for GDK key release events.
 *  @param ev Event.
 *  @return true if the event was handled.
 */
bool
GtkCanvas::on_key_release_event (GdkEventKey* ev)
{
	DEBUG_TRACE (PBD::DEBUG::CanvasEvents, "canvas key release\n");
	return deliver_event (reinterpret_cast<GdkEvent*>(ev));
}

/** Handler for GDK button press events.
 *  @param ev Event.
 *  @return true if the event was handled.
 */
bool
GtkCanvas::on_button_press_event (GdkEventButton* ev)
{
	/* translate event coordinates from window to canvas */

	GdkEvent copy = *((GdkEvent*)ev);
	Duple winpos = Duple (ev->x, ev->y);
	Duple where = window_to_canvas (winpos);

	pick_current_item (winpos, ev->state);

	copy.button.x = where.x;
	copy.button.y = where.y;

	/* Coordinates in the event will be canvas coordinates, correctly adjusted
	   for scroll if this GtkCanvas is in a GtkCanvasViewport.
	*/

	DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("canvas button press %1 @ %2, %3 => %4\n", ev->button, ev->x, ev->y, where));
	return deliver_event (reinterpret_cast<GdkEvent*>(&copy));
}

/** Handler for GDK button release events.
 *  @param ev Event.
 *  @return true if the event was handled.
 */
bool
GtkCanvas::on_button_release_event (GdkEventButton* ev)
{
	/* translate event coordinates from window to canvas */

	GdkEvent copy = *((GdkEvent*)ev);
	Duple winpos = Duple (ev->x, ev->y);
	Duple where = window_to_canvas (winpos);

	pick_current_item (winpos, ev->state);

	copy.button.x = where.x;
	copy.button.y = where.y;

	/* Coordinates in the event will be canvas coordinates, correctly adjusted
	   for scroll if this GtkCanvas is in a GtkCanvasViewport.
	*/

	DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("canvas button release %1 @ %2, %3 => %4\n", ev->button, ev->x, ev->y, where));
	return deliver_event (reinterpret_cast<GdkEvent*>(&copy));
}

bool
GtkCanvas::get_mouse_position (Duple& winpos) const
{
	int x;
	int y;
	Gdk::ModifierType mask;
	Glib::RefPtr<Gdk::Window> self = Glib::RefPtr<Gdk::Window>::cast_const (get_window ());

	if (!self) {
		winpos = Duple (0, 0);
		return false;
	}

	Glib::RefPtr<Gdk::Window> win = self->get_pointer (x, y, mask);

	winpos.x = x;
	winpos.y = y;

	return true;
}

/** Handler for GDK motion events.
 *  @param ev Event.
 *  @return true if the event was handled.
 */
bool
GtkCanvas::on_motion_notify_event (GdkEventMotion* ev)
{
	/* translate event coordinates from window to canvas */

	GdkEvent copy = *((GdkEvent*)ev);
	Duple point (ev->x, ev->y);
	Duple where = window_to_canvas (point);

	copy.motion.x = where.x;
	copy.motion.y = where.y;

	/* Coordinates in "copy" will be canvas coordinates,
	*/

	DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("canvas motion @ %1, %2 canvas @ %3, %4\n", ev->x, ev->y, copy.motion.x, copy.motion.y));

	MouseMotion (point); /* EMIT SIGNAL */

	pick_current_item (point, ev->state);

	/* Now deliver the motion event.  It may seem a little inefficient
	   to recompute the items under the event, but the enter notify/leave
	   events may have deleted canvas items so it is important to
	   recompute the list in deliver_event.
	*/

	return deliver_event (reinterpret_cast<GdkEvent*> (&copy));
}

bool
GtkCanvas::on_enter_notify_event (GdkEventCrossing* ev)
{
	pick_current_item (Duple (ev->x, ev->y), ev->state);
	return true;
}

bool
GtkCanvas::on_leave_notify_event (GdkEventCrossing* ev)
{
	switch (ev->detail) {
	case GDK_NOTIFY_ANCESTOR:
	case GDK_NOTIFY_UNKNOWN:
	case GDK_NOTIFY_VIRTUAL:
	case GDK_NOTIFY_NONLINEAR:
	case GDK_NOTIFY_NONLINEAR_VIRTUAL:
		/* leaving window, cancel any tooltips */
		stop_tooltip_timeout ();
		hide_tooltip ();
		break;
	default:
		/* we don't care about any other kind
		   of leave event (notably GDK_NOTIFY_INFERIOR)
		*/
		break;
	}
	_new_current_item = 0;
	deliver_enter_leave (Duple (ev->x, ev->y), ev->state);
	return true;
}

void
GtkCanvas::on_map ()
{
	Gtk::EventBox::on_map();
#ifdef __APPLE__
	if (_nsglview) {
		Gtkmm2ext::nsglview_set_visible (_nsglview, true);
		Gtk::Allocation a = get_allocation();
		gint xx, yy;
		gtk_widget_translate_coordinates(
				GTK_WIDGET(gobj()),
				GTK_WIDGET(get_toplevel()->gobj()),
				0, 0, &xx, &yy);
		Gtkmm2ext::nsglview_resize (_nsglview, xx, yy, a.get_width(), a.get_height());
	}
#endif
}

void
GtkCanvas::on_unmap ()
{
	stop_tooltip_timeout ();
	Gtk::EventBox::on_unmap();
#ifdef __APPLE__
	if (_nsglview) {
		Gtkmm2ext::nsglview_set_visible (_nsglview, false);
	}
#endif
}

void
GtkCanvas::queue_draw()
{
#ifdef __APPLE__
	if (_nsglview) {
		Gtkmm2ext::nsglview_queue_draw (_nsglview, 0, 0, get_width (), get_height ());
		return;
	}
#endif
	Gtk::Widget::queue_draw ();
}

void
GtkCanvas::queue_draw_area (int x, int y, int width, int height)
{
#ifdef __APPLE__
	if (_nsglview) {
		Gtkmm2ext::nsglview_queue_draw (_nsglview, x, y, width, height);
		return;
	}
#endif
	Gtk::Widget::queue_draw_area (x, y, width, height);
}

/** Called to request a redraw of our canvas.
 *  @param area Area to redraw, in window coordinates.
 */
void
GtkCanvas::request_redraw (Rect const & request)
{
	if (_in_dtor) {
		return;
	}

	/* clamp area requested to actual visible window */

	Rect real_area = request.intersection (visible_area());

	if (real_area) {
		if (real_area.width () && real_area.height ()) {
			// Item intersects with visible canvas area
			queue_draw_area (real_area.x0, real_area.y0, real_area.width(), real_area.height());
		}

	} else {
		// Item does not intersect with visible canvas area
	}
}

/** Called to request that we try to get a particular size for ourselves.
 *  @param size Size to request, in pixels.
 */
void
GtkCanvas::request_size (Duple size)
{
	Duple req = size;

	if (req.x > INT_MAX) {
		req.x = INT_MAX;
	}

	if (req.y > INT_MAX) {
		req.y = INT_MAX;
	}

	set_size_request (req.x, req.y);
}

/** `Grab' an item, so that all events are sent to that item until it is `ungrabbed'.
 *  This is typically used for dragging items around, so that they are grabbed during
 *  the drag.
 *  @param item Item to grab.
 */
void
GtkCanvas::grab (Item* item)
{
	/* XXX: should this be doing gdk_pointer_grab? */
	_grabbed_item = item;
}


/** `Ungrab' any item that was previously grabbed */
void
GtkCanvas::ungrab ()
{
	/* XXX: should this be doing gdk_pointer_ungrab? */
	_grabbed_item = nullptr;
}

/** Set keyboard focus on an item, so that all keyboard events are sent to that item until the focus
 *  moves elsewhere.
 *  @param item Item to grab.
 */
void
GtkCanvas::focus (Item* item)
{
	_focused_item = item;
}

void
GtkCanvas::unfocus (Item* item)
{
	if (item == _focused_item) {
		_focused_item = 0;
	}
}

/** @return The visible area of the canvas, in window coordinates */
ArdourCanvas::Rect
GtkCanvas::visible_area () const
{
	return Rect (0, 0, get_allocation().get_width (), get_allocation().get_height ());
}

Coord
GtkCanvas::width() const
{
	return get_allocation().get_width();
}

Coord
GtkCanvas::height() const
{
	return get_allocation().get_height();
}

void
GtkCanvas::start_tooltip_timeout (Item* item)
{
	stop_tooltip_timeout ();

	if (item && !item->tooltip().empty() && Gtkmm2ext::PersistentTooltip::tooltips_enabled ()) {
		current_tooltip_item = item;

		/* wait for the first idle that happens after this is
		   called. this means that we've stopped processing events, which
		   in turn implies that the user has stopped doing stuff for a
		   little while.
		*/

		Glib::signal_idle().connect (sigc::mem_fun (*this, &GtkCanvas::really_start_tooltip_timeout));
	}
}

bool
GtkCanvas::really_start_tooltip_timeout ()
{
	/* an idle has occurred since we entered a tooltip-bearing widget. Now
	 * wait 1 second and if the timeout isn't cancelled, show the tooltip.
	 */

	if (current_tooltip_item) {
		tooltip_timeout_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &GtkCanvas::show_tooltip), tooltip_timeout_msecs);
	}

	return false; /* this is called from an idle callback, don't call it again */
}

void
GtkCanvas::stop_tooltip_timeout ()
{
	current_tooltip_item = 0;
	tooltip_timeout_connection.disconnect ();
}

bool
GtkCanvas::show_tooltip ()
{
	Rect tooltip_item_bbox;

	if (!current_tooltip_item || current_tooltip_item->tooltip().empty() || !current_tooltip_item->bounding_box()) {
		return false;
	}

	if (!tooltip_window) {
		tooltip_window = new Gtk::Window (Gtk::WINDOW_POPUP);
		tooltip_label = manage (new Gtk::Label);
		tooltip_label->show ();
		tooltip_window->add (*tooltip_label);
		tooltip_window->set_border_width (1);
		tooltip_window->set_name ("tooltip");
	}

	tooltip_label->set_text (current_tooltip_item->tooltip());

	/* figure out where to position the tooltip */

	Gtk::Widget* toplevel = get_toplevel();
	assert (toplevel);
	int pointer_x, pointer_y;
	Gdk::ModifierType mask;

	(void) toplevel->get_window()->get_pointer (pointer_x, pointer_y, mask);

	Duple tooltip_window_origin (pointer_x, pointer_y);

	/* convert to root window coordinates */

	int win_x, win_y;
	dynamic_cast<Gtk::Window*>(toplevel)->get_position (win_x, win_y);

	tooltip_window_origin = tooltip_window_origin.translate (Duple (win_x, win_y));

	/* we don't want the pointer to be inside the window when it is
	 * displayed, because then we generate a leave/enter event pair when
	 * the window is displayed then hidden - the enter event will
	 * trigger a new tooltip timeout.
	 *
	 * So move the window right of the pointer position by just a enough
	 * to get it away from the pointer.
	 */

	tooltip_window_origin.x += 30;
	tooltip_window_origin.y += 45;

	/* move the tooltip window into position */

	tooltip_window->move (tooltip_window_origin.x, tooltip_window_origin.y);

	/* ready to show */

	tooltip_window->present ();

	/* called from a timeout handler, don't call it again */

	return false;
}

void
GtkCanvas::hide_tooltip ()
{
	/* hide it if its there */

	if (tooltip_window) {
		tooltip_window->hide ();

		// Delete the tooltip window so it'll get re-created
		// (i.e. properly re-sized) on the next usage.
		delete tooltip_window;
		tooltip_window = NULL;
	}
}

Glib::RefPtr<Pango::Context>
GtkCanvas::get_pango_context ()
{
	return Glib::wrap (gdk_pango_context_get());
}

void
GtkCanvas::queue_resize ()
{
	if (!resize_queued) {
		Glib::signal_idle().connect (sigc::mem_fun (*this, &GtkCanvas::resize_handler));
		resize_queued = true;
	}
}

bool
GtkCanvas::resize_handler ()
{
	resize_queued = false;
	_root.layout ();
	return false;
}

bool
GtkCanvas::grab_can_translate () const
{
	if (!_grabbed_item) {
		/* weird, but correct! */
		return true;
	}

	return _grabbed_item->scroll_translation ();
}

void
GtkCanvas::render (Cairo::RefPtr<Cairo::Context> const & ctx, cairo_rectangle_t* r)
{
	ArdourCanvas::Rect rect (r->x, r->y, r->width + r->x, r->height + r->y);
	Canvas::render (rect, ctx);
}

/** Create a GtkCanvaSViewport.
 *  @param hadj Adjustment to use for horizontal scrolling.
 *  @param vadj Adjustment to use for vertica scrolling.
 */
GtkCanvasViewport::GtkCanvasViewport (Gtk::Adjustment& hadj, Gtk::Adjustment& vadj)
	: Alignment (0, 0, 1.0, 1.0)
	, hadjustment (hadj)
	, vadjustment (vadj)
{
	add (_canvas);

	hadj.signal_value_changed().connect (sigc::mem_fun (*this, &GtkCanvasViewport::scrolled));
	vadj.signal_value_changed().connect (sigc::mem_fun (*this, &GtkCanvasViewport::scrolled));
}

void
GtkCanvasViewport::scrolled ()
{
	_canvas.scroll_to (hadjustment.get_value(), vadjustment.get_value());
	queue_draw ();
}

/** Handler for when GTK asks us what minimum size we want.
 *  @param req Requsition to fill in.
 */
void
GtkCanvasViewport::on_size_request (Gtk::Requisition* req)
{
	Distance width;
	Distance height;

	_canvas.root()->size_request (width, height);
	_canvas.request_size (Duple (width, height));

	/* special case ArdourCanvas::COORD_MAX (really: no size constraint),
	 * also limit to cairo constraints determined by coordinates of things
	 * sent to pixman being in 16.16 format. */

	if (width > 32767) {
		width = 0;
	}
	if (height > 32767) {
		height = 0;
	}

	req->width  = std::max<int>(1, width);
	req->height = std::max<int>(1, height);

}
