/*
    Copyright (C) 2002-2009 Paul Davis

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

#ifndef  __gtk_ardour_port_matrix_grid_h__
#define  __gtk_ardour_port_matrix_grid_h__

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include "ardour/types.h"
#include "port_matrix_component.h"
#include "port_matrix_types.h"
#include "port_group.h"

class PortMatrix;
class PortMatrixBody;

namespace ARDOUR {
	class Bundle;
}

/**  The grid part of the port matrix */
class PortMatrixGrid : public PortMatrixComponent
{
public:
	PortMatrixGrid (PortMatrix *, PortMatrixBody *);

	void button_press (double, double, GdkEventButton *);
	void button_release (double, double, GdkEventButton *);
	void motion (double, double);

	double component_to_parent_x (double x) const;
	double parent_to_component_x (double x) const;
	double component_to_parent_y (double y) const;
	double parent_to_component_y (double y) const;
	void mouseover_changed (std::list<PortMatrixNode> const &);
	void draw_extra (cairo_t *);

private:

	void compute_dimensions ();
	void render (cairo_t *);
	void render_group_pair (cairo_t *, boost::shared_ptr<const PortGroup>, boost::shared_ptr<const PortGroup>, uint32_t, uint32_t);

	PortMatrixNode position_to_node (double, double) const;
	void queue_draw_for (std::list<PortMatrixNode> const &);
	void draw_association_indicator (cairo_t *, uint32_t, uint32_t, double p = 1);
	void draw_empty_square (cairo_t *, uint32_t, uint32_t);
	void draw_non_connectable_indicator (cairo_t *, uint32_t, uint32_t);
	std::list<PortMatrixNode> nodes_on_line (int, int, int, int) const;
	void set_association (PortMatrixNode, bool);
	bool toggle_state (PortMatrixNode::State) const;

	bool _dragging;
	bool _drag_valid;
	bool _moved;
	int _drag_start_x;
	int _drag_start_y;
	int _drag_x;
	int _drag_y;
};

#endif
