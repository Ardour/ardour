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

#ifndef __port_matrix_row_labels_h__
#define __port_matrix_row_labels_h__

#include <boost/shared_ptr.hpp>
#include <gdkmm/color.h>
#include "port_matrix_labels.h"

class PortMatrix;
class PortMatrixBody;
class PortMatrixNode;

namespace ARDOUR {
	class Bundle;
	class BundleChannel;
}

namespace Gtk {
	class Menu;
}

/** The row labels part of the port matrix */
class PortMatrixRowLabels : public PortMatrixLabels
{
public:
	PortMatrixRowLabels (PortMatrix *, PortMatrixBody *);

	void button_press (double, double, GdkEventButton *);

	double component_to_parent_x (double x) const;
	double parent_to_component_x (double x) const;
	double component_to_parent_y (double y) const;
	double parent_to_component_y (double y) const;
	void mouseover_changed (std::list<PortMatrixNode> const &);
	void motion (double, double);

private:
	void render_channel_name (cairo_t *, Gdk::Color, Gdk::Color, double, double, ARDOUR::BundleChannel const &);
	void render_bundle_name (cairo_t *, Gdk::Color, Gdk::Color, double, double, boost::shared_ptr<ARDOUR::Bundle>);
	double channel_x (ARDOUR::BundleChannel const &) const;
	double channel_y (ARDOUR::BundleChannel const &) const;

	void render (cairo_t *);
	void compute_dimensions ();
	void remove_channel_proxy (boost::weak_ptr<ARDOUR::Bundle>, uint32_t);
	void rename_channel_proxy (boost::weak_ptr<ARDOUR::Bundle>, uint32_t);
	void queue_draw_for (ARDOUR::BundleChannel const &);
	double port_name_x () const;
	double bundle_name_x () const;

	double _longest_port_name;
	double _longest_bundle_name;
};

#endif
