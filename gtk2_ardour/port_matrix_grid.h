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

class PortMatrix;
class PortMatrixBody;

namespace ARDOUR {
	class Bundle;
}

/// The grid part of the port matrix
class PortMatrixGrid : public PortMatrixComponent
{
public:
	PortMatrixGrid (PortMatrix *, PortMatrixBody *);

	void button_press (double, double, int);
	void mouseover_event (double, double);

	double component_to_parent_x (double x) const;
	double parent_to_component_x (double x) const;
	double component_to_parent_y (double y) const;
	double parent_to_component_y (double y) const;
	void mouseover_changed (PortMatrixNode const &);
	void draw_extra (cairo_t *);

private:
	
	void compute_dimensions ();
	void render (cairo_t *);

	double channel_position (PortMatrixBundleChannel, ARDOUR::BundleList const &, double) const;
	PortMatrixNode position_to_node (double, double) const;
	PortMatrixBundleChannel position_to_channel (double, ARDOUR::BundleList const &, double) const;
	void queue_draw_for (PortMatrixNode const &);

	PortMatrix* _port_matrix;
};

#endif
