/*
    Copyright (C) 2009 Paul Davis 

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

#ifndef __gtk2_ardour_cairo_widget_h__
#define __gtk2_ardour_cairo_widget_h__

#include <gtkmm/eventbox.h>

class CairoWidget : public Gtk::EventBox
{
public:
	CairoWidget ();
	virtual ~CairoWidget ();

	void set_dirty ();

protected:
	virtual void render (cairo_t *) = 0;
	virtual bool on_expose_event (GdkEventExpose *);
	void on_size_allocate (Gtk::Allocation &);

	int _width; ///< pixmap width
	int _height; ///< pixmap height
	
private:
	bool _dirty;
	GdkPixmap* _pixmap;
};

#endif
