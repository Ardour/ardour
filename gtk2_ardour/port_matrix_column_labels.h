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

#ifndef __port_matrix_column_labels_h__
#define __port_matrix_column_labels_h__

#include <boost/shared_ptr.hpp>
#include "port_matrix_labels.h"

namespace ARDOUR {
	class Bundle;
	class BundleChannel;
}

class PortMatrixNode;

/** The column labels part of the port matrix */
class PortMatrixColumnLabels : public PortMatrixLabels
{
public:
	PortMatrixColumnLabels (PortMatrix *, PortMatrixBody *);

	void button_press (double, double, int, uint32_t);
  
	double component_to_parent_x (double x) const;
	double parent_to_component_x (double x) const;
	double component_to_parent_y (double y) const;
	double parent_to_component_y (double y) const;
	void mouseover_changed (PortMatrixNode const &);

	uint32_t overhang () const {
		return _overhang;
	}
		
private:
	void render_bundle_name (cairo_t *, Gdk::Color, Gdk::Color, double, double, boost::shared_ptr<ARDOUR::Bundle>);
	void render_channel_name (cairo_t *, Gdk::Color, Gdk::Color, double, double, ARDOUR::BundleChannel const &);
	double channel_x (ARDOUR::BundleChannel const &) const;
	double channel_y (ARDOUR::BundleChannel const &) const;
	void queue_draw_for (ARDOUR::BundleChannel const &);
	
	void render (cairo_t *);
	void compute_dimensions ();
	double basic_text_x_pos (int) const;
	std::vector<std::pair<double, double> > port_name_shape (double, double) const;

	double slanted_height () const {
		return _height - _highest_group_name - 2 * name_pad();
	}

	double _longest_bundle_name;
	double _longest_channel_name;
	double _highest_text;
	double _highest_group_name;
	uint32_t _overhang;
};

#endif
