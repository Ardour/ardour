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

#include "canvas/visibility.h"

#include "canvas/root_group.h"

namespace Gtk {
	class Window;
	class Label;
}

namespace Pango {
	class Context;
}

namespace ArdourCanvas
{
struct Rect;

class Item;
class ScrollGroup;

/** The base class for our different types of canvas.
 *
 *  A canvas is an area which holds a collection of canvas items, which in
 *  turn represent shapes, text, etc.
 *
 *  The canvas has an arbitrarily large area, and is addressed in coordinates
 *  of screen pixels, with an origin of (0, 0) at the top left.  x increases
 *  rightwards and y increases downwards.
 */

class LIBCANVAS_API Canvas
{
public:
	Canvas ();
	virtual ~Canvas () {}

	/** called to request a redraw of an area of the canvas in WINDOW coordinates */
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
	Item* root () {
		return &_root;
	}

        void set_background_color (ArdourCanvas::Color);
        ArdourCanvas::Color background_color() const { return _bg_color; }

	/** Called when an item is being destroyed */
	virtual void item_going_away (Item *, Rect) {}
	void item_shown_or_hidden (Item *);
        void item_visual_property_changed (Item*);
	void item_changed (Item *, Rect);
	void item_moved (Item *, Rect);

        Duple canvas_to_window (Duple const&, bool rounded = true) const;
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
	void add_scroller (ScrollGroup& i);

        virtual Rect  visible_area () const = 0;
        virtual Coord width () const = 0;
        virtual Coord height () const = 0;

	/** Store the coordinates of the mouse pointer in window coordinates in
	   @param winpos. Return true if the position was within the window,
	   false otherwise.
	*/
	virtual bool get_mouse_position (Duple& winpos) const = 0;

	/** Signal to be used by items that need to track the mouse position
	   within the window.
	*/
	sigc::signal<void,Duple const&> MouseMotion;

	/** Ensures that the position given by @param winpos (in window
	    coordinates) is within the current window area, possibly reduced by
	    @param border.
	*/
	Duple clamp_to_window (Duple const& winpos, Duple border = Duple());

        void zoomed();

        std::string indent() const;
        std::string render_indent() const;
        void dump (std::ostream&) const;

	/** Ask the canvas to pick the current item again, and generate
	    an enter event for it.
	*/
	virtual void re_enter () = 0;

	virtual void start_tooltip_timeout (Item*) {}
	virtual void stop_tooltip_timeout () {}

	/** Set the timeout used to display tooltips, in milliseconds
	 */
	static void set_tooltip_timeout (uint32_t msecs);

	virtual Glib::RefPtr<Pango::Context> get_pango_context() = 0;

  protected:
	Root  _root;
        Color _bg_color;

	static uint32_t tooltip_timeout_msecs;

	void queue_draw_item_area (Item *, Rect);
        virtual void pick_current_item (int state) = 0;
        virtual void pick_current_item (Duple const &, int state) = 0;

	std::list<ScrollGroup*> scrollers;
};

/** A canvas which renders onto a GTK EventBox */
class LIBCANVAS_API GtkCanvas : public Canvas, public Gtk::EventBox
{
public:
	GtkCanvas ();
	~GtkCanvas () { _in_dtor = true ; }

	void request_redraw (Rect const &);
	void request_size (Duple);
	void grab (Item *);
	void ungrab ();
	void focus (Item *);
	void unfocus (Item*);

	Rect visible_area () const;
	Coord width() const;
	Coord height() const;

	bool get_mouse_position (Duple& winpos) const;

	void set_single_exposure (bool s) { _single_exposure = s; }
	bool single_exposure () { return _single_exposure; }

	void re_enter ();

	void start_tooltip_timeout (Item*);
	void stop_tooltip_timeout ();

	Glib::RefPtr<Pango::Context> get_pango_context();

  protected:
	void on_size_allocate (Gtk::Allocation&);
	bool on_scroll_event (GdkEventScroll *);
	bool on_expose_event (GdkEventExpose *);
	bool on_key_press_event (GdkEventKey *);
	bool on_key_release_event (GdkEventKey *);
	bool on_button_press_event (GdkEventButton *);
	bool on_button_release_event (GdkEventButton* event);
	bool on_motion_notify_event (GdkEventMotion *);
        bool on_enter_notify_event (GdkEventCrossing*);
        bool on_leave_notify_event (GdkEventCrossing*);

	bool button_handler (GdkEventButton *);
	bool motion_notify_handler (GdkEventMotion *);
        bool deliver_event (GdkEvent *);
        void deliver_enter_leave (Duple const & point, int state);

        void pick_current_item (int state);
        void pick_current_item (Duple const &, int state);

private:
	void item_going_away (Item *, Rect);
	bool send_leave_event (Item const *, double, double) const;

	Cairo::RefPtr<Cairo::Surface> canvas_image;

        /** Item currently chosen for event delivery based on pointer position */
        Item * _current_item;
        /** Item pending as _current_item */
        Item * _new_current_item;
	/** the item that is currently grabbed, or 0 */
	Item * _grabbed_item;
        /** the item that currently has key focus or 0 */
	Item * _focused_item;

	bool _single_exposure;

	sigc::connection tooltip_timeout_connection;
	Item* current_tooltip_item;
	Gtk::Window* tooltip_window;
	Gtk::Label* tooltip_label;
	bool show_tooltip ();
	void hide_tooltip ();
	bool really_start_tooltip_timeout ();

	bool _in_dtor;
};

/** A GTK::Alignment with a GtkCanvas inside it plus some Gtk::Adjustments for
 *   scrolling.
 *
 * This provides a GtkCanvas that can be scrolled. It does NOT implement the
 * Gtk::Scrollable interface.
 */
class LIBCANVAS_API GtkCanvasViewport : public Gtk::Alignment
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
