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
	, _log_renders (true)
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
		cerr << "CANVAS @ " << this << endl;
		dump (cerr);
		cerr << "-------------------------\n";
	}
#endif

	// checkpoint ("render", "-> render");
	render_count = 0;
	
	context->save ();

#ifdef CANVAS_DEBUG
	if (getenv ("ARDOUR_REDRAW_CANVAS")) {
		/* light up the canvas to show redraws */
		context->set_source_rgba (random()%255 / 255.0,
					  random()%255 / 255.0,
					  random()%255 / 255.0,
					  255);
		context->rectangle (area.x0, area.y0, area.width(), area.height());
		context->stroke ();
	}
#endif

	/* clip to the requested area */
	context->rectangle (area.x0, area.y0, area.width(), area.height());
	context->clip ();

	boost::optional<Rect> root_bbox = _root.bounding_box();
	if (!root_bbox) {
		/* the root has no bounding box, so there's nothing to render */
		// checkpoint ("render", "no root bbox");
		context->restore ();
		return;
	}

	boost::optional<Rect> draw = root_bbox.get().intersection (area);
	if (draw) {
		/* there's a common area between the root and the requested
		   area, so render it.
		*/
		// checkpoint ("render", "... root");
		context->stroke ();

		_root.render (*draw, context);
	}

	if (_log_renders) {
		_renders.push_back (area);
	}

	context->restore ();

#ifdef CANVAS_DEBUG
	if (getenv ("ARDOUR_HARLEQUIN_CANVAS")) {
		/* light up the canvas to show redraws */
		context->set_source_rgba (random()%255 / 255.0,
					  random()%255 / 255.0,
					  random()%255 / 255.0,
					  255);
		context->rectangle (area.x0, area.y0, area.width(), area.height());
		context->fill ();
	}
#endif
	// checkpoint ("render", "<- render");
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
	return d.translate (Duple (-_scroll_offset_x, -_scroll_offset_y));
}	

Rect
Canvas::window_to_canvas (Rect const & r) const
{
	return r.translate (Duple (_scroll_offset_x, _scroll_offset_y));
}

Rect
Canvas::canvas_to_window (Rect const & r) const
{
	return r.translate (Duple (-_scroll_offset_x, -_scroll_offset_y));
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
	// cerr << "CANVAS Invalidate " << area << " TRANSLATE AS " << canvas_area << endl;
	request_redraw (canvas_area);
}

/** Construct a GtkCanvas */
GtkCanvas::GtkCanvas ()
	: _current_item (0)
	, _grabbed_item (0)
{
	/* these are the events we want to know about */
	add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK);
}

/** Handler for button presses on the canvas.
 *  @param ev GDK event.
 */
bool
GtkCanvas::button_handler (GdkEventButton* ev)
{
	DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("canvas button %3 %1 %1\n", ev->x, ev->y, (ev->type == GDK_BUTTON_PRESS ? "press" : "release")));
	/* The Duple that we are passing in here is in canvas coordinates */
	return deliver_event (Duple (ev->x, ev->y), reinterpret_cast<GdkEvent*> (ev));
}

/** Handler for pointer motion events on the canvas.
 *  @param ev GDK event.
 *  @return true if the motion event was handled, otherwise false.
 */
bool
GtkCanvas::motion_notify_handler (GdkEventMotion* ev)
{
	if (_grabbed_item) {
		/* if we have a grabbed item, it gets just the motion event,
		   since no enter/leave events can have happened.
		*/
		DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("%1 %2 (%3) was grabbed, send MOTION event there\n",
								       _grabbed_item, _grabbed_item->whatami(), _grabbed_item->name));
		return _grabbed_item->Event (reinterpret_cast<GdkEvent*> (ev));
	}

	/* This is in canvas coordinates */
	Duple point (ev->x, ev->y);

	/* find the items at the new mouse position */
	vector<Item const *> items;
	_root.add_items_at_point (point, items);

	Item const * new_item = items.empty() ? 0 : items.back ();

	if (_current_item && _current_item != new_item) {
		/* leave event */
		GdkEventCrossing leave_event;
		leave_event.type = GDK_LEAVE_NOTIFY;
		leave_event.x = ev->x;
		leave_event.y = ev->y;
		_current_item->Event (reinterpret_cast<GdkEvent*> (&leave_event));
	}

	if (new_item && _current_item != new_item) {
		/* enter event */
		GdkEventCrossing enter_event;
		enter_event.type = GDK_ENTER_NOTIFY;
		enter_event.x = ev->x;
		enter_event.y = ev->y;
		new_item->Event (reinterpret_cast<GdkEvent*> (&enter_event));
	}

	_current_item = new_item;

	/* Now deliver the motion event.  It may seem a little inefficient
	   to recompute the items under the event, but the enter notify/leave
	   events may have deleted canvas items so it is important to
	   recompute the list in deliver_event.
	*/
	return deliver_event (point, reinterpret_cast<GdkEvent*> (ev));
}

/** Deliver an event to the appropriate item; either the grabbed item, or
 *  one of the items underneath the event.
 *  @param point Position that the event has occurred at, in canvas coordinates.
 *  @param event The event.
 */
bool
GtkCanvas::deliver_event (Duple point, GdkEvent* event)
{
	if (_grabbed_item) {
		/* we have a grabbed item, so everything gets sent there */
		DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("%1 %2 (%3) was grabbed, send event there\n",
								       _grabbed_item, _grabbed_item->whatami(), _grabbed_item->name));
		return _grabbed_item->Event (event);
	}

	/* find the items that exist at the event's position */
	vector<Item const *> items;
	_root.add_items_at_point (point, items);

	DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("%1 possible items to deliver event to\n", items.size()));

	/* run through the items under the event, from top to bottom, until one claims the event */
	vector<Item const *>::const_reverse_iterator i = items.rbegin ();
	while (i != items.rend()) {

		if ((*i)->ignore_events ()) {
			DEBUG_TRACE (
				PBD::DEBUG::CanvasEvents,
				string_compose ("canvas event ignored by %1 %2\n", (*i)->whatami(), (*i)->name.empty() ? "[unknown]" : (*i)->name)
				);
			++i;
			continue;
		}
		
		if ((*i)->Event (event)) {
			/* this item has just handled the event */
			DEBUG_TRACE (
				PBD::DEBUG::CanvasEvents,
				string_compose ("canvas event handled by %1 %2\n", (*i)->whatami(), (*i)->name.empty() ? "[unknown]" : (*i)->name)
				);
			
			return true;
		}
		
		DEBUG_TRACE (
			PBD::DEBUG::CanvasEvents,
			string_compose ("canvas event left unhandled by %1 %2\n", (*i)->whatami(), (*i)->name.empty() ? "[unknown]" : (*i)->name)
			);
		
		++i;
	}

	/* debugging */
	if (PBD::debug_bits & PBD::DEBUG::CanvasEvents) {
		while (i != items.rend()) {
			DEBUG_TRACE (PBD::DEBUG::CanvasEvents, string_compose ("canvas event not seen by %1\n", (*i)->name.empty() ? "[unknown]" : (*i)->name));
			++i;
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
	
	if (_current_item == item) {
		_current_item = 0;
	}

	if (_grabbed_item == item) {
		_grabbed_item = 0;
	}
}

/** Handler for GDK expose events.
 *  @param ev Event.
 *  @return true if the event was handled.
 */
bool
GtkCanvas::on_expose_event (GdkEventExpose* ev)
{

	Cairo::RefPtr<Cairo::Context> c = get_window()->create_cairo_context ();
	
	/* WINDOW  CANVAS
	 * 0,0     _scroll_offset_x, _scroll_offset_y
	 */

	/* render using canvas coordinates */

	Rect canvas_area (ev->area.x, ev->area.y, ev->area.x + ev->area.width, ev->area.y + ev->area.height);
	canvas_area  = canvas_area.translate (Duple (_scroll_offset_x, _scroll_offset_y));

	/* things are going to render to the cairo surface with canvas
	 * coordinates:
	 *
	 *  an item at window/cairo 0,0 will have canvas_coords _scroll_offset_x,_scroll_offset_y
	 *
	 * let them render at their natural coordinates by using cairo_translate()
	 */

	c->translate (-_scroll_offset_x, -_scroll_offset_y);

	render (canvas_area, c);

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
	return button_handler ((GdkEventButton*) &copy);
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

	copy.button.x = where.x;
	copy.button.y = where.y;

	/* Coordinates in the event will be canvas coordinates, correctly adjusted
	   for scroll if this GtkCanvas is in a GtkCanvasViewport.
	*/
	return button_handler ((GdkEventButton*) &copy);
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
	Duple where = window_to_canvas (Duple (ev->x, ev->y));

	copy.motion.x = where.x;
	copy.motion.y = where.y;

	/* Coordinates in the event will be canvas coordinates, correctly adjusted
	   for scroll if this GtkCanvas is in a GtkCanvasViewport.
	*/
	return motion_notify_handler ((GdkEventMotion*) &copy);
}

/** Called to request a redraw of our canvas.
 *  @param area Area to redraw, in canvas coordinates.
 */
void
GtkCanvas::request_redraw (Rect const & request)
{
	Rect area = canvas_to_window (request);
	// cerr << "Invalidate " << request << " TRANSLATE AS " << area << endl;
	queue_draw_area (floor (area.x0), floor (area.y0), ceil (area.x1) - floor (area.x0), ceil (area.y1) - floor (area.y0));
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

