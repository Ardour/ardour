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

    $Id: fastmeter.h 570 2006-06-07 21:21:21Z sampo $
*/

#ifndef __gtkmm2ext_pixfader_h__
#define __gtkmm2ext_pixfader_h__

#include <cmath>

#include <gtkmm/drawingarea.h>
#include <gtkmm/adjustment.h>
#include <gdkmm/pixbuf.h>

namespace Gtkmm2ext {

class PixFader : public Gtk::DrawingArea {
  public:
	PixFader (Glib::RefPtr<Gdk::Pixbuf> base, 
		  Glib::RefPtr<Gdk::Pixbuf> handle,
		  Gtk::Adjustment& adjustment);
	virtual ~PixFader ();
	
  protected:
	Gtk::Adjustment& adjustment;

	void on_size_request (GtkRequisition*);

	bool on_expose_event (GdkEventExpose*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_motion_notify_event (GdkEventMotion*);
	bool on_scroll_event (GdkEventScroll* ev);

  private:  
	Glib::RefPtr<Gdk::Pixbuf> base_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf> handle_pixbuf;
	gint pixheight;

	GdkRectangle pixrect;

	GdkWindow* grab_window;
	double grab_y;
	double grab_start;
	int last_drawn;
	bool dragging;
	float default_value;
	int unity_y;

	void adjustment_changed ();

	int display_height () {
		return (int) floor (pixheight * (1.0 - (adjustment.get_upper() - adjustment.get_value ()) / ((adjustment.get_upper() - adjustment.get_lower()))));
	}
};


} /* namespace */

 #endif /* __gtkmm2ext_pixfader_h__ */
