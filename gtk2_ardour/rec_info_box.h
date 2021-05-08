/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
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

	virtual void set_session (ARDOUR::Session*);

protected:
	virtual void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*) = 0;
	virtual void dpi_reset () {}

	void on_size_request (Gtk::Requisition*);
	void on_size_allocate (Gtk::Allocation&);
	virtual void update ();

	Glib::RefPtr<Pango::Layout> _layout_label;
	Glib::RefPtr<Pango::Layout> _layout_value;
	int                         _width;
	int                         _height;
};

class DurationInfoBox : public RecInfoBox
{
public:
	virtual void set_session (ARDOUR::Session*);

protected:
	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	void dpi_reset ();
	virtual void update ();

private:
	void rec_state_changed ();
	sigc::connection _rectime_connection;
};

class XrunInfoBox : public RecInfoBox
{
public:
	virtual void set_session (ARDOUR::Session*);
	virtual void update ();

protected:
	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	void dpi_reset ();
};

class RemainInfoBox : public RecInfoBox
{
public:
	virtual void set_session (ARDOUR::Session*);

protected:
	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	void dpi_reset ();
	virtual void update ();

private:
	void count_recenabled_streams (ARDOUR::Route&);
	uint32_t         _rec_enabled_streams;
	sigc::connection _diskspace_connection;
};

#endif
