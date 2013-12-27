/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __gtkmm2ext_pixscroller_h__ 
#define __gtkmm2ext_pixscroller_h__

#include <gtkmm/drawingarea.h>
#include <gtkmm/adjustment.h>
#include <gdkmm.h>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API PixScroller : public Gtk::DrawingArea
{
  public:
	PixScroller(Gtk::Adjustment& adjustment, 
		    Glib::RefPtr<Gdk::Pixbuf> slider,
		    Glib::RefPtr<Gdk::Pixbuf> rail);

	bool on_expose_event (GdkEventExpose*);
	bool on_motion_notify_event (GdkEventMotion*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_scroll_event (GdkEventScroll*);
	void on_size_request (GtkRequisition*);

  protected:
	Gtk::Adjustment& adj;

  private:

        Cairo::RefPtr< Cairo::Context > rail_context;
        Cairo::RefPtr< Cairo::ImageSurface > rail_surface;
	Glib::RefPtr<Gdk::Pixbuf> rail;
        Cairo::RefPtr< Cairo::Context > slider_context;
        Cairo::RefPtr< Cairo::ImageSurface > slider_surface;
	Glib::RefPtr<Gdk::Pixbuf> slider;
	Gdk::Rectangle sliderrect;
	Gdk::Rectangle railrect;
	GdkWindow* grab_window;
	double grab_y;
	double grab_start;
	int overall_height;
	bool dragging;
	
	float default_value;

	void adjustment_changed ();
};

} // namespace

#endif /* __gtkmm2ext_pixscroller_h__ */
