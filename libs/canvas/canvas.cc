/*
    Copyright (C) 2011 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#if !defined USE_CAIRO_IMAGE_SURFACE && !defined NDEBUG
#define OPTIONAL_CAIRO_IMAGE_SURFACE
#endif

/** @file  canvas/canvas.cc
 *  @brief Implementation of the main canvas classes.
 */

#include <list>
#include <cassert>
#include <gtkmm/adjustment.h>
#include <gtkmm/label.h>
#include <gtkmm/window.h>

#include "pbd/compose.h"
#include "pbd/stacktrace.h"

#include "canvas/canvas.h"
#include "canvas/colors.h"
#include "canvas/debug.h"
#include "canvas/line.h"
#include "canvas/scroll_group.h"
#include "canvas/utils.h"

using namespace std;
using namespace ArdourCanvas;

uint32_t Canvas::tooltip_timeout_msecs = 750;

/** Construct a new Canvas */
Canvas::Canvas ()
	: _root (this)
        , _bg_color (rgba_to_color (0, 1.0, 0.0, 1.0))
{
	set_epoch ();
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

/** Render an area of the canvas.
 *  @param area Area in window coordinates.
 *  @param context Cairo context to render to.
 */
void
Canvas::render (Rect const & area, Cairo::RefPtr<Cairo::Context> const & context) const
{
#ifdef CANVAS_DEBUG
	if (DEBUG_ENABLED(PBD::DEBUG::CanvasRender)) {
		cerr << this << " RENDER: " << area << endl;
		//cerr << "CANVAS @ " << this << endl;
		//dump (cerr);
		//cerr << "-------------------------\n";
	}
#endif

	render_count = 0;
	
	boost::optional<Rect> root_bbox = _root.bounding_box();
	if (!root_bbox) {
		/* the root has no bounding box, so there's nothing to render */
		return;
	}

	boost::optional<Rect> draw = root_bbox->intersection (area);
	if (draw) {
		
		/* there's a common area between the root and the requested
		   area, so render it.
		*/

		_root.render (*draw, context);

#if defined CANVAS_DEBUG && !PLATFORM_WINDOWS
		if (getenv ("CANVAS_HARLEQUIN_DEBUGGING")) {
			// This transparently colors the rect being rendered, after it has been drawn.
			double r = (random() % 65536) /65536.0;
			double g = (random() % 65536) /65536.0;
			double b = (random() % 65536) /65536.0;
			context->rectangle (draw->x0, draw->y0, draw->x1 - draw->x0, draw->y1 - draw->y0);
			context->set_source_rgba (r, g, b, 0.25);
			context->fill ();
		}
#endif
	}

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

/** Called when an item has been shown or hidden.
 *  @param item Item that has been shown or hidden.
 */
void
Canvas::item_shown_or_hidden (Item* item)
{
	boost::optional<Rect> bbox = item->bounding_box ();
	if (bbox) {
		if (item->item_to_window (*bbox).intersection (visible_area ())) {
			queue_draw_item_area (item, bbox.get ());
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
	boost::optional<Rect> bbox = item->bounding_box ();
	if (bbox) {
		if (item->item_to_window (*bbox).intersection (visible_area ())) {
			queue_draw_item_area (item, bbox.get ());
		}
	}
}

/** Called when an item has changed, but not moved.
 *  @param item Item that has changed.
 *  @param pre_change_bounding_box The bounding box of item before the change,
 *  in the item's coordinates.
 */
void
Canvas::item_changed (Item* item, boost::optional<Rect> pre_change_bounding_box)
{
	
	Rect window_bbox = visible_area ();

	if (pre_change_bounding_box) {

		if (item->item_to_window (*pre_change_bounding_box).intersection (window_bbox)) {
			/* request a redraw of the item's old bounding box */
			queue_draw_item_area (item, pre_change_bounding_box.get ());
		}
	}

	boost::optional<Rect> post_change_bounding_box = item->bounding_box ();
	if (post_change_bounding_box) {
		
		if (item->item_to_window (*post_change_bounding_box).intersection (window_bbox)) {
			/* request a redraw of the item's new bounding box */
			queue_draw_item_area (item, post_change_bounding_box.get ());
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

	if (best_group) {
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
Canvas::item_moved (Item* item, boost::optional<Rect> pre_change_parent_bounding_box)
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
		queue_draw_item_area (item->parent(), pre_change_parent_bounding_box.get ());
	}

	boost::optional<Rect> post_change_bounding_box = item->bounding_box ();
	if (post_change_bounding_box) {
		/* request a redraw of where the item now is */
		queue_draw_item_area (item, post_change_bounding_box.get ());
	}
}

/** Request a redraw of a particular area in an item's coordinates.
 *  @param item Item.
 *  @param area Area to redraw in the item's coordinates.
 */
void
Canvas::queue_draw_item_area (Item* item, Rect area)
{
	request_redraw (item->item_to_window (area));
}

void
Canvas::set_tooltip_timeout (uint32_t msecs)
{
	tooltip_timeout_msecs = msecs;
}

void
Canvas::set_background_color (Color c)
{
        _bg_color = c;

        boost::optional<Rect> r = _root.bounding_box();

        if (r) {
                request_redraw (_root.item_to_window (r.get()));
        }
}

void
GtkCanvas::re_enter ()
{
	DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, "re-enter canvas by request\n");
	_current_item = 0;
	pick_current_item (0);
}

/** Construct a GtkCanvas */
GtkCanvas::GtkCanvas ()
	: _current_item (0)
	, _new_current_item (0)
	, _grabbed_item (0)
	, _focused_item (0)
	, _single_exposure (1)
	, current_tooltip_item (0)
	, tooltip_window (0)
{
	/* these are the events we want to know about */
	add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK |
		    Gdk::SCROLL_MASK | Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK |
		    Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
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

/** Given @param point (a position in window coordinates)
 *  and mouse state @param state, check to see if _current_item
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
		for (vector<Item const*>::const_iterator it = items.begin(); it != items.end(); ++it) {
#ifdef CANVAS_DEBUG
			std::cerr << "\tItem " << (*it)->whatami() << '/' << (*it)->name << " ignore events ? " << (*it)->ignore_events() << " vis ? " << (*it)->visible() << std::endl;
#else
			std::cerr << "\tItem " << (*it)->whatami() << '/' << " ignore events ? " << (*it)->ignore_events() << " vis ? " << (*it)->visible() << std::endl;
#endif
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

		/* no items at point, just send leave event below */
		_new_current_item = 0;

	} else {

		if (within_items.front() == _current_item) {
			/* uppermost item at point is already _current_item */
			DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, string_compose ("CURRENT ITEM %1/%2\n", _new_current_item->whatami(), _current_item->name));
			return;
		}
	
		_new_current_item = const_cast<Item*> (within_items.front());
	}

	if (_new_current_item != _current_item) {
		deliver_enter_leave (point, state);
	}

	if (_current_item) {
		DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, string_compose ("CURRENT ITEM %1/%2\n", _new_current_item->whatami(), _current_item->name));
	} else {
		DEBUG_TRACE (PBD::DEBUG::CanvasEnterLeave, "--- no current item\n");
	}

}

/** Deliver a series of enter & leave events based on the pointer position being at window
 * coordinate @param point, and pointer @param state (modifier keys, etc)
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
	 * coordinates but @param point is in window coordinates.
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
			 * entire heirarchy for the current item
			 */

			for (i = _current_item->parent(); i ; i = i->parent()) {
				items_to_leave_virtual.push_back (i);
			}
		}

	} else if (_current_item == 0) {

		enter_detail = GDK_NOTIFY_UNKNOWN;

		/* no current item, so also send virtual enter events to the
		 * entire heirarchy for the new item 
		 */

		for (i = _new_current_item->parent(); i ; i = i->parent()) {
			items_to_enter_virtual.push_back (i);
		}

	} else if (_current_item->is_descendant_of (*_new_current_item)) {

		/* move from descendant to ancestor (X: "_current_item is an
		 * inferior ("child") of _new_current_item") 
		 *
		 * Deliver "virtual" leave notifications to all items in the
		 * heirarchy between current and new_current.
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
		 * heirarchy between current and new_current.
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

/** Called when an item is being destroyed.
 *  @param item Item being destroyed.
 *  @param bounding_box Last known bounding box of the item.
 */
void
GtkCanvas::item_going_away (Item* item, boost::optional<Rect> bounding_box)
{
	if (bounding_box) {
		queue_draw_item_area (item, bounding_box.get ());
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
GtkCanvas::on_size_allocate (Gtk::Allocation& a)
{
	EventBox::on_size_allocate (a);
#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	if (getenv("ARDOUR_IMAGE_SURFACE")) {
#endif
#if defined USE_CAIRO_IMAGE_SURFACE || defined OPTIONAL_CAIRO_IMAGE_SURFACE
	/* allocate an image surface as large as the canvas itself */

	canvas_image.clear ();
	canvas_image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, a.get_width(), a.get_height());
#endif
#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	}
#endif
}

/** Handler for GDK expose events.
 *  @param ev Event.
 *  @return true if the event was handled.
 */
bool
GtkCanvas::on_expose_event (GdkEventExpose* ev)
{
#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	Cairo::RefPtr<Cairo::Context> draw_context;
	Cairo::RefPtr<Cairo::Context> window_context;
	if (getenv("ARDOUR_IMAGE_SURFACE")) {
		if (!canvas_image) {
			canvas_image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, get_width(), get_height());
		}
		draw_context = Cairo::Context::create (canvas_image);
		window_context = get_window()->create_cairo_context ();
	} else {
		draw_context = get_window()->create_cairo_context ();
	}
#elif defined USE_CAIRO_IMAGE_SURFACE
	if (!canvas_image) {
		canvas_image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, get_width(), get_height());
	}
        Cairo::RefPtr<Cairo::Context> draw_context = Cairo::Context::create (canvas_image);
	Cairo::RefPtr<Cairo::Context> window_context = get_window()->create_cairo_context ();
#else 
	Cairo::RefPtr<Cairo::Context> draw_context = get_window()->create_cairo_context ();
#endif

        /* draw background color */
        
        draw_context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
        draw_context->clip_preserve ();
        set_source_rgba (draw_context, _bg_color);
        draw_context->fill ();
        
        /* render canvas */
		if ( _single_exposure ) {
	
			render (Rect (ev->area.x, ev->area.y, ev->area.x + ev->area.width, ev->area.y + ev->area.height), draw_context);

		} else {
			GdkRectangle* rects;
			gint nrects;
			
			gdk_region_get_rectangles (ev->region, &rects, &nrects);
			for (gint n = 0; n < nrects; ++n) {
				draw_context->set_identity_matrix();  //reset the cairo matrix, just in case someone left it transformed after drawing ( cough )
				render (Rect (rects[n].x, rects[n].y, rects[n].x + rects[n].width, rects[n].y + rects[n].height), draw_context);
			}
			g_free (rects);
		}
		
#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	if (getenv("ARDOUR_IMAGE_SURFACE")) {
#endif
#if defined USE_CAIRO_IMAGE_SURFACE || defined OPTIONAL_CAIRO_IMAGE_SURFACE
	/* now blit our private surface back to the GDK one */

	window_context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	window_context->clip ();
	window_context->set_source (canvas_image, 0, 0);
	window_context->set_operator (Cairo::OPERATOR_SOURCE);
	window_context->paint ();
#endif
#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	}
#endif

	return true;
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

	DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("canvas button press @ %1, %2 => %3\n", ev->x, ev->y, where));
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

	DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("canvas button release @ %1, %2 => %3\n", ev->x, ev->y, where));
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
		std::cerr << " no self window\n";
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
	hide_tooltip ();

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

/** Called to request a redraw of our canvas.
 *  @param area Area to redraw, in window coordinates.
 */
void
GtkCanvas::request_redraw (Rect const & request)
{
	Rect real_area;

	Coord const w = width ();
	Coord const h = height ();

	/* clamp area requested to actual visible window */

	real_area.x0 = max (0.0, min (w, request.x0));
	real_area.x1 = max (0.0, min (w, request.x1));
	real_area.y0 = max (0.0, min (h, request.y0));
	real_area.y1 = max (0.0, min (h, request.y1));

	queue_draw_area (real_area.x0, real_area.y0, real_area.width(), real_area.height());
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
	_grabbed_item = 0;
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
Rect
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

	if (item) {
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
	/* an idle has occured since we entered a tooltip-bearing widget. Now
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
	/* force the canvas to size itself */
	// _canvas.root()->bounding_box(); 

	req->width = 16;
	req->height = 16;
}

