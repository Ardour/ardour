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
#include "ardour/types.h"
#include "port_matrix_body.h"
#include "port_matrix.h"
#include "port_matrix_column_labels.h"
#include "port_matrix_row_labels.h"
#include "port_matrix_grid.h"

using namespace std;

PortMatrixBody::PortMatrixBody (PortMatrix* p)
	: _matrix (p),
	  _xoffset (0),
	  _yoffset (0),
	  _mouse_over_grid (false)
{
	_column_labels = new PortMatrixColumnLabels (p, this);
	_row_labels = new PortMatrixRowLabels (p, this);
	_grid = new PortMatrixGrid (p, this);
	
	add_events (Gdk::LEAVE_NOTIFY_MASK | Gdk::POINTER_MOTION_MASK);
}


PortMatrixBody::~PortMatrixBody ()
{
	delete _column_labels;
	delete _row_labels;
	delete _grid;
}

bool
PortMatrixBody::on_expose_event (GdkEventExpose* event)
{
	Gdk::Rectangle const exposure (
		event->area.x, event->area.y, event->area.width, event->area.height
		);

	bool intersects;
	
	Gdk::Rectangle r = exposure;
	/* the get_pixmap call may cause things to be rerendered and sizes to change,
	   so fetch the pixmap before calculating where to put it */
	GdkPixmap* p = _column_labels->get_pixmap (get_window()->gobj());
	r.intersect (_column_labels->parent_rectangle(), intersects);

	if (intersects) {

		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			p,
			_column_labels->parent_to_component_x (r.get_x()),
			_column_labels->parent_to_component_y (r.get_y()),
			r.get_x(),
			r.get_y(),
			r.get_width(),
			r.get_height()
			);
	}

	r = exposure;
	p = _row_labels->get_pixmap (get_window()->gobj());
	r.intersect (_row_labels->parent_rectangle(), intersects);

	if (intersects) {
		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			p,
			_row_labels->parent_to_component_x (r.get_x()),
			_row_labels->parent_to_component_y (r.get_y()),
			r.get_x(),
			r.get_y(),
			r.get_width(),
			r.get_height()
			);
	}

	r = exposure;
	p = _grid->get_pixmap (get_window()->gobj());
	r.intersect (_grid->parent_rectangle(), intersects);

	if (intersects) {
		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			p,
			_grid->parent_to_component_x (r.get_x()),
			_grid->parent_to_component_y (r.get_y()),
			r.get_x(),
			r.get_y(),
			r.get_width(),
			r.get_height()
			);
	}

	cairo_t* cr = gdk_cairo_create (get_window()->gobj());

	cairo_save (cr);
	set_cairo_clip (cr, _grid->parent_rectangle ());
	_grid->draw_extra (cr);
	cairo_restore (cr);

	cairo_save (cr);
	set_cairo_clip (cr, _row_labels->parent_rectangle ());
	_row_labels->draw_extra (cr);
	cairo_restore (cr);

	cairo_save (cr);
	set_cairo_clip (cr, _column_labels->parent_rectangle ());
	_column_labels->draw_extra (cr);
	cairo_restore (cr);
	
	cairo_destroy (cr);

	return true;
}

void
PortMatrixBody::on_size_request (Gtk::Requisition *req)
{
	pair<int, int> const col = _column_labels->dimensions ();
	pair<int, int> const row = _row_labels->dimensions ();
	pair<int, int> const grid = _grid->dimensions ();

	/* don't ask for the maximum size of our contents, otherwise GTK won't
	   let the containing window shrink below this size */

	/* XXX these shouldn't be hard-coded */
	int const min_width = 512;
	int const min_height = 512;

	req->width = min (min_width, max (col.first, grid.first + row.first));
	req->height = min (min_height / _matrix->min_height_divisor(), col.second + grid.second);
}

void
PortMatrixBody::on_size_allocate (Gtk::Allocation& alloc)
{
	Gtk::EventBox::on_size_allocate (alloc);

	_alloc_width = alloc.get_width ();
	_alloc_height = alloc.get_height ();

	compute_rectangles ();
	_matrix->setup_scrollbars ();
}

void
PortMatrixBody::compute_rectangles ()
{
	/* full sizes of components */
	pair<uint32_t, uint32_t> const col = _column_labels->dimensions ();
	uint32_t col_overhang = _column_labels->overhang ();
	pair<uint32_t, uint32_t> const row = _row_labels->dimensions ();
	pair<uint32_t, uint32_t> const grid = _grid->dimensions ();

	Gdk::Rectangle col_rect;
	Gdk::Rectangle row_rect;
	Gdk::Rectangle grid_rect;

	if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {

		col_rect.set_x (0);
		col_rect.set_y (0);
		grid_rect.set_x (0);

		col_rect.set_width (min (col.first, _alloc_width));

		uint32_t const y = min (_alloc_height, col.second);
		col_rect.set_height (y);
		row_rect.set_y (y);
		row_rect.set_height (_alloc_height - y);
		grid_rect.set_y (y);
		grid_rect.set_height (_alloc_height - y);

		uint32_t x = 0;
		if (_alloc_width > (grid.first + row.first)) {
			x = grid.first;
		} else if (_alloc_width > row.first) {
			x = _alloc_width - row.first;
		}

		grid_rect.set_width (x);
		row_rect.set_x (x);
		row_rect.set_width (_alloc_width - x);
			

	} else if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {

		col_rect.set_height (min (_alloc_height, col.second));
		
		row_rect.set_x (0);
		row_rect.set_y (0);
		row_rect.set_width (min (_alloc_width, row.first));
		row_rect.set_height (std::min (_alloc_height - col_rect.get_height(), row.second));

		grid_rect.set_x (row_rect.get_width());
		grid_rect.set_y (0);
		grid_rect.set_width (std::min (_alloc_width - row_rect.get_width(), grid.first));
		grid_rect.set_height (row_rect.get_height ());

		col_rect.set_width (grid_rect.get_width () + col_overhang);
		col_rect.set_x (row_rect.get_width() + grid_rect.get_width() - col_rect.get_width());
		col_rect.set_y (row_rect.get_height());
		
	}

	_row_labels->set_parent_rectangle (row_rect);
	_column_labels->set_parent_rectangle (col_rect);
	_grid->set_parent_rectangle (grid_rect);
}

void
PortMatrixBody::setup ()
{
	/* Discard any old connections to bundles */
	
	for (list<sigc::connection>::iterator i = _bundle_connections.begin(); i != _bundle_connections.end(); ++i) {
		i->disconnect ();
	}
	_bundle_connections.clear ();

	/* Connect to bundles so that we find out when their names change */
	
	PortGroup::BundleList r = _matrix->rows()->bundles ();
	for (PortGroup::BundleList::iterator i = r.begin(); i != r.end(); ++i) {
		
		_bundle_connections.push_back (
			i->bundle->Changed.connect (sigc::hide (sigc::mem_fun (*this, &PortMatrixBody::rebuild_and_draw_row_labels)))
			);
		
	}

	PortGroup::BundleList c = _matrix->columns()->bundles ();
	for (PortGroup::BundleList::iterator i = c.begin(); i != c.end(); ++i) {
		_bundle_connections.push_back (
			i->bundle->Changed.connect (sigc::hide (sigc::mem_fun (*this, &PortMatrixBody::rebuild_and_draw_column_labels)))
			);
	}
	
	_column_labels->setup ();
	_row_labels->setup ();
	_grid->setup ();

	set_mouseover (PortMatrixNode ());
	compute_rectangles ();
}

uint32_t
PortMatrixBody::full_scroll_width ()
{
	return _grid->dimensions().first;

}

uint32_t
PortMatrixBody::alloc_scroll_width ()
{
	return _grid->parent_rectangle().get_width();
}

uint32_t
PortMatrixBody::full_scroll_height ()
{
	return _grid->dimensions().second;
}

uint32_t
PortMatrixBody::alloc_scroll_height ()
{
	return _grid->parent_rectangle().get_height();
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
	if (Gdk::Region (_grid->parent_rectangle()).point_in (ev->x, ev->y)) {

		_grid->button_press (
			_grid->parent_to_component_x (ev->x),
			_grid->parent_to_component_y (ev->y),
			ev->button, ev->time
			);

	} else if (Gdk::Region (_row_labels->parent_rectangle()).point_in (ev->x, ev->y)) {

		_row_labels->button_press (
			_row_labels->parent_to_component_x (ev->x),
			_row_labels->parent_to_component_y (ev->y),
			ev->button, ev->time
			);
	
	} else if (Gdk::Region (_column_labels->parent_rectangle()).point_in (ev->x, ev->y)) {

		_column_labels->button_press (
			_column_labels->parent_to_component_x (ev->x),
			_column_labels->parent_to_component_y (ev->y),
			ev->button, ev->time
			);
	}

	return true;
}

bool
PortMatrixBody::on_button_release_event (GdkEventButton* ev)
{
	if (Gdk::Region (_row_labels->parent_rectangle()).point_in (ev->x, ev->y) ||
	    Gdk::Region (_column_labels->parent_rectangle()).point_in (ev->x, ev->y)) {

		_row_labels->clear_channel_highlights ();
		_column_labels->clear_channel_highlights ();
		
	}

	return true;
}

void
PortMatrixBody::rebuild_and_draw_grid ()
{
	_grid->require_rebuild ();
	queue_draw ();
}

void
PortMatrixBody::rebuild_and_draw_column_labels ()
{
	_column_labels->require_rebuild ();
	queue_draw ();
}

void
PortMatrixBody::rebuild_and_draw_row_labels ()
{
	_row_labels->require_rebuild ();
	queue_draw ();
}

bool
PortMatrixBody::on_leave_notify_event (GdkEventCrossing* ev)
{
	if (ev->type == GDK_LEAVE_NOTIFY) {
		set_mouseover (PortMatrixNode ());
	}

	return true;
}

bool
PortMatrixBody::on_motion_notify_event (GdkEventMotion* ev)
{
	if (Gdk::Region (_grid->parent_rectangle()).point_in (ev->x, ev->y)) {
		_grid->mouseover_event (
			_grid->parent_to_component_x (ev->x),
			_grid->parent_to_component_y (ev->y)
			);
		_mouse_over_grid = true;
	} else {
		if (_mouse_over_grid) {
			set_mouseover (PortMatrixNode ());
			_mouse_over_grid = false;
		}
	}

	return true;
}

void
PortMatrixBody::set_mouseover (PortMatrixNode const & n)
{
	if (n == _mouseover) {
		return;
	}

	PortMatrixNode old = _mouseover;
	_mouseover = n;
	
	_grid->mouseover_changed (old);
	_row_labels->mouseover_changed (old);
	_column_labels->mouseover_changed (old);
}



void
PortMatrixBody::highlight_associated_channels (int dim, ARDOUR::BundleChannel h)
{
	ARDOUR::BundleChannel bc[2];
	bc[dim] = h;

	if (!bc[dim].bundle) {
		return;
	}

	if (dim == _matrix->column_index()) {
		_column_labels->add_channel_highlight (bc[dim]);
	} else {
		_row_labels->add_channel_highlight (bc[dim]);
	}

	PortGroup::BundleList const b = _matrix->ports(1 - dim)->bundles ();

	for (PortGroup::BundleList::const_iterator i = b.begin(); i != b.end(); ++i) {
	        for (uint32_t j = 0; j < i->bundle->nchannels(); ++j) {
			bc[1 - dim] = ARDOUR::BundleChannel (i->bundle, j);
			if (_matrix->get_state (bc) == PortMatrixNode::ASSOCIATED) {
				if (dim == _matrix->column_index()) {
					_row_labels->add_channel_highlight (bc[1 - dim]);
				} else {
					_column_labels->add_channel_highlight (bc[1 - dim]);
				}
			}
		}
	}
}

void
PortMatrixBody::set_cairo_clip (cairo_t* cr, Gdk::Rectangle const & r) const
{
	cairo_rectangle (cr, r.get_x(), r.get_y(), r.get_width(), r.get_height());
	cairo_clip (cr);
}

void
PortMatrixBody::component_size_changed ()
{
	compute_rectangles ();
	_matrix->setup_scrollbars ();
}

pair<uint32_t, uint32_t>
PortMatrixBody::max_size () const
{
	pair<uint32_t, uint32_t> const col = _column_labels->dimensions ();
	pair<uint32_t, uint32_t> const row = _row_labels->dimensions ();
	pair<uint32_t, uint32_t> const grid = _grid->dimensions ();
	
	return make_pair (std::max (row.first, _column_labels->overhang()) + grid.first, col.second + grid.second);
}
