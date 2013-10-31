/*
    Copyright (C) 2011-2013 Paul Davis
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

/** @file  canvas/canvas.h
 *  @brief Declaration of the main canvas classes.
 */

#ifndef __CANVAS_CANVAS_H__
#define __CANVAS_CANVAS_H__

#include <set>

#include <gdkmm/window.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/alignment.h>
#include <cairomm/surface.h>
#include <cairomm/context.h>

#include "pbd/signals.h"
#include "canvas/root_group.h"

namespace ArdourCanvas
{

class Rect;
class Group;	

/** The base class for our different types of canvas.
 *
 *  A canvas is an area which holds a collection of canvas items, which in
 *  turn represent shapes, text, etc.
 *
 *  The canvas has an arbitrarily large area, and is addressed in coordinates
 *  of screen pixels, with an origin of (0, 0) at the top left.  x increases
 *  rightwards and y increases downwards.
 */
	
class Canvas
{
public:
	Canvas ();
	virtual ~Canvas () {}

	/** called to request a redraw of an area of the canvas */
	virtual void request_redraw (Rect const &) = 0;
	/** called to ask the canvas to request a particular size from its host */
	virtual void request_size (Duple) = 0;
	/** called to ask the canvas' host to `grab' an item */
	virtual void grab (Item *) = 0;
	/** called to ask the canvas' host to `ungrab' any grabbed item */
	virtual void ungrab () = 0;

	/** called to ask the canvas' host to keyboard focus on an item */
	virtual void focus (Item *) = 0;
	/** called to ask the canvas' host to drop keyboard focus on an item */
	virtual void unfocus (Item*) = 0;

	void render (Rect const &, Cairo::RefPtr<Cairo::Context> const &) const;

	/** @return root group */
	Group* root () {
		return &_root;
	}

	/** Called when an item is being destroyed */
	virtual void item_going_away (Item *, boost::optional<Rect>) {}
	void item_shown_or_hidden (Item *);
        void item_visual_property_changed (Item*);
	void item_changed (Item *, boost::optional<Rect>);
	void item_moved (Item *, boost::optional<Rect>);

        virtual Cairo::RefPtr<Cairo::Context> context () = 0;

        Rect canvas_to_window (Rect const&) const;
        Rect window_to_canvas (Rect const&) const;
        Duple canvas_to_window (Duple const&) const;
        Duple window_to_canvas (Duple const&) const;

        void canvas_to_window (Coord cx, Coord cy, Coord& wx, Coord& wy) {
		Duple d = canvas_to_window (Duple (cx, cy));
		wx = d.x;
		wy = d.y;
        }

        void window_to_canvas (Coord wx, Coord wy, Coord& cx, Coord& cy) {
		Duple d = window_to_canvas (Duple (wx, wy));
		cx = d.x;
		cy = d.y;
        }

        void scroll_to (Coord x, Coord y);
        virtual Rect visible_area () const = 0;

        void zoomed();
    
        std::string indent() const;
        std::string render_indent() const;
        void dump (std::ostream&) const;
    
protected:
	void queue_draw_item_area (Item *, Rect);
	
	/** our root group */
	RootGroup _root;

        Coord _scroll_offset_x;
        Coord _scroll_offset_y;

        virtual void enter_leave_items (int state) = 0;
        virtual void enter_leave_items (Duple const &, int state) = 0;
};

/** A canvas which renders onto a GTK EventBox */
class GtkCanvas : public Canvas, public Gtk::EventBox
{
public:
	GtkCanvas ();

	void request_redraw (Rect const &);
	void request_size (Duple);
	void grab (Item *);
	void ungrab ();
	void focus (Item *);
	void unfocus (Item*);

	Cairo::RefPtr<Cairo::Context> context ();

	Rect visible_area () const;

protected:
	bool on_expose_event (GdkEventExpose *);
	bool on_button_press_event (GdkEventButton *);
	bool on_button_release_event (GdkEventButton* event);
	bool on_motion_notify_event (GdkEventMotion *);
        bool on_enter_notify_event (GdkEventCrossing*);
        bool on_leave_notify_event (GdkEventCrossing*);
	
	bool button_handler (GdkEventButton *);
	bool motion_notify_handler (GdkEventMotion *);
	bool deliver_event (Duple, GdkEvent *);

        void enter_leave_items (int state);
        void enter_leave_items (Duple const &, int state);

private:
	void item_going_away (Item *, boost::optional<Rect>);
	bool send_leave_event (Item const *, double, double) const;

        /** Items that the pointer is currently within */
        std::set<Item const *> within_items;
	/** the item that is currently grabbed, or 0 */
	Item const * _grabbed_item;
        /** the item that currently has key focus or 0 */
	Item const * _focused_item;
};

/** A GTK::Alignment with a GtkCanvas inside it plus some Gtk::Adjustments for
 *   scrolling. 
 *
 * This provides a GtkCanvas that can be scrolled. It does NOT implement the
 * Gtk::Scrollable interface.
 */
class GtkCanvasViewport : public Gtk::Alignment
{
public:
	GtkCanvasViewport (Gtk::Adjustment &, Gtk::Adjustment &);

	/** @return our GtkCanvas */
	GtkCanvas* canvas () {
		return &_canvas;
	}

protected:
	void on_size_request (Gtk::Requisition *);

private:
	/** our GtkCanvas */
	GtkCanvas _canvas;
        Gtk::Adjustment& hadjustment;
        Gtk::Adjustment& vadjustment;

        void scrolled ();
};

}

std::ostream& operator<< (std::ostream&, const ArdourCanvas::Canvas&);

#endif
