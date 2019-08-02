/*
 * Copyright (C) 2009 Carl Hetherington <carl@carlh.net>
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

#ifndef __gtk_ardour_port_matrix_body_h__
#define __gtk_ardour_port_matrix_body_h__

#include <gtkmm/eventbox.h>
#include "port_group.h"
#include "port_matrix_types.h"

class PortMatrix;
class PortMatrixColumnLabels;
class PortMatrixRowLabels;
class PortMatrixGrid;
class PortMatrixComponent;

/** The main body of the port matrix.  It is made up of three parts:
 *  column labels, grid and row labels, each drawn using cairo.
 */
class PortMatrixBody : public Gtk::EventBox
{
public:
	PortMatrixBody (PortMatrix *);
	~PortMatrixBody ();

	void setup ();

	uint32_t full_scroll_width ();
	uint32_t alloc_scroll_width ();
	uint32_t full_scroll_height ();
	uint32_t alloc_scroll_height ();

	uint32_t xoffset () const {
		return _xoffset;
	}
	void set_xoffset (uint32_t);
	uint32_t yoffset () const {
		return _yoffset;
	}
	void set_yoffset (uint32_t);

	void rebuild_and_draw_grid ();

	void set_mouseover (PortMatrixNode const &);
	void set_mouseover (std::list<PortMatrixNode> const &);
	std::list<PortMatrixNode> mouseover () const {
		return _mouseover;
	}

	void highlight_associated_channels (int, ARDOUR::BundleChannel);
	void component_size_changed ();
	std::pair<uint32_t, uint32_t> max_size () const;

	uint32_t column_labels_border_x () const;
	uint32_t column_labels_height () const;

	sigc::signal<void> DimensionsChanged;

protected:
	bool on_expose_event (GdkEventExpose *);
	void on_size_request (Gtk::Requisition *);
	void on_size_allocate (Gtk::Allocation &);
	bool on_button_press_event (GdkEventButton *);
	bool on_button_release_event (GdkEventButton *);
	bool on_leave_notify_event (GdkEventCrossing *);
	bool on_motion_notify_event (GdkEventMotion *);

private:
	void compute_rectangles ();
	void rebuild_and_draw_column_labels ();
	void rebuild_and_draw_row_labels ();
	void update_bundles ();
	void set_cairo_clip (cairo_t *, Gdk::Rectangle const &) const;

	PortMatrix* _matrix;
	PortMatrixColumnLabels* _column_labels;
	PortMatrixRowLabels* _row_labels;
	PortMatrixGrid* _grid;
	std::list<PortMatrixComponent*> _components;

	uint32_t _alloc_width; ///< allocated width
	uint32_t _alloc_height; ///< allocated height
	uint32_t _xoffset;
	uint32_t _yoffset;
	uint32_t _column_labels_border_x;
	uint32_t _column_labels_height;

	std::list<PortMatrixNode> _mouseover;
	bool _ignore_component_size_changed;

	PBD::ScopedConnectionList _bundle_connections;
};

#endif
