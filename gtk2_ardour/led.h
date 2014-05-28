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

#ifndef __gtk2_ardour_led_h__
#define __gtk2_ardour_led_h__

#include <stdint.h>

#include "gtkmm2ext/cairo_widget.h"

class LED : public CairoWidget
{
  public:
	LED ();
	virtual ~LED ();

	void set_diameter (float);

  protected:
	void render (cairo_t *);
	void on_size_request (Gtk::Requisition* req);
	void on_realize ();

  private:
	float _diameter;
	float _red;
	float _green;
	float _blue;
	bool  _fixed_diameter;

	void set_colors_from_style ();
};

#endif /* __gtk2_ardour_led_h__ */
