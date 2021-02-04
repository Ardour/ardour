/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _rec_info_box_h_
#define _rec_info_box_h_

#include "ardour/session_handle.h"

#include "gtkmm2ext/cairo_widget.h"

namespace ARDOUR {
	class Route;
}

class RecInfoBox : public CairoWidget, public ARDOUR::SessionHandlePtr
{
public:
	RecInfoBox ();
	~RecInfoBox ();

	void set_session (ARDOUR::Session*);

protected:
	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	void on_size_request (Gtk::Requisition*);
	void on_size_allocate (Gtk::Allocation&);

private:
	void dpi_reset ();
	void update ();
	void rec_state_changed ();
	void count_recenabled_streams (ARDOUR::Route&);

	Glib::RefPtr<Pango::Layout> _layout_label;
	Glib::RefPtr<Pango::Layout> _layout_value;

	int                         _width;
	int                         _height;
	uint32_t                    _rec_enabled_streams; 
	sigc::connection            _rectime_connection;
};

#endif
