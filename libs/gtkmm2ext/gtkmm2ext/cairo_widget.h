/*
    Copyright (C) 2009 Paul Davis

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

#ifndef __gtk2_ardour_cairo_widget_h__
#define __gtk2_ardour_cairo_widget_h__

#include <gtkmm/eventbox.h>

#include "gtkmm2ext/visibility.h"
#include "gtkmm2ext/widget_state.h"

/** A parent class for widgets that are rendered using Cairo.
 */

class LIBGTKMM2EXT_API CairoWidget : public Gtk::EventBox
{
public:
	CairoWidget ();
	virtual ~CairoWidget ();

	void set_dirty ();

	Gtkmm2ext::ActiveState active_state() const { return _active_state; }
	Gtkmm2ext::VisualState visual_state() const { return _visual_state; }
	
	/* derived widgets can override these two to catch 
	   changes in active & visual state
	*/
	
	virtual void set_active_state (Gtkmm2ext::ActiveState);
	virtual void set_visual_state (Gtkmm2ext::VisualState);

	void unset_active_state () { set_active_state (Gtkmm2ext::Off); }
	void unset_visual_state () { set_visual_state (Gtkmm2ext::NoVisualState); }

	/* this is an API simplification for widgets
	   that only use the Active and Normal active states.
	*/
	void set_active (bool);
	bool get_active () { return active_state() != Gtkmm2ext::Off; }

	/* widgets can be told to only draw their "foreground, and thus leave
	   in place whatever background is drawn by their parent. the default
	   is that the widget will fill its event window with the background
	   color of the parent container.
	*/

	void set_draw_background (bool yn);

	sigc::signal<void> StateChanged;

	static void provide_background_for_cairo_widget (Gtk::Widget& w, const Gdk::Color& bg);

protected:
	/** Render the widget to the given Cairo context */
	virtual void render (cairo_t *) = 0;
	virtual bool on_expose_event (GdkEventExpose *);
	void on_size_allocate (Gtk::Allocation &);
	void on_state_changed (Gtk::StateType);
	Gdk::Color get_parent_bg ();

	Gtkmm2ext::ActiveState _active_state;
	Gtkmm2ext::VisualState _visual_state;
	bool                   _need_bg;
};

#endif
