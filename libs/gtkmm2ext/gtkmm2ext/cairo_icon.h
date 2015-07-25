/*
    Copyright (C) 2015 Paul Davis

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

#ifndef __gtk2_ardour_cairo_icon_h__
#define __gtk2_ardour_cairo_icon_h__

#include <gtkmm/bin.h>

#include "gtkmm2ext/visibility.h"
#include "gtkmm2ext/ardour_icon.h"

/** A parent class for icons that are rendered using Cairo but need to be
 * widgets for event handling
 */

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API CairoIcon : public Gtk::Bin
{
public:
	CairoIcon (Gtkmm2ext::ArdourIcon::Icon, uint32_t fg = 0x000000ff);
	virtual ~CairoIcon ();

	void render (cairo_t *, cairo_rectangle_t*);
	void set_fg (uint32_t fg);

	bool on_expose_event (GdkEventExpose*);
	
  private:
	Cairo::RefPtr<Cairo::Surface> image_surface;
	ArdourIcon::Icon icon_type;
	uint32_t fg;
};

}

#endif /* __gtk2_ardour_cairo_icon_h__ */
