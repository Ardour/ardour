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

#include "cairo_widget.h"

class Editor;

class EditorGroupTabs : public CairoWidget
{
public:
	EditorGroupTabs (Editor *);

	void set_session (ARDOUR::Session *);

private:
	void on_size_request (Gtk::Requisition *);
	bool on_button_press_event (GdkEventButton *);
	void render (cairo_t *);
	void draw_group (cairo_t *, int32_t, int32_t, ARDOUR::RouteGroup* , Gdk::Color const &);
	
	Editor* _editor;
};
