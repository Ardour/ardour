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

#include <iostream>
#include "ardour/bundle.h"
#include "port_matrix_body.h"
#include "port_matrix.h"

PortMatrixBody::PortMatrixBody (PortMatrix* p, Arrangement a)
	: _port_matrix (p),
	  _column_labels (this, a == TOP_AND_RIGHT ? PortMatrixColumnLabels::TOP : PortMatrixColumnLabels::BOTTOM),
	  _row_labels (p, this, a == BOTTOM_AND_LEFT ? PortMatrixRowLabels::LEFT : PortMatrixRowLabels::RIGHT),
	  _grid (p, this),
	  _arrangement (a),
	  _xoffset (0),
	  _yoffset (0)
{
	modify_bg (Gtk::STATE_NORMAL, Gdk::Color ("#00000"));
}


bool
PortMatrixBody::on_expose_event (GdkEventExpose* event)
{
	Gdk::Rectangle const exposure (
		event->area.x, event->area.y, event->area.width, event->area.height
		);

	bool intersects;
	Gdk::Rectangle r = exposure;
	r.intersect (_column_labels_rect, intersects);

	if (intersects) {
		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			_column_labels.get_pixmap (get_window()->gobj()),
			r.get_x() - _column_labels_rect.get_x() + _xoffset,
			r.get_y() - _column_labels_rect.get_y(),
			r.get_x(),
			r.get_y(),
			r.get_width(),
			r.get_height()
			);
	}

	r = exposure;
	r.intersect (_row_labels_rect, intersects);

	if (intersects) {
		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			_row_labels.get_pixmap (get_window()->gobj()),
			r.get_x() - _row_labels_rect.get_x(),
			r.get_y() - _row_labels_rect.get_y() + _yoffset,
			r.get_x(),
			r.get_y(),
			r.get_width(),
			r.get_height()
			);
	}

	r = exposure;
	r.intersect (_grid_rect, intersects);

	if (intersects) {
		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			_grid.get_pixmap (get_window()->gobj()),
			r.get_x() - _grid_rect.get_x() + _xoffset,
			r.get_y() - _grid_rect.get_y() + _yoffset,
			r.get_x(),
			r.get_y(),
			r.get_width(),
			r.get_height()
			);
	}

	return true;
}

void
PortMatrixBody::on_size_request (Gtk::Requisition *req)
{
	std::pair<int, int> const col = _column_labels.dimensions ();
	std::pair<int, int> const row = _row_labels.dimensions ();
	std::pair<int, int> const grid = _grid.dimensions ();

	req->width = std::max (col.first, grid.first + row.first);
	req->height = col.second + grid.second;
}

void
PortMatrixBody::on_size_allocate (Gtk::Allocation& alloc)
{
	Gtk::EventBox::on_size_allocate (alloc);
	set_allocation (alloc);

	_alloc_width = alloc.get_width ();
	_alloc_height = alloc.get_height ();

	compute_rectangles ();
	_port_matrix->setup_scrollbars ();
}

void
PortMatrixBody::compute_rectangles ()
{
	/* full sizes of components */
	std::pair<uint32_t, uint32_t> const col = _column_labels.dimensions ();
	std::pair<uint32_t, uint32_t> const row = _row_labels.dimensions ();
	std::pair<uint32_t, uint32_t> const grid = _grid.dimensions ();

	if (_arrangement == TOP_AND_RIGHT) {

		/* build from top left */

		_column_labels_rect.set_x (0);
		_column_labels_rect.set_y (0);
		_grid_rect.set_x (0);

		if (_alloc_width > col.first) {
			_column_labels_rect.set_width (col.first);
		} else {
			_column_labels_rect.set_width (_alloc_width);
		}

		/* move down to y division */
		
		uint32_t y = 0;
		if (_alloc_height > col.second) {
			y = col.second;
		} else {
			y = _alloc_height;
		}

		_column_labels_rect.set_height (y);
		_row_labels_rect.set_y (y);
		_row_labels_rect.set_height (_alloc_height - y);
		_grid_rect.set_y (y);
		_grid_rect.set_height (_alloc_height - y);

		/* move right to x division */

		uint32_t x = 0;
		if (_alloc_width > (grid.first + row.first)) {
			x = grid.first;
		} else if (_alloc_width > row.first) {
			x = _alloc_width - row.first;
		}

		_grid_rect.set_width (x);
		_row_labels_rect.set_x (x);
		_row_labels_rect.set_width (_alloc_width - x);
			

	} else if (_arrangement == BOTTOM_AND_LEFT) {

		/* build from bottom right */

		/* move left to x division */

		uint32_t x = 0;
		if (_alloc_width > (grid.first + row.first)) {
			x = grid.first;
		} else if (_alloc_width > row.first) {
			x = _alloc_width - row.first;
		}

		_grid_rect.set_x (_alloc_width - x);
		_grid_rect.set_width (x);
		_column_labels_rect.set_width (col.first - grid.first + x);
		_column_labels_rect.set_x (_alloc_width - _column_labels_rect.get_width());

		_row_labels_rect.set_width (std::min (_alloc_width - x, row.first));
		_row_labels_rect.set_x (_alloc_width - x - _row_labels_rect.get_width());

		/* move up to the y division */
		
		uint32_t y = 0;
		if (_alloc_height > col.second) {
			y = col.second;
		} else {
			y = _alloc_height;
		}

		_column_labels_rect.set_y (_alloc_height - y);
		_column_labels_rect.set_height (y);

		_grid_rect.set_height (std::min (grid.second, _alloc_height - y));
		_grid_rect.set_y (_alloc_height - y - _grid_rect.get_height());

		_row_labels_rect.set_height (_grid_rect.get_height());
		_row_labels_rect.set_y (_grid_rect.get_y());

	}
}

void
PortMatrixBody::setup (
	std::vector<boost::shared_ptr<ARDOUR::Bundle> > const & row,
	std::vector<boost::shared_ptr<ARDOUR::Bundle> > const & column
	)
{
	for (std::list<sigc::connection>::iterator i = _bundle_connections.begin(); i != _bundle_connections.end(); ++i) {
		i->disconnect ();
	}

	_bundle_connections.clear ();
	
	_row_bundles = row;
	_column_bundles = column;

	for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::iterator i = _row_bundles.begin(); i != _row_bundles.end(); ++i) {
		
		_bundle_connections.push_back (
			(*i)->NameChanged.connect (sigc::mem_fun (*this, &PortMatrixBody::rebuild_and_draw_row_labels))
			);
		
	}

	for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::iterator i = _column_bundles.begin(); i != _column_bundles.end(); ++i) {
		_bundle_connections.push_back (
			(*i)->NameChanged.connect (sigc::mem_fun (*this, &PortMatrixBody::rebuild_and_draw_column_labels))
			);
	}
	
	_column_labels.setup ();
	_row_labels.setup ();
	_grid.setup ();

	compute_rectangles ();
}

uint32_t
PortMatrixBody::full_scroll_width ()
{
	return _grid.dimensions().first;

}

uint32_t
PortMatrixBody::alloc_scroll_width ()
{
	return _grid_rect.get_width();
}

uint32_t
PortMatrixBody::full_scroll_height ()
{
	return _grid.dimensions().second;
}

uint32_t
PortMatrixBody::alloc_scroll_height ()
{
	return _grid_rect.get_height();
}

void
PortMatrixBody::set_xoffset (uint32_t xo)
{
	_xoffset = xo;
	queue_draw ();
}

void
PortMatrixBody::set_yoffset (uint32_t yo)
{
	_yoffset = yo;
	queue_draw ();
}

bool
PortMatrixBody::on_button_press_event (GdkEventButton* ev)
{
	if (Gdk::Region (_grid_rect).point_in (ev->x, ev->y)) {

		_grid.button_press (
			ev->x - _grid_rect.get_x() + _xoffset,
			ev->y - _grid_rect.get_y() + _yoffset,
			ev->button
			);

	} else if (Gdk::Region (_row_labels_rect).point_in (ev->x, ev->y)) {

		_row_labels.button_press (
			ev->x - _row_labels_rect.get_x(),
			ev->y - _row_labels_rect.get_y() + _yoffset,
			ev->button, ev->time
			);
	
	} else {
	
		return false;
		
	}

	return true;
}

void
PortMatrixBody::rebuild_and_draw_grid ()
{
	_grid.require_rebuild ();
	queue_draw ();
}

void
PortMatrixBody::rebuild_and_draw_column_labels ()
{
	_column_labels.require_rebuild ();
	queue_draw ();
}

void
PortMatrixBody::rebuild_and_draw_row_labels ()
{
	_row_labels.require_rebuild ();
	queue_draw ();
}
