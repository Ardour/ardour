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

#include <gdkmm/window.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/viewport.h>
#include <cairomm/surface.h>
#include <cairomm/context.h>
#include "pbd/signals.h"
#include "canvas/root_group.h"

class XMLTree;

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
	Canvas (XMLTree const *);
	virtual ~Canvas () {}

	/** called to request a redraw of an area of the canvas */
	virtual void request_redraw (Rect const &) = 0;
	/** called to ask the canvas to request a particular size from its host */
	virtual void request_size (Duple) = 0;
	/** called to ask the canvas' host to `grab' an item */
	virtual void grab (Item *) = 0;
	/** called to ask the canvas' host to `ungrab' any grabbed item */
	virtual void ungrab () = 0;

	void render (Rect const &, Cairo::RefPtr<Cairo::Context> const &) const;

	/** @return root group */
	Group* root () {
		return &_root;
	}

	/** Called when an item is being destroyed */
	virtual void item_going_away (Item *, boost::optional<Rect>) {}
	void item_shown_or_hidden (Item *);
	void item_changed (Item *, boost::optional<Rect>);
	void item_moved (Item *, boost::optional<Rect>);

	XMLTree* get_state () const;

	virtual Cairo::RefPtr<Cairo::Context> context () = 0;

	std::list<Rect> const & renders () const {
		return _renders;
	}

	void set_log_renders (bool log) {
		_log_renders = log;
	}

        std::string indent() const;
        std::string render_indent() const;
        void dump (std::ostream&) const;
    
protected:
	void queue_draw_item_area (Item *, Rect);
	
	/** our root group */
	RootGroup _root;

	mutable std::list<Rect> _renders;
	bool _log_renders;
};

/** A Canvas which renders onto an in-memory pixbuf.  In Ardour's context,
 *  this is most useful for testing.
 */
class ImageCanvas : public Canvas
{
public:
	ImageCanvas (Duple size = Duple (1024, 1024));
	ImageCanvas (XMLTree const *, Duple size = Duple (1024, 1024));

	void request_redraw (Rect const &) {
		/* XXX */
	}

	void request_size (Duple) {
		/* XXX */
	}
	
	void grab (Item *) {}
	void ungrab () {}

	void render_to_image (Rect const &) const;
	void clear ();
	void write_to_png (std::string const &);

	Cairo::RefPtr<Cairo::Context> context ();

private:
	/** our Cairo surface */
	Cairo::RefPtr<Cairo::Surface> _surface;
	/** our Cairo context */
	Cairo::RefPtr<Cairo::Context> _context;
};

/** A canvas which renders onto a GTK EventBox */
class GtkCanvas : public Canvas, public Gtk::EventBox
{
public:
	GtkCanvas ();
	GtkCanvas (XMLTree const *);

	void request_redraw (Rect const &);
	void request_size (Duple);
	void grab (Item *);
	void ungrab ();

	Cairo::RefPtr<Cairo::Context> context ();

protected:
	bool on_expose_event (GdkEventExpose *);
	bool on_button_press_event (GdkEventButton *);
	bool on_button_release_event (GdkEventButton* event);
	bool on_motion_notify_event (GdkEventMotion *);
	
	bool button_handler (GdkEventButton *);
	bool motion_notify_handler (GdkEventMotion *);
	bool deliver_event (Duple, GdkEvent *);

private:
	void item_going_away (Item *, boost::optional<Rect>);
	bool send_leave_event (Item const *, double, double) const;

	/** the item that the mouse is currently over, or 0 */
	Item const * _current_item;
	/** the item that is currently grabbed, or 0 */
	Item const * _grabbed_item;
};

/** A GTK::Viewport with a GtkCanvas inside it.  This provides a GtkCanvas
 *  that can be scrolled.
 */
class GtkCanvasViewport : public Gtk::Viewport
{
public:
	GtkCanvasViewport (Gtk::Adjustment &, Gtk::Adjustment &);

	/** @return our GtkCanvas */
	GtkCanvas* canvas () {
		return &_canvas;
	}

	void window_to_canvas (int, int, Coord &, Coord &) const;
	Rect visible_area () const;

protected:
	void on_size_request (Gtk::Requisition *);

private:
	/** our GtkCanvas */
	GtkCanvas _canvas;
};

}

std::ostream& operator<< (std::ostream&, const ArdourCanvas::Canvas&);

#endif
