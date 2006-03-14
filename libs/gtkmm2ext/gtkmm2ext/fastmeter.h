/*
    Copyright (C) 2003 Paul Davis 

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

    $Id$
*/

#ifndef __gtkmm2ext_fastmeter_h__
#define __gtkmm2ext_fastmeter_h__

#include <gtkmm/drawingarea.h>
#include <gdkmm/pixmap.h>

namespace Gtkmm2ext {

class FastMeter : public Gtk::DrawingArea {
  public:
	enum Orientation { 
		Horizontal,
		Vertical
	};
	
	FastMeter (long hold_cnt, unsigned long width, Orientation);
	virtual ~FastMeter ();
	
	void set (float level, float user_level=0.0f);
	void clear ();

	float get_level() { return current_level; }
	float get_user_level() { return current_user_level; }
	float get_peak() { return current_peak; }

	long hold_count() { return hold_cnt; }
	void set_hold_count (long);
	
	static void set_horizontal_xpm (std::string);
	static void set_vertical_xpm (std::string);
	
  protected:
	bool on_expose_event (GdkEventExpose*);
	void on_size_request (GtkRequisition*);
	void on_realize ();

  private:  
	static std::string h_image_path;
	static std::string v_image_path;
	static Glib::RefPtr<Gdk::Pixmap> h_pixmap;
	static Glib::RefPtr<Gdk::Bitmap> h_mask;
	static gint h_pixheight;
	static gint h_pixwidth;

	static Glib::RefPtr<Gdk::Pixmap> v_pixmap;
	static Glib::RefPtr<Gdk::Bitmap> v_mask;
	static gint v_pixheight;
	static gint v_pixwidth;

	Orientation orientation;
	Glib::RefPtr<Gdk::Pixmap> backing;
	GdkRectangle pixrect;
	gint request_width;
	gint request_height;
	unsigned long hold_cnt;
	unsigned long hold_state;
	float current_level;
	float current_peak;
	float current_user_level;
	
	bool vertical_expose (GdkEventExpose*);
	bool horizontal_expose (GdkEventExpose*);
};


} /* namespace */

 #endif /* __gtkmm2ext_fastmeter_h__ */
