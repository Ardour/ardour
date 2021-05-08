/*
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __gtk2_ardour_cairo_widget_h__
#define __gtk2_ardour_cairo_widget_h__

#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <gtkmm/eventbox.h>

#include "gtkmm2ext/visibility.h"
#include "gtkmm2ext/cairo_canvas.h"
#include "gtkmm2ext/cairo_theme.h"
#include "gtkmm2ext/widget_state.h"

/** A parent class for widgets that are rendered using Cairo.
 */

class LIBGTKMM2EXT_API CairoWidget : public Gtk::EventBox, public Gtkmm2ext::CairoCanvas, public Gtkmm2ext::CairoTheme
{
public:
	CairoWidget ();
	virtual ~CairoWidget ();

	void set_canvas_widget ();
	void use_nsglview ();
	void use_image_surface (bool yn = true);

	/* swizzle Gtk::Widget methods for Canvas::Widget */
	void queue_draw ();
	void queue_resize ();
	int get_width () const;
	int get_height () const;
	void size_allocate (Gtk::Allocation&);

	void set_dirty (cairo_rectangle_t *area = 0);

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
	bool get_active () const { return active_state() != Gtkmm2ext::Off; }

	/* widgets can be told to only draw their "foreground, and thus leave
	   in place whatever background is drawn by their parent. the default
	   is that the widget will fill its event window with the background
	   color of the parent container.
	*/

	void set_draw_background (bool yn);

	sigc::signal<void> StateChanged;
	sigc::signal<bool> QueueDraw;
	sigc::signal<bool> QueueResize;

	static void provide_background_for_cairo_widget (Gtk::Widget& w, const Gdk::Color& bg);

	uint32_t background_color ();


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
	static void set_focus_handler (sigc::slot<void,Gtk::Widget*>);

protected:
	/** Render the widget to the given Cairo context */
	virtual bool on_expose_event (GdkEventExpose *);
	void on_size_allocate (Gtk::Allocation &);
	void on_state_changed (Gtk::StateType);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	void on_realize ();
	bool on_button_press_event (GdkEventButton*);
	Gdk::Color get_parent_bg ();
	void on_map();
	void on_unmap();

	/* this is an additional virtual "on_..." method. Glibmm does not
	   provide a direct signal for name changes, so this acts as a proxy.
	*/

	virtual void on_name_changed () {};

	Gtkmm2ext::ActiveState _active_state;
	Gtkmm2ext::VisualState _visual_state;
	bool                   _need_bg;
	bool                   _grabbed;

	static sigc::slot<void,Gtk::Widget*> focus_handler;

private:
	void on_widget_name_changed ();

	Cairo::RefPtr<Cairo::Surface> image_surface;
	Glib::SignalProxyProperty _name_proxy;
	sigc::connection _parent_style_change;
	Widget * _current_parent;
	bool _canvas_widget;
	void* _nsglview;
	bool _use_image_surface;
	Gdk::Rectangle _allocation;
	Glib::ustring _widget_name;

};

#endif
