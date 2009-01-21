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

#ifndef __gtk_ardour_port_matrix_body_h__
#define __gtk_ardour_port_matrix_body_h__

#include "port_matrix_column_labels.h"
#include "port_matrix_row_labels.h"
#include "port_matrix_grid.h"

class PortMatrix;

/** The main body of the port matrix.  It is made up of three parts:
 *  column labels, grid and row labels, each drawn using cairo.
 *  This class handles the arrangement of these parts.
 */
class PortMatrixBody : public Gtk::EventBox
{
public:
	enum Arrangement {
		TOP_AND_RIGHT,
		BOTTOM_AND_LEFT
	};

	PortMatrixBody (PortMatrix *, Arrangement);

	/** @return bundles to offer for columns */
	std::vector<boost::shared_ptr<ARDOUR::Bundle> > const & column_bundles () {
		return _column_bundles;
	}

	/** @return bundles to offer for rows */
	std::vector<boost::shared_ptr<ARDOUR::Bundle> > const & row_bundles () {
		return _row_bundles;
	}
	
	void setup (
		std::vector<boost::shared_ptr<ARDOUR::Bundle> > const &,
		std::vector<boost::shared_ptr<ARDOUR::Bundle> > const &
		);

	uint32_t full_scroll_width ();
	uint32_t alloc_scroll_width ();
	uint32_t full_scroll_height ();
	uint32_t alloc_scroll_height ();

	void set_xoffset (uint32_t);
	void set_yoffset (uint32_t);

	void repaint_grid ();

protected:
	bool on_expose_event (GdkEventExpose *);
	void on_size_request (Gtk::Requisition *);
	void on_size_allocate (Gtk::Allocation &);
	bool on_button_press_event (GdkEventButton *);

private:
	void compute_rectangles ();
	void repaint_column_labels ();
	void repaint_row_labels ();
	
	PortMatrix* _port_matrix;
	PortMatrixColumnLabels _column_labels;
	PortMatrixRowLabels _row_labels;
	PortMatrixGrid _grid;

	Arrangement _arrangement;
	uint32_t _alloc_width; ///< allocated width
	uint32_t _alloc_height; ///< allocated height
	Gdk::Rectangle _column_labels_rect;
	Gdk::Rectangle _row_labels_rect;
	Gdk::Rectangle _grid_rect;
	uint32_t _xoffset;
	uint32_t _yoffset;

	/// bundles to offer for columns
	std::vector<boost::shared_ptr<ARDOUR::Bundle> > _column_bundles;
	/// bundles to offer for rows
	std::vector<boost::shared_ptr<ARDOUR::Bundle> > _row_bundles;

	std::list<sigc::connection> _bundle_connections;
};

#endif
