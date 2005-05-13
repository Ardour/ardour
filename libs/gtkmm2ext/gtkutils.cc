/*
    Copyright (C) 1998-99 Paul Barton-Davis
 
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

#include <string>

#include <gtkmm.h>
#include <gtkmm2ext/gtkutils.h>

void
set_usize_to_display_given_text (Gtk::Widget &w,
				 const std::string& text,
				 gint hpadding,
				 gint vpadding)
{
	int height = 0;
        int width = 0;

        w.create_pango_layout(text)->get_pixel_size(width, height);

	height += vpadding;
	width += hpadding;

	w.set_size_request(width, height);
}
