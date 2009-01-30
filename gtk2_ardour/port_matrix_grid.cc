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
#include <cairo/cairo.h>
#include "ardour/bundle.h"
#include "ardour/types.h"
#include "port_matrix_grid.h"
#include "port_matrix.h"

PortMatrixGrid::PortMatrixGrid (PortMatrix* m, PortMatrixBody* b)
	: PortMatrixComponent (m, b)
{
	
}

void
PortMatrixGrid::compute_dimensions ()
{
	_width = 0;
	ARDOUR::BundleList const c = _matrix->columns()->bundles();
	for (ARDOUR::BundleList::const_iterator i = c.begin(); i != c.end(); ++i) {
		_width += (*i)->nchannels() * column_width();
	}

	_height = 0;
	ARDOUR::BundleList const r = _matrix->rows()->bundles();
	for (ARDOUR::BundleList::const_iterator i = r.begin(); i != r.end(); ++i) {
		_height += (*i)->nchannels() * row_height();
	}
}


void
PortMatrixGrid::render (cairo_t* cr)
{
	/* BACKGROUND */
	
	set_source_rgb (cr, background_colour());
	cairo_rectangle (cr, 0, 0, _width, _height);
	cairo_fill (cr);

	/* VERTICAL GRID LINES */
	
	set_source_rgb (cr, grid_colour());
	uint32_t x = 0;
	ARDOUR::BundleList const c = _matrix->columns()->bundles();
	for (ARDOUR::BundleList::size_type i = 0; i < c.size(); ++i) {

		cairo_set_line_width (cr, thin_grid_line_width());
		for (uint32_t j = 1; j < c[i]->nchannels(); ++j) {
			x += column_width();
			cairo_move_to (cr, x, 0);
			cairo_line_to (cr, x, _height);
			cairo_stroke (cr);
		}

		if (i < (c.size() - 1)) {
			x += column_width();
			cairo_set_line_width (cr, thick_grid_line_width());
			cairo_move_to (cr, x, 0);
			cairo_line_to (cr, x, _height);
			cairo_stroke (cr);
		}
        }
		
 	uint32_t grid_width = x + column_width();

	/* HORIZONTAL GRID LINES */
	
	uint32_t y = 0;
	ARDOUR::BundleList const r = _matrix->rows()->bundles();
	for (ARDOUR::BundleList::size_type i = 0; i < r.size(); ++i) {

		cairo_set_line_width (cr, thin_grid_line_width());
		for (uint32_t j = 1; j < r[i]->nchannels(); ++j) {
			y += row_height();
			cairo_move_to (cr, 0, y);
			cairo_line_to (cr, grid_width, y);
			cairo_stroke (cr);
		}

		if (i < (r.size() - 1)) {
			y += row_height();
			cairo_set_line_width (cr, thick_grid_line_width());
			cairo_move_to (cr, 0, y);
			cairo_line_to (cr, grid_width, y);
			cairo_stroke (cr);
		}
	}

	/* ASSOCIATION INDICATORS */
	
	uint32_t bx = 0;
	uint32_t by = 0;
	
	for (ARDOUR::BundleList::const_iterator i = c.begin(); i < c.end(); ++i) {
		by = 0;
		
		for (ARDOUR::BundleList::const_iterator j = r.begin(); j < r.end(); ++j) {

			x = bx;
			for (uint32_t k = 0; k < (*i)->nchannels (); k++) {

				y = by;
				for (uint32_t l = 0; l < (*j)->nchannels (); ++l) {

					ARDOUR::BundleChannel c[2];
					c[_matrix->column_index()] = ARDOUR::BundleChannel (*i, k);
					c[_matrix->row_index()] = ARDOUR::BundleChannel (*j, l);
					
					PortMatrix::State const s = _matrix->get_state (c);
						
					switch (s) {
					case PortMatrix::ASSOCIATED:
						set_source_rgba (cr, association_colour(), 0.5);
						cairo_arc (
							cr,
							x + column_width() / 2,
							y + column_width() / 2,
							(column_width() - (2 * connection_indicator_pad())) / 2,
							0,
							2 * M_PI
							);

						cairo_fill (cr);
						break;

					case PortMatrix::UNKNOWN:
						set_source_rgba (cr, unknown_colour(), 0.5);
						cairo_rectangle (
							cr,
							x + thick_grid_line_width(),
							y + thick_grid_line_width(),
							column_width() - 2 * thick_grid_line_width(),
							row_height() - 2 * thick_grid_line_width()
							);
						cairo_fill (cr);
						break;
					
					case PortMatrix::NOT_ASSOCIATED:
						break;
					}

					y += row_height();
				}
				x += column_width();
			}
			
			by += (*j)->nchannels () * row_height();
		}
		
		bx += (*i)->nchannels () * column_width();
	}
}


PortMatrixNode
PortMatrixGrid::position_to_node (double x, double y) const
{
	return PortMatrixNode (
		position_to_channel (y, _matrix->rows()->bundles(), row_height()),
		position_to_channel (x, _matrix->columns()->bundles(), column_width())
		);
}


ARDOUR::BundleChannel
PortMatrixGrid::position_to_channel (double p, ARDOUR::BundleList const& bundles, double inc) const
{
	uint32_t pos = p / inc;

	for (ARDOUR::BundleList::const_iterator i = bundles.begin(); i != bundles.end(); ++i) {
		if (pos < (*i)->nchannels()) {
			return ARDOUR::BundleChannel (*i, pos);
		} else {
			pos -= (*i)->nchannels();
		}
	}
			
	return ARDOUR::BundleChannel (boost::shared_ptr<ARDOUR::Bundle> (), 0);
}


double
PortMatrixGrid::channel_position (
	ARDOUR::BundleChannel bc,
	ARDOUR::BundleList const& bundles,
	double inc) const
{
	double p = 0;
	
	ARDOUR::BundleList::const_iterator i = bundles.begin ();
	while (i != bundles.end() && *i != bc.bundle) {
		p += inc * (*i)->nchannels();
		++i;
	}

	if (i == bundles.end()) {
		return 0;
	}

	p += inc * bc.channel;

	return p;
}

void
PortMatrixGrid::button_press (double x, double y, int b)
{
	PortMatrixNode const node = position_to_node (x, y);
	
	if (node.row.bundle && node.column.bundle) {

		ARDOUR::BundleChannel c[2];
		c[_matrix->row_index()] = node.row;
		c[_matrix->column_index()] = node.column;
		
		PortMatrix::State const s = _matrix->get_state (c);
		
		if (s == PortMatrix::ASSOCIATED || s == PortMatrix::NOT_ASSOCIATED) {

			bool const n = !(s == PortMatrix::ASSOCIATED);

			ARDOUR::BundleChannel c[2];
			c[_matrix->row_index()] = node.row;
			c[_matrix->column_index()] = node.column;
			
			_matrix->set_state (c, n);
		}

		require_render ();
		_body->queue_draw ();
	}
}

void
PortMatrixGrid::draw_extra (cairo_t* cr)
{
	set_source_rgba (cr, mouseover_line_colour(), 0.3);
	cairo_set_line_width (cr, mouseover_line_width());

	double const x = component_to_parent_x (
		channel_position (_body->mouseover().column, _matrix->columns()->bundles(), column_width()) + column_width() / 2
		);
	
	double const y = component_to_parent_y (
		channel_position (_body->mouseover().row, _matrix->rows()->bundles(), row_height()) + row_height() / 2
		);
	
	if (_body->mouseover().row.bundle) {

		cairo_move_to (cr, x, y);
		if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {
			cairo_line_to (cr, component_to_parent_x (0), y);
		} else if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {
			cairo_line_to (cr, _parent_rectangle.get_x() + _parent_rectangle.get_width(), y);
		}
		cairo_stroke (cr);
	}

	if (_body->mouseover().column.bundle) {

		cairo_move_to (cr, x, y);
		if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {
			cairo_line_to (cr, x, _parent_rectangle.get_y() + _parent_rectangle.get_height());
		} else if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {
			cairo_line_to (cr, x, component_to_parent_y (0));
		}
		cairo_stroke (cr);
	}
}

void
PortMatrixGrid::mouseover_changed (PortMatrixNode const& old)
{
	queue_draw_for (old);
	queue_draw_for (_body->mouseover());
}

void
PortMatrixGrid::mouseover_event (double x, double y)
{
	_body->set_mouseover (position_to_node (x, y));
}

void
PortMatrixGrid::queue_draw_for (PortMatrixNode const &n)
{
	if (n.row.bundle) {

		double const y = channel_position (n.row, _matrix->rows()->bundles(), row_height());
		_body->queue_draw_area (
			_parent_rectangle.get_x(),
			component_to_parent_y (y),
			_parent_rectangle.get_width(),
			row_height()
			);
	}

	if (n.column.bundle) {

		double const x = channel_position (n.column, _matrix->columns()->bundles(), column_width());
		
		_body->queue_draw_area (
			component_to_parent_x (x),
			_parent_rectangle.get_y(),
			column_width(),
			_parent_rectangle.get_height()
			);
	}
}

double
PortMatrixGrid::component_to_parent_x (double x) const
{
	return x - _body->xoffset() + _parent_rectangle.get_x();
}

double
PortMatrixGrid::parent_to_component_x (double x) const
{
	return x + _body->xoffset() - _parent_rectangle.get_x();
}

double
PortMatrixGrid::component_to_parent_y (double y) const
{
	return y - _body->yoffset() + _parent_rectangle.get_y();
}

double
PortMatrixGrid::parent_to_component_y (double y) const
{
	return y + _body->yoffset() - _parent_rectangle.get_y();
}

