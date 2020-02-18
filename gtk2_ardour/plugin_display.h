/*
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
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

#ifndef __ardour_plugin_display__
#define __ardour_plugin_display__

#include <gtkmm/drawingarea.h>

#include "ardour/plugin.h"

class PluginDisplay : public Gtk::DrawingArea
{
public:
	PluginDisplay(boost::shared_ptr<ARDOUR::Plugin>, uint32_t max_height = 80);
	virtual ~PluginDisplay();

protected:
	bool on_expose_event (GdkEventExpose *);
	void on_size_request (Gtk::Requisition* req);
	bool on_button_press_event (GdkEventButton *ev);
	bool on_button_release_event (GdkEventButton *ev);

	void plugin_going_away () {
		_qdraw_connection.disconnect ();
	}

	virtual void update_height_alloc (uint32_t inline_height);
	virtual uint32_t render_inline (cairo_t *, uint32_t width);

	virtual void display_frame (cairo_t* cr, double w, double h);

	boost::shared_ptr<ARDOUR::Plugin> _plug;
	PBD::ScopedConnection _qdraw_connection;
	PBD::ScopedConnection _death_connection;
	cairo_surface_t* _surf;
	uint32_t _max_height;
	uint32_t _cur_height;
	bool _scroll;
};

#endif
