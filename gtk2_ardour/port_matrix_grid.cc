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
#include "port_matrix_grid.h"
#include "port_matrix.h"

PortMatrixGrid::PortMatrixGrid (PortMatrix* p, PortMatrixBody* b)
	: PortMatrixComponent (b),
	  _port_matrix (p)
{
	
}

void
PortMatrixGrid::compute_dimensions ()
{
	_width = 0;
	for (uint32_t i = 0; i < _body->column_bundles().size(); ++i) {
		_width += _body->column_bundles()[i]->nchannels() * column_width();
	}

	_height = 0;
	for (uint32_t i = 0; i < _body->row_bundles().size(); ++i) {
		_height += _body->row_bundles()[i]->nchannels() * row_height();
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
	for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::size_type i = 0; i < _body->column_bundles().size(); ++i) {

		cairo_set_line_width (cr, thin_grid_line_width());
		for (uint32_t j = 1; j < _body->column_bundles()[i]->nchannels(); ++j) {
			x += column_width();
			cairo_move_to (cr, x, 0);
			cairo_line_to (cr, x, _height);
			cairo_stroke (cr);
		}

		if (i < (_body->column_bundles().size() - 1)) {
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
	for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::size_type i = 0; i < _body->row_bundles().size(); ++i) {

		cairo_set_line_width (cr, thin_grid_line_width());
		for (uint32_t j = 1; j < _body->row_bundles()[i]->nchannels(); ++j) {
			y += row_height();
			cairo_move_to (cr, 0, y);
			cairo_line_to (cr, grid_width, y);
			cairo_stroke (cr);
		}

		if (i < (_body->row_bundles().size() - 1)) {
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
	
	for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::const_iterator i = _body->column_bundles().begin(); i < _body->column_bundles().end(); ++i) {
		by = 0;
		
		for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::const_iterator j = _body->row_bundles().begin(); j < _body->row_bundles().end(); ++j) {

			x = bx;
			for (uint32_t k = 0; k < (*i)->nchannels (); k++) {

				y = by;
				for (uint32_t l = 0; l < (*j)->nchannels (); ++l) {
					
					PortMatrix::State const s = _port_matrix->get_state (*j, l, *i, k);
						
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


void
PortMatrixGrid::button_press (double x, double y, int b)
{
	uint32_t grid_column = x / column_width ();
	uint32_t grid_row = y / row_height ();

	boost::shared_ptr<ARDOUR::Bundle> our_bundle;
	uint32_t our_channel = 0;
	boost::shared_ptr<ARDOUR::Bundle> other_bundle;
	uint32_t other_channel = 0;

	for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::const_iterator i = _body->row_bundles().begin(); i != _body->row_bundles().end(); ++i) {
		if (grid_row < (*i)->nchannels ()) {
			our_bundle = *i;
			our_channel = grid_row;
			break;
		} else {
			grid_row -= (*i)->nchannels ();
		}
	}

	for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::const_iterator i = _body->column_bundles().begin(); i != _body->column_bundles().end(); ++i) {
		if (grid_column < (*i)->nchannels ()) {
			other_bundle = *i;
			other_channel = grid_column;
			break;
		} else {
			grid_column -= (*i)->nchannels ();
		}
	}

	
	if (our_bundle && other_bundle) {
		
		PortMatrix::State const s = _port_matrix->get_state (
			our_bundle, our_channel, other_bundle, other_channel
			);
				
		if (s == PortMatrix::ASSOCIATED || s == PortMatrix::NOT_ASSOCIATED) {

			bool const n = !(s == PortMatrix::ASSOCIATED);
			
		_port_matrix->set_state (
			our_bundle, our_channel, other_bundle, other_channel,
				n, 0
			);
		}

		require_render ();
		_body->queue_draw ();
	}
}


