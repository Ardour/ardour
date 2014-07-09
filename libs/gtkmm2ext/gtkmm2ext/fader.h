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

#include <gtkmm/drawingarea.h>
#include <gtkmm/adjustment.h>
#include <gdkmm.h>
#include <gtkmm2ext/binding_proxy.h>

#include <boost/shared_ptr.hpp>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API Fader : public Gtk::DrawingArea
{
  public:
        Fader (Gtk::Adjustment& adjustment, 
			   const std::string& face_image_file,
			   const std::string& active_face_image_file,
			   const std::string& underlay_image_file,
			   const std::string& handle_image_file,
			   const std::string& active_handle_image_file,
			   int min_pos_x, 
			   int min_pos_y,
			   int max_pos_x,
			   int max_pos_y);

	virtual ~Fader ();

	void set_controllable (boost::shared_ptr<PBD::Controllable> c) { binding_proxy.set_controllable (c); }

	void set_default_value (float);

  protected:
	void get_handle_position (double& x, double& y);

	void on_size_request (GtkRequisition*);
	void on_size_allocate (Gtk::Allocation& alloc);

	bool on_expose_event (GdkEventExpose*);
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

    Glib::RefPtr<Gdk::Pixbuf> _handle_pixbuf;
    Glib::RefPtr<Gdk::Pixbuf> _active_handle_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf> _underlay_pixbuf;
    Glib::RefPtr<Gdk::Pixbuf> _face_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf> _active_face_pixbuf;
	int _min_pos_x;
	int _min_pos_y;
	int _max_pos_x;
	int _max_pos_y;

	bool _hovering;

	GdkWindow* _grab_window;
	double _grab_loc_x;
	double _grab_loc_y;
	double _grab_start_x;
	double _grab_start_y;
	double _last_drawn_x;
	double _last_drawn_y;
	bool _dragging;
	float _default_value;
	int _unity_loc;

	void adjustment_changed ();
	void set_adjustment_from_event (GdkEventButton *);
	void update_unity_position ();
};


} /* namespace */

 #endif /* __gtkmm2ext_fader_h__ */
