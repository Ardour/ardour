/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017-2019 Ben Loftis <ben@harrisonconsoles.com>
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

#pragma once

#include "ardour/session_handle.h"
#include "ardour/types.h"
#include "gtkmm2ext/cairo_widget.h"
#include "pbd/property_basics.h"

#include "editor_component.h"

namespace ARDOUR {
	class Session;
}

class Editor;
class RegionView;
class RouteTimeAxisView;

/** Class to provide a visual summary of the contents of an editor window; represents
 *  the whole session as a set of lines, one per region view.
 */
class EditorVSummary : public CairoWidget, public EditorComponent, public ARDOUR::SessionHandlePtr, public PBD::ScopedConnectionList
{
public:
	EditorVSummary (Editor&);
	~EditorVSummary ();

	void set_session (ARDOUR::Session *);
	void set_overlays_dirty ();
	void set_background_dirty ();
	void routes_added (std::list<RouteTimeAxisView*> const &);

private:
	void parameter_changed (std::string);
	void presentation_info_changed (PBD::PropertyChange const &);
	void route_group_property_changed (PBD::PropertyChange const &);
	void on_size_allocate (Gtk::Allocation& alloc);

	bool on_button_press_event (GdkEventButton *);
	bool on_button_release_event (GdkEventButton *);
	bool on_motion_notify_event (GdkEventMotion *);

	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	void get_editor (std::pair<double, double>* y) const;
	void set_editor (double const y);
	double editor_y_to_summary (double) const;

	int _last_y;
	int _offset_y;
	double _last_editor_y;

	bool _move_dragging;

	std::pair<double, double> _view_rectangle;

	void set_colors ();

	uint32_t _viewrect_color;
	uint32_t _background_color;
	uint32_t _basetrack_color;
	uint32_t _audiotrack_color;
	uint32_t _miditrack_color;
	uint32_t _bus_color;
	uint32_t _vca_color;

	int _editor_scroll_height;

	cairo_surface_t* _image;
	void render_background_image ();
	bool _background_dirty;

	PBD::ScopedConnection route_ctrl_id_connection;
	PBD::ScopedConnectionList session_connections;
};
