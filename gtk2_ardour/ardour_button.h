/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __gtk2_ardour_ardour_button_h__
#define __gtk2_ardour_ardour_button_h__

#include <stdint.h>

#include <gtkmm/activatable.h>

#include "cairo_widget.h"

class ArdourButton : public CairoWidget, Gtk::Activatable
{
  public:
	ArdourButton ();
	virtual ~ArdourButton ();

        void set_diameter (float);

	void set_text (const std::string&);
	void set_markup (const std::string&);

	void set_led_left (bool yn);
	void set_distinct_led_click (bool yn);

	sigc::signal<void> signal_clicked;

  protected:
	void render (cairo_t *);
        void on_size_request (Gtk::Requisition* req);
        void on_realize ();
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);

  private:
	Glib::RefPtr<Pango::Layout> _layout;
	std::string _text;
	int     _text_width;
	int     _text_height;
	bool    _led_left;
        float   _diameter;
        bool    _fixed_diameter;
	bool    _distinct_led_click;

	cairo_pattern_t* edge_pattern;
	cairo_pattern_t* fill_pattern;
	cairo_pattern_t* led_inset_pattern;
	cairo_pattern_t* reflection_pattern;

	double text_r;
	double text_g;
	double text_b;
	double text_a;

	double led_r;
	double led_g;
	double led_b;
	double led_a;

        void set_colors ();
	void color_handler ();
	void state_handler ();
};

#endif /* __gtk2_ardour_ardour_button_h__ */
