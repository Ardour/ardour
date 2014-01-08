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

/** @file  canvas/canvas.cc
 *  @brief Implementation of the main canvas classes.
 */

#include <list>
#include <cassert>
#include <gtkmm/adjustment.h>
#include <gtkmm/label.h>

#include "pbd/compose.h"
#include "pbd/stacktrace.h"

#include "canvas/canvas.h"
#include "canvas/debug.h"

using namespace std;
using namespace ArdourCanvas;

/** Construct a new Canvas */
Canvas::Canvas ()
	: _root (this)
	, _scroll_offset_x (0)
	, _scroll_offset_y (0)
{
	set_epoch ();
}

void
Canvas::scroll_to (Coord x, Coord y)
{
	_scroll_offset_x = x;
	_scroll_offset_y = y;

	pick_current_item (0); // no current mouse position 
}

void
Canvas::zoomed ()
{
	pick_current_item (0); // no current mouse position
}

/** Render an area of the canvas.
 *  @param area Area in canvas coordinates.
 *  @param context Cairo context to render to.
 */
void
Canvas::render (Rect const & area, Cairo::RefPtr<Cairo::Context> const & context) const
{
#ifdef CANVAS_DEBUG
	if (DEBUG_ENABLED(PBD::DEBUG::CanvasRender)) {
		cerr << "RENDER: " << area << endl;
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

		// This outlines the rect being rendered, after it has been drawn.
		// context->rectangle (draw->x0, draw->y0, draw->x1 - draw->x0, draw->y1 - draw->y0);
		// context->set_source_rgba (1.0, 0, 0, 1.0);
		// context->stroke ();

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
		queue_draw_item_area (item, bbox.get ());
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
		queue_draw_item_area (item, bbox.get ());
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
	if (pre_change_bounding_box) {
		/* request a redraw of the item's old bounding box */
		queue_draw_item_area (item, pre_change_bounding_box.get ());
	}

	boost::optional<Rect> post_change_bounding_box = item->bounding_box ();
	if (post_change_bounding_box) {
		/* request a redraw of the item's new bounding box */
		queue_draw_item_area (item, post_change_bounding_box.get ());
	}
}

Duple
Canvas::window_to_canvas (Duple const & d) const
{
	return d.translate (Duple (_scroll_offset_x, _scroll_offset_y));
}

Duple
Canvas::canvas_to_window (Duple const & d) const
{
	Duple wd = d.translate (Duple (-_scroll_offset_x, -_scroll_offset_y));

	/* Note that this intentionally always returns integer coordinates */

	wd.x = round (wd.x);
	wd.y = round (wd.y);

	return wd;
}	

Rect
Canvas::window_to_canvas (Rect const & r) const
{
	return r.translate (Duple (_scroll_offset_x, _scroll_offset_y));
}

Rect
Canvas::canvas_to_window (Rect const & r) const
{
	Rect wr = r.translate (Duple (-_scroll_offset_x, -_scroll_offset_y));

	/* Note that this intentionally always returns integer coordinates */

	wr.x0 = round (wr.x0);
	wr.x1 = round (wr.x1);
	wr.y0 = round (wr.y0);
	wr.y1 = round (wr.y1);

	return wr;
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
	ArdourCanvas::Rect canvas_area = item->item_to_canvas (area);
	// cerr << "CANVAS " << this << " for " << item->whatami() << ' ' << item->name << " invalidate " << area << " TRANSLATE AS " << canvas_area << endl;
	request_redraw (canvas_area);
}

/** Construct a GtkCanvas */
GtkCanvas::GtkCanvas ()
	: _current_item (0)
	, _new_current_item (0)
	, _grabbed_item (0)
	, _focused_item (0)
{
	/* these are the events we want to know about */
	add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK |
		    Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK);
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

	pick_current_item (window_to_canvas (Duple (x, y)), state);
}
		
void
GtkCanvas::pick_current_item (Duple const & point, int state)
{
	/* we do not enter/leave items during a drag/grab */

	if (_grabbed_item) {
		return;
	}

	/* find the items at the given position */

	vector<Item const *> items;
	_root.add_items_at_point (point, items);

	/* put all items at point that are event-sensitive and visible and NOT
	   groups into within_items. Note that items is sorted from bottom to
	   top, but we're going to reverse that for within_items so that its
	   first item is the upper-most item that can be chosen as _current_item.
	*/
	
	vector<Item const *>::const_iterator i;
	list<Item const *> within_items;

	for (i = items.begin(); i != items.end(); ++i) {

		Item const * new_item = *i;

		/* We ignore invisible items, groups and items that ignore events */

		if (!new_item->visible() || new_item->ignore_events() || dynamic_cast<Group const *>(new_item) != 0) {
			continue;
		}
		
		within_items.push_front (new_item);
	}

	if (within_items.empty()) {

		/* no items at point, just send leave event below */

	} else {
		if (within_items.front() == _current_item) {
			/* uppermost item at point is already _current_item */
			return;
		}
		
		_new_current_item = const_cast<Item*> (within_items.front());
	}

	deliver_enter_leave (point, state);
}

void
GtkCanvas::deliver_enter_leave (Duple const & point, int state)
{
	/* setup enter & leave event structures */

	GdkEventCrossing enter_event;
	enter_event.type = GDK_ENTER_NOTIFY;
	enter_event.window = get_window()->gobj();
	enter_event.send_event = 0;
	enter_event.subwindow = 0;
	enter_event.mode = GDK_CROSSING_NORMAL;
	enter_event.focus = FALSE;
	enter_event.state = state;
	enter_event.x = point.x;
	enter_event.y = point.y;

	GdkEventCrossing leave_event = enter_event;
	leave_event.type = GDK_LEAVE_NOTIFY;

	Item* i;
	GdkNotifyType enter_detail;
	GdkNotifyType leave_detail;
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
		 * inferior of _new_current_item") 
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
		 * an inferior of _current_item")
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
		// std::cerr << "LEAVE " << _current_item->whatami() << '/' << _current_item->name << std::endl;
	}

	leave_event.detail = GDK_NOTIFY_VIRTUAL;
	enter_event.detail = GDK_NOTIFY_VIRTUAL;

	for (vector<Item*>::iterator it = items_to_leave_virtual.begin(); it != items_to_leave_virtual.end(); ++it) {
		if (!(*it)->ignore_events()) {
			(*it)->Event ((GdkEvent*)&leave_event);
			// std::cerr << "leave " << (*it)->whatami() << '/' << (*it)->name << std::endl;
		}
	}

	for (vector<Item*>::iterator it = items_to_enter_virtual.begin(); it != items_to_enter_virtual.end(); ++it) {
		if (!(*it)->ignore_events()) {
			(*it)->Event ((GdkEvent*)&enter_event);
			// std::cerr << "enter " << (*it)->whatami() << '/' << (*it)->name << std::endl;
		}
	}

	if (_new_current_item && !_new_current_item->ignore_events()) {
		enter_event.detail = enter_detail;
		_new_current_item->Event ((GdkEvent*)&enter_event);
		// std::cerr << "ENTER " << _new_current_item->whatami() << '/' << _new_current_item->name << std::endl;
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

	if (_grabbed_item) {
		/* we have a grabbed item, so everything gets sent there */
		DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("%1 %2 (%3) was grabbed, send event there\n",
								       _grabbed_item, _grabbed_item->whatami(), _grabbed_item->name));
		return _grabbed_item->Event (event);
	}

	if (!_current_item) {
		return false;
	}

	/* run through the items from child to parent, until one claims the event */

	Item* item = const_cast<Item*> (_current_item);
	
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
		
		DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("canvas event left unhandled by %1 %2\n", item->whatami(), item->name.empty() ? "[unknown]" : item->name));

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
	
	/* no need to send a leave event to this item, since it is going away 
	 */

	if (_new_current_item == item) {
		_new_current_item = 0;
	}

	if (_current_item == item) {
		_current_item = 0;
	}

	if (_grabbed_item == item) {
		_grabbed_item = 0;
	}

	if (_focused_item == item) {
		_focused_item = 0;
	}

	pick_current_item (0); // no mouse state
	
}

/** Handler for GDK expose events.
 *  @param ev Event.
 *  @return true if the event was handled.
 */
bool
GtkCanvas::on_expose_event (GdkEventExpose* ev)
{
	Cairo::RefPtr<Cairo::Context> c = get_window()->create_cairo_context ();

	render (Rect (ev->area.x, ev->area.y, ev->area.x + ev->area.width, ev->area.y + ev->area.height), c);

	return true;
}

/** @return Our Cairo context, or 0 if we don't have one */
Cairo::RefPtr<Cairo::Context>
GtkCanvas::context ()
{
	Glib::RefPtr<Gdk::Window> w = get_window ();
	if (!w) {
		return Cairo::RefPtr<Cairo::Context> ();
	}

	return w->create_cairo_context ();
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
	Duple where = window_to_canvas (Duple (ev->x, ev->y));

	copy.button.x = where.x;
	copy.button.y = where.y;
	
	/* Coordinates in the event will be canvas coordinates, correctly adjusted
	   for scroll if this GtkCanvas is in a GtkCanvasViewport.
	*/

	pick_current_item (where, ev->state);
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
	Duple where = window_to_canvas (Duple (ev->x, ev->y));
	
	pick_current_item (where, ev->state);

	copy.button.x = where.x;
	copy.button.y = where.y;

	/* Coordinates in the event will be canvas coordinates, correctly adjusted
	   for scroll if this GtkCanvas is in a GtkCanvasViewport.
	*/

	pick_current_item (where, ev->state);
	DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("canvas button release @ %1, %2 => %3\n", ev->x, ev->y, where));
	return deliver_event (reinterpret_cast<GdkEvent*>(&copy));
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

	// DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("canvas motion @ %1, %2\n", ev->x, ev->y));

	if (_grabbed_item) {
		/* if we have a grabbed item, it gets just the motion event,
		   since no enter/leave events can have happened.
		*/
		DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("%1 %2 (%3) was grabbed, send MOTION event there\n",
								       _grabbed_item, _grabbed_item->whatami(), _grabbed_item->name));
		return _grabbed_item->Event (reinterpret_cast<GdkEvent*> (&copy));
	}

	pick_current_item (where, ev->state);

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
	Duple where = window_to_canvas (Duple (ev->x, ev->y));
	pick_current_item (where, ev->state);
	return true;
}

bool
GtkCanvas::on_leave_notify_event (GdkEventCrossing* ev)
{
	_new_current_item = 0;
	Duple where = window_to_canvas (Duple (ev->x, ev->y));
	deliver_enter_leave (where, ev->state);
	return true;
}

/** Called to request a redraw of our canvas.
 *  @param area Area to redraw, in canvas coordinates.
 */
void
GtkCanvas::request_redraw (Rect const & request)
{
	Rect area = canvas_to_window (request);
	queue_draw_area (area.x0, area.y0, area.width(), area.height());
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

/** @return The visible area of the canvas, in canvas coordinates */
Rect
GtkCanvas::visible_area () const
{
	Distance const xo = _scroll_offset_x;
	Distance const yo = _scroll_offset_y;
	return Rect (xo, yo, xo + get_allocation().get_width (), yo + get_allocation().get_height ());
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

