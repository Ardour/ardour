/*
    Copyright (C) 2006 Paul Davis 

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

#ifndef __gtkmm2ext_fader_h__
#define __gtkmm2ext_fader_h__

#include <cmath>
#include <stdint.h>

#include <gtkmm/adjustment.h>
#include <gdkmm.h>
#include <gtkmm2ext/binding_proxy.h>
#include "gtkmm2ext/cairo_widget.h"

#include <boost/shared_ptr.hpp>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API Fader : public CairoWidget
{
  public:
        Fader (Gtk::Adjustment& adjustment, 
			   const Glib::RefPtr<Gdk::Pixbuf>& face_pixbuf,
			   const Glib::RefPtr<Gdk::Pixbuf>& active_face_pixbuf,
			   const Glib::RefPtr<Gdk::Pixbuf>& underlay_pixbuf,
			   const Glib::RefPtr<Gdk::Pixbuf>& handle_pixbuf,
			   const Glib::RefPtr<Gdk::Pixbuf>& active_handle_pixbuf,
			   int min_pos_x, 
			   int min_pos_y,
			   int max_pos_x,
			   int max_pos_y,
			   bool read_only);

	virtual ~Fader ();

	void set_controllable (boost::shared_ptr<PBD::Controllable> c) { binding_proxy.set_controllable (c); }
	void set_default_value (float);
	void set_touch_cursor (const Glib::RefPtr<Gdk::Pixbuf>& touch_cursor);

  protected:
	void get_handle_position (double& x, double& y);

	void on_size_request (GtkRequisition*);
	void on_size_allocate (Gtk::Allocation& alloc);

	void render (cairo_t* cr, cairo_rectangle_t*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_motion_notify_event (GdkEventMotion*);
	bool on_scroll_event (GdkEventScroll* ev);
	bool on_enter_notify_event (GdkEventCrossing* ev);
	bool on_leave_notify_event (GdkEventCrossing* ev);

  protected:
	Gtk::Adjustment& adjustment;
	BindingProxy binding_proxy;

  private:

    const Glib::RefPtr<Gdk::Pixbuf> _face_pixbuf;
	const Glib::RefPtr<Gdk::Pixbuf> _active_face_pixbuf;
	const Glib::RefPtr<Gdk::Pixbuf> _underlay_pixbuf;
    const Glib::RefPtr<Gdk::Pixbuf> _handle_pixbuf;
    const Glib::RefPtr<Gdk::Pixbuf> _active_handle_pixbuf;
	int _min_pos_x;
	int _min_pos_y;
	int _max_pos_x;
	int _max_pos_y;

	bool _hovering;

	GdkWindow* _grab_window;
	Gdk::Cursor *_touch_cursor;

	double _grab_start_mouse_x;
	double _grab_start_mouse_y;
	double _grab_start_handle_x;
	double _grab_start_handle_y;
	double _last_drawn_x;
	double _last_drawn_y;
	bool _dragging;
	float _default_value;
	int _unity_loc;
	bool _read_only;

	void adjustment_changed ();
	void update_unity_position ();
};


} /* namespace */

 #endif /* __gtkmm2ext_fader_h__ */
