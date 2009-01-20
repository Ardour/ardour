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

PortMatrixBody::PortMatrixBody (PortMatrix* p)
	: _port_matrix (p),
	  _column_labels (this),
	  _row_labels (p, this),
	  _grid (p, this),
	  _alloc_width (0),
	  _alloc_height (0),
	  _alloc_xdiv (0),
	  _alloc_ydiv (0),
	  _xoffset (0),
	  _yoffset (0)
{

}


bool
PortMatrixBody::on_expose_event (GdkEventExpose* event)
{
	Gdk::Rectangle const exposure (
		event->area.x, event->area.y, event->area.width, event->area.height
		);

	Gdk::Rectangle const col (0, 0, _alloc_width, _alloc_ydiv);
	Gdk::Rectangle const row (_alloc_xdiv, _alloc_ydiv, _alloc_width - _alloc_xdiv, _alloc_height - _alloc_ydiv);
	Gdk::Rectangle const grid (0, _alloc_ydiv, _alloc_xdiv, _alloc_height - _alloc_ydiv);

	bool intersects;
	Gdk::Rectangle r = exposure;
	r.intersect (col, intersects);

	if (intersects) {
		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			_column_labels.get_pixmap (get_window()->gobj()),
			r.get_x() + _xoffset,
			r.get_y(),
			r.get_x(),
			r.get_y(),
			r.get_width(),
			r.get_height()
			);
	}

	r = exposure;
	r.intersect (row, intersects);

	if (intersects) {
		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			_row_labels.get_pixmap (get_window()->gobj()),
			r.get_x() - _alloc_xdiv,
			r.get_y() + _yoffset - _alloc_ydiv,
			r.get_x(),
			r.get_y(),
			r.get_width(),
			r.get_height()
			);
	}

	r = exposure;
	r.intersect (grid, intersects);

	if (intersects) {
		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			_grid.get_pixmap (get_window()->gobj()),
			r.get_x() + _xoffset,
			r.get_y() + _yoffset - _alloc_ydiv,
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

	compute_divs ();
	_port_matrix->setup_scrollbars ();
}

void
PortMatrixBody::compute_divs ()
{
	std::pair<uint32_t, uint32_t> const col = _column_labels.dimensions ();
	if (_alloc_height > col.second) {
		/* allocated height is enough for the column labels */
		_alloc_ydiv = col.second;
	} else {
		/* not enough space for the column labels */
		_alloc_ydiv = _alloc_height;
	}

	std::pair<uint32_t, uint32_t> const grid = _grid.dimensions ();
	std::pair<uint32_t, uint32_t> const row = _row_labels.dimensions ();

	if (_alloc_width > (grid.first + row.first)) {
		/* allocated width is larger than we need, so
		   put the x division at the extent of the grid */
		_alloc_xdiv = grid.first;
	} else if (_alloc_width > row.first) {
		/* allocated width is large enough for the row labels
		   but not for the whole grid, so display the whole
		   row label section and cut part of the grid off */
		_alloc_xdiv = _alloc_width - row.first;
	} else {
		/* allocated width isn't even enough for the row labels */
		_alloc_xdiv = 0;
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
			(*i)->NameChanged.connect (sigc::mem_fun (*this, &PortMatrixBody::repaint_row_labels))
			);
		
	}

	for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::iterator i = _column_bundles.begin(); i != _column_bundles.end(); ++i) {
		_bundle_connections.push_back (
			(*i)->NameChanged.connect (sigc::mem_fun (*this, &PortMatrixBody::repaint_column_labels))
			);
	}
	
	_column_labels.setup ();
	_row_labels.setup ();
	_grid.setup ();

	compute_divs ();
}

uint32_t
PortMatrixBody::full_scroll_width ()
{
	return _grid.dimensions().first;

}

uint32_t
PortMatrixBody::alloc_scroll_width ()
{
	return _alloc_xdiv;
}

uint32_t
PortMatrixBody::full_scroll_height ()
{
	return _grid.dimensions().second;
}

uint32_t
PortMatrixBody::alloc_scroll_height ()
{
	return _alloc_height - _alloc_ydiv;
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
	if (ev->x < _alloc_xdiv && ev->y > _alloc_ydiv) {
		_grid.button_press (ev->x + _xoffset, ev->y + _yoffset - _alloc_ydiv, ev->button);
	} else if (ev->x > _alloc_xdiv && ev->y > _alloc_ydiv) {
		_row_labels.button_press (ev->x - _alloc_xdiv, ev->y + _yoffset - _alloc_ydiv, ev->button, ev->time);
	} else {
		return false;
	}

	return true;
}

void
PortMatrixBody::repaint_grid ()
{
	_grid.require_render ();
	queue_draw ();
}

void
PortMatrixBody::repaint_column_labels ()
{
	_column_labels.require_render ();
	queue_draw ();
}

void
PortMatrixBody::repaint_row_labels ()
{
	_row_labels.require_render ();
	queue_draw ();
}
