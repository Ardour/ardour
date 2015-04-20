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

#include <cairomm/surface.h>
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

	virtual void render (cairo_t *, cairo_rectangle_t*) = 0;

	static void set_flat_buttons (bool yn);
	static bool flat_buttons() { return _flat_buttons; }

	static void set_widget_prelight (bool yn);
	static bool widget_prelight() { return _widget_prelight; }

	static void set_source_rgb_a( cairo_t* cr, Gdk::Color, float a=1.0 );

	/* set_focus_handler() will cause all button-press events on any
	   CairoWidget to invoke this slot/functor/function/method/callback.
	   
	   We do this because in general, CairoWidgets do not grab
	   keyboard focus, but a button press on them should
	   clear focus from any active text entry.

	   This is global to all CairoWidgets and derived types.

	   However, derived types can override the behaviour by defining their
	   own on_button_press_event() handler which returns true under all
	   conditions (which will block this handler from being called). If 
	   they wish to invoke any existing focus handler from their own
	   button press handler, they can just use: focus_handler();
	*/
	static void set_focus_handler (sigc::slot<void>);

protected:
	/** Render the widget to the given Cairo context */
	virtual bool on_expose_event (GdkEventExpose *);
	void on_size_allocate (Gtk::Allocation &);
	void on_state_changed (Gtk::StateType);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	bool on_button_press_event (GdkEventButton*);
	Gdk::Color get_parent_bg ();
	
	/* this is an additional virtual "on_..." method. Glibmm does not
	   provide a direct signal for name changes, so this acts as a proxy.
	*/

	virtual void on_name_changed () {};

	Gtkmm2ext::ActiveState _active_state;
	Gtkmm2ext::VisualState _visual_state;
	bool                   _need_bg;

	static bool	_flat_buttons;
	static bool	_widget_prelight;
	bool		_grabbed;

	static sigc::slot<void> focus_handler;

  private:
	Cairo::RefPtr<Cairo::Surface> image_surface;
	Glib::SignalProxyProperty _name_proxy;
	sigc::connection _parent_style_change;
	Widget * _current_parent;
	
};

#endif
