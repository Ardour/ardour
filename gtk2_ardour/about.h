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

#ifndef __ardour_gtk_about_h__
#define __ardour_gtk_about_h__

#include <gtkmm/window.h>
#include <gtkmm/pixmap.h>
#include <libgnomecanvas/libgnomecanvas.h>

class ARDOUR_UI;

class About : public Gtk::Window
{
  public:
	About (ARDOUR_UI *);
	~About ();

	void show_sub (bool yn);

  protected:
	void realize_impl ();
	
  private:
	Gtk::DrawingArea logo_area;
	GdkPixmap*       logo_pixmap;
	Gtk::Label       first_label;
	Gtk::Label       second_label;
	Gtk::Label       third_label;
	Gtk::VBox        vbox;
	Gtk::VBox        subvbox;

	vector<string>   authors;
	vector<string>   supporters;

	uint32_t  about_index;
	uint32_t  about_cnt;
	int  logo_height;
	int  logo_width;
	bool drawn;
	bool support;
	ARDOUR_UI * _ui;
	
	sigc::connection timeout_connection;
	
	bool load_logo_size ();
	bool load_logo (Gtk::Window&);
	gint logo_area_expose (GdkEventExpose*);

	gint button_release_event_impl (GdkEventButton*);
	gint start_animating ();
	void stop_animating ();

	void gone_hidden ();
	
#ifdef WITH_PAYMENT_OPTIONS
	Gtk::Image      paypal_pixmap;
	Gtk::Button      paypal_button;
	void goto_paypal ();
#endif
};	

#endif /* __ardour_gtk_about_h__ */
