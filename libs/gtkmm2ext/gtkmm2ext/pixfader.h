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

#ifndef __gtkmm2ext_pixfader_h__
#define __gtkmm2ext_pixfader_h__

#include <cmath>
#include <stdint.h>

#include <gtkmm/drawingarea.h>
#include <gtkmm/adjustment.h>
#include <gdkmm.h>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API PixFader : public Gtk::DrawingArea
{
  public:
        PixFader (Gtk::Adjustment& adjustment, int orientation, int span, int girth);
	virtual ~PixFader ();

	void set_default_value (float);
	void set_text (const std::string&);

  protected:
	Glib::RefPtr<Pango::Layout> _layout;
	std::string                 _text;
	int   _text_width;
	int   _text_height;

	Gtk::Adjustment& adjustment;

	void on_size_request (GtkRequisition*);
	void on_size_allocate (Gtk::Allocation& alloc);

	bool on_expose_event (GdkEventExpose*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_motion_notify_event (GdkEventMotion*);
	bool on_scroll_event (GdkEventScroll* ev);
	bool on_enter_notify_event (GdkEventCrossing* ev);
	bool on_leave_notify_event (GdkEventCrossing* ev);

	void on_state_changed (Gtk::StateType);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	Gdk::Color get_parent_bg ();

	enum Orientation {
		VERT,
		HORIZ,
	};

  private:
	int span, girth;
	int _orien;
	
	cairo_pattern_t* fg_gradient;
	cairo_pattern_t* bg_gradient;

	bool _hovering;

	GdkWindow* grab_window;
	double grab_loc;
	double grab_start;
	int last_drawn;
	bool dragging;
	float default_value;
	int unity_loc;

	void adjustment_changed ();
	float display_span ();
	void set_adjustment_from_event (GdkEventButton *);
	void update_unity_position ();

	sigc::connection _parent_style_change;
	Widget * _current_parent;
};


} /* namespace */

 #endif /* __gtkmm2ext_pixfader_h__ */
