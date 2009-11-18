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
#include "port_matrix_body.h"
#include "keyboard.h"

using namespace std;

PortMatrixGrid::PortMatrixGrid (PortMatrix* m, PortMatrixBody* b)
	: PortMatrixComponent (m, b),
	  _dragging (false),
	  _drag_valid (false),
	  _moved (false)
{

}

void
PortMatrixGrid::compute_dimensions ()
{
	if (_matrix->visible_columns() == 0) {
		_width = 0;
	} else {
		_width = group_size (_matrix->visible_columns()) * grid_spacing ();
	}

	if (_matrix->visible_rows() == 0) {
		_height = 0;
	} else {
		_height = group_size (_matrix->visible_rows()) * grid_spacing ();
	}
}


void
PortMatrixGrid::render (cairo_t* cr)
{
	set_source_rgb (cr, background_colour());
	cairo_rectangle (cr, 0, 0, _width, _height);
	cairo_fill (cr);

	PortGroup::BundleList const & row_bundles = _matrix->visible_rows()->bundles();
	PortGroup::BundleList const & column_bundles = _matrix->visible_columns()->bundles();

	uint32_t x = 0;

	/* VERTICAL GRID LINES */

	set_source_rgb (cr, grid_colour());
	uint32_t N = 0;

	for (PortGroup::BundleList::const_iterator i = column_bundles.begin(); i != column_bundles.end(); ++i) {

		cairo_set_line_width (cr, thick_grid_line_width());
		cairo_move_to (cr, x, 0);
		cairo_line_to (cr, x, _height);
		cairo_stroke (cr);

		if (!_matrix->show_only_bundles()) {
			cairo_set_line_width (cr, thin_grid_line_width());
			for (uint32_t j = 0; j < i->bundle->nchannels(); ++j) {
				x += grid_spacing ();
				cairo_move_to (cr, x, 0);
				cairo_line_to (cr, x, _height);
				cairo_stroke (cr);
			}

		} else {

			x += grid_spacing ();

		}

		++N;
	}

	uint32_t y = 0;

	/* HORIZONTAL GRID LINES */

	N = 0;
	for (PortGroup::BundleList::const_iterator i = row_bundles.begin(); i != row_bundles.end(); ++i) {

		cairo_set_line_width (cr, thick_grid_line_width());
		cairo_move_to (cr, 0, y);
		cairo_line_to (cr, _width, y);
		cairo_stroke (cr);

		if (!_matrix->show_only_bundles()) {
			cairo_set_line_width (cr, thin_grid_line_width());
			for (uint32_t j = 0; j < i->bundle->nchannels(); ++j) {
				y += grid_spacing ();
				cairo_move_to (cr, 0, y);
				cairo_line_to (cr, _width, y);
				cairo_stroke (cr);
			}

		} else {

			y += grid_spacing ();

		}

		++N;
	}

	/* ASSOCIATION INDICATORS */

	uint32_t bx = 0;
	uint32_t by = 0;

	if (_matrix->show_only_bundles()) {

		for (PortGroup::BundleList::const_iterator i = column_bundles.begin(); i != column_bundles.end(); ++i) {
			by = 0;

			for (PortGroup::BundleList::const_iterator j = row_bundles.begin(); j != row_bundles.end(); ++j) {

				PortMatrixNode::State s = get_association (PortMatrixNode (
										   ARDOUR::BundleChannel (i->bundle, 0),
										   ARDOUR::BundleChannel (j->bundle, 0)
										   ));
				switch (s) {
				case PortMatrixNode::ASSOCIATED:
					draw_association_indicator (cr, bx, by);
					break;
				case PortMatrixNode::PARTIAL:
					draw_association_indicator (cr, bx, by, 0.5);
					break;
				default:
					break;
				}

				by += grid_spacing();
			}

			bx += grid_spacing();

		}

	} else {

		for (PortGroup::BundleList::const_iterator i = column_bundles.begin(); i != column_bundles.end(); ++i) {
			by = 0;

			for (PortGroup::BundleList::const_iterator j = row_bundles.begin(); j != row_bundles.end(); ++j) {

				x = bx;
				for (uint32_t k = 0; k < i->bundle->nchannels (); ++k) {

					y = by;
					for (uint32_t l = 0; l < j->bundle->nchannels (); ++l) {

						ARDOUR::BundleChannel c[2];
						c[_matrix->column_index()] = ARDOUR::BundleChannel (i->bundle, k);
						c[_matrix->row_index()] = ARDOUR::BundleChannel (j->bundle, l);

						PortMatrixNode::State const s = _matrix->get_state (c);

						switch (s) {
						case PortMatrixNode::ASSOCIATED:
							draw_association_indicator (cr, x, y);
							break;

						case PortMatrixNode::NOT_ASSOCIATED:
							break;

						default:
							break;
						}

						y += grid_spacing();
					}

					x += grid_spacing();
				}

				by += j->bundle->nchannels () * grid_spacing();
			}

			bx += i->bundle->nchannels () * grid_spacing();
		}
	}
}

void
PortMatrixGrid::draw_association_indicator (cairo_t* cr, uint32_t x, uint32_t y, double p)
{
	set_source_rgba (cr, association_colour(), 0.5);

	cairo_arc (
		cr,
		x + grid_spacing() / 2,
		y + grid_spacing() / 2,
		(grid_spacing() - (2 * connection_indicator_pad())) / 2,
		0,
		p * 2 * M_PI
		);

	cairo_fill (cr);
}

void
PortMatrixGrid::draw_empty_square (cairo_t* cr, uint32_t x, uint32_t y)
{
	set_source_rgb (cr, background_colour());
	cairo_rectangle (
		cr,
		x + thick_grid_line_width(),
		y + thick_grid_line_width(),
		grid_spacing() - 2 * thick_grid_line_width(),
		grid_spacing() - 2 * thick_grid_line_width()
		);
	cairo_fill (cr);
}

PortMatrixNode
PortMatrixGrid::position_to_node (double x, double y) const
{
	return PortMatrixNode (
		position_to_channel (y, x, _matrix->visible_rows()),
		position_to_channel (x, y, _matrix->visible_columns())
		);
}

void
PortMatrixGrid::button_press (double x, double y, int b, uint32_t t, guint)
{
	ARDOUR::BundleChannel const px = position_to_channel (x, y, _matrix->visible_columns());
	ARDOUR::BundleChannel const py = position_to_channel (y, x, _matrix->visible_rows());

	if (b == 1) {
		
		_dragging = true;
		_drag_valid = (px.bundle && py.bundle);
		
		_moved = false;
		_drag_start_x = x / grid_spacing ();
		_drag_start_y = y / grid_spacing ();

	} else if (b == 3) {

		_matrix->popup_menu (px, py, t);

	}
}

PortMatrixNode::State
PortMatrixGrid::get_association (PortMatrixNode node) const
{
	if (_matrix->show_only_bundles()) {

		bool have_off_diagonal_association = false;
		bool have_diagonal_association = false;
		bool have_diagonal_not_association = false;

		for (uint32_t i = 0; i < node.row.bundle->nchannels (); ++i) {

			for (uint32_t j = 0; j < node.column.bundle->nchannels (); ++j) {

				ARDOUR::BundleChannel c[2];
				c[_matrix->column_index()] = ARDOUR::BundleChannel (node.row.bundle, i);
				c[_matrix->row_index()] = ARDOUR::BundleChannel (node.column.bundle, j);

				PortMatrixNode::State const s = _matrix->get_state (c);

				switch (s) {
				case PortMatrixNode::ASSOCIATED:
					if (i == j) {
						have_diagonal_association = true;
					} else {
						have_off_diagonal_association = true;
					}
					break;

				case PortMatrixNode::NOT_ASSOCIATED:
					if (i == j) {
						have_diagonal_not_association = true;
					}
					break;

				default:
					break;
				}
			}
		}

		if (have_diagonal_association && !have_off_diagonal_association && !have_diagonal_not_association) {
			return PortMatrixNode::ASSOCIATED;
		} else if (!have_diagonal_association && !have_off_diagonal_association) {
			return PortMatrixNode::NOT_ASSOCIATED;
		}

		return PortMatrixNode::PARTIAL;

	} else {

		ARDOUR::BundleChannel c[2];
		c[_matrix->column_index()] = node.column;
		c[_matrix->row_index()] = node.row;
		return _matrix->get_state (c);

	}

	/* NOTREACHED */
	return PortMatrixNode::NOT_ASSOCIATED;
}

void
PortMatrixGrid::set_association (PortMatrixNode node, bool s)
{
	if (_matrix->show_only_bundles()) {

		for (uint32_t i = 0; i < node.column.bundle->nchannels(); ++i) {
			for (uint32_t j = 0; j < node.row.bundle->nchannels(); ++j) {

				ARDOUR::BundleChannel c[2];
				c[_matrix->column_index()] = ARDOUR::BundleChannel (node.column.bundle, i);
				c[_matrix->row_index()] = ARDOUR::BundleChannel (node.row.bundle, j);
				_matrix->set_state (c, s && (i == j));
			}
		}

	} else {

		if (node.row.bundle && node.column.bundle) {

			ARDOUR::BundleChannel c[2];
			c[_matrix->row_index()] = node.row;
			c[_matrix->column_index()] = node.column;
			_matrix->set_state (c, s);
		}
	}
}

void
PortMatrixGrid::button_release (double x, double y, int b, uint32_t /*t*/, guint s)
{
	if (b == 1) {

		if (x != -1) {
			
			if (_dragging && _moved) {
				
				if (_drag_valid) {
					list<PortMatrixNode> const p = nodes_on_line (_drag_start_x, _drag_start_y, _drag_x, _drag_y);
					
					if (!p.empty()) {
						PortMatrixNode::State const s = get_association (p.front());
						for (list<PortMatrixNode>::const_iterator i = p.begin(); i != p.end(); ++i) {
							set_association (*i, toggle_state (s));
						}
					}
				}
				
			} else {
				
				if (Keyboard::modifier_state_equals (s, Keyboard::PrimaryModifier)) {
					/* associate/disassociate things diagonally down and right until we run out */
					PortMatrixNode::State s = (PortMatrixNode::State) 0;
					while (1) {
						PortMatrixNode const n = position_to_node (x, y);
						if (n.row.bundle && n.column.bundle) {
							if (s == (PortMatrixNode::State) 0) {
								s = get_association (n);
							}
							set_association (n, toggle_state (s));
						} else {
							break;
						}
						x += grid_spacing ();
						y += grid_spacing ();
					}
					
				} else {
					
					PortMatrixNode const n = position_to_node (x, y);
					if (n.row.bundle && n.column.bundle) {
						PortMatrixNode::State const s = get_association (n);
						set_association (n, toggle_state (s));
					}
				}
			}

			require_render ();
		}
		
		_body->queue_draw ();
	}

	_dragging = false;
}


void
PortMatrixGrid::draw_extra (cairo_t* cr)
{
	set_source_rgba (cr, mouseover_line_colour(), 0.3);
	cairo_set_line_width (cr, mouseover_line_width());

	list<PortMatrixNode> const m = _body->mouseover ();

	for (list<PortMatrixNode>::const_iterator i = m.begin(); i != m.end(); ++i) {
	
		double const x = component_to_parent_x (channel_to_position (i->column, _matrix->visible_columns()) * grid_spacing()) + grid_spacing() / 2;
		double const y = component_to_parent_y (channel_to_position (i->row, _matrix->visible_rows()) * grid_spacing()) + grid_spacing() / 2;

		if (i->row.bundle && i->column.bundle) {

			cairo_move_to (cr, x, y);
			if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {
				cairo_line_to (cr, component_to_parent_x (0), y);
			} else if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {
				cairo_line_to (cr, _parent_rectangle.get_x() + _parent_rectangle.get_width(), y);
			}
			cairo_stroke (cr);
			
			cairo_move_to (cr, x, y);
			if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {
				cairo_line_to (cr, x, _parent_rectangle.get_y() + _parent_rectangle.get_height());
			} else if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {
				cairo_line_to (cr, x, component_to_parent_y (0));
			}
			cairo_stroke (cr);
		}
	}

	if (_dragging && _drag_valid && _moved) {

		list<PortMatrixNode> const p = nodes_on_line (_drag_start_x, _drag_start_y, _drag_x, _drag_y);

		if (!p.empty()) {

			bool const s = toggle_state (get_association (p.front()));

			for (list<PortMatrixNode>::const_iterator i = p.begin(); i != p.end(); ++i) {
				if (s) {
					draw_association_indicator (
						cr,
						component_to_parent_x (channel_to_position (i->column, _matrix->visible_columns()) * grid_spacing ()),
						component_to_parent_y (channel_to_position (i->row, _matrix->visible_rows()) * grid_spacing ())
						);
				} else {
					draw_empty_square (
						cr,
						component_to_parent_x (channel_to_position (i->column, _matrix->visible_columns()) * grid_spacing ()),
						component_to_parent_y (channel_to_position (i->row, _matrix->visible_rows()) * grid_spacing ())
						);
				}
			}
		}

		set_source_rgba (cr, association_colour (), 0.3);

		cairo_move_to (
			cr,
			component_to_parent_x (_drag_start_x * grid_spacing() + grid_spacing() / 2),
			component_to_parent_y (_drag_start_y * grid_spacing() + grid_spacing() / 2)
			);

		cairo_line_to (
			cr,
			component_to_parent_x (_drag_x * grid_spacing() + grid_spacing() / 2),
			component_to_parent_y (_drag_y * grid_spacing() + grid_spacing() / 2)
			);

		cairo_stroke (cr);

	}
}

void
PortMatrixGrid::mouseover_changed (list<PortMatrixNode> const & old)
{
	queue_draw_for (old);
	queue_draw_for (_body->mouseover());
}

void
PortMatrixGrid::motion (double x, double y)
{
	_body->set_mouseover (position_to_node (x, y));

	int const px = x / grid_spacing ();
	int const py = y / grid_spacing ();

	if (_dragging && !_moved && ( (px != _drag_start_x || py != _drag_start_x) )) {
		_moved = true;
	}

	if (_dragging && _drag_valid && _moved) {
		_drag_x = px;
		_drag_y = py;
		_body->queue_draw ();
	}
}

void
PortMatrixGrid::queue_draw_for (list<PortMatrixNode> const &n)
{
	for (list<PortMatrixNode>::const_iterator i = n.begin(); i != n.end(); ++i) {
		
		if (i->row.bundle) {

			double const y = channel_to_position (i->row, _matrix->visible_rows()) * grid_spacing ();
			_body->queue_draw_area (
				_parent_rectangle.get_x(),
				component_to_parent_y (y),
				_parent_rectangle.get_width(),
				grid_spacing()
				);
		}

		if (i->column.bundle) {

			double const x = channel_to_position (i->column, _matrix->visible_columns()) * grid_spacing ();
			
			_body->queue_draw_area (
				component_to_parent_x (x),
				_parent_rectangle.get_y(),
				grid_spacing(),
				_parent_rectangle.get_height()
				);
		}
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

list<PortMatrixNode>
PortMatrixGrid::nodes_on_line (int x0, int y0, int x1, int y1) const
{
	list<PortMatrixNode> p;

	bool const steep = abs (y1 - y0) > abs (x1 - x0);
	if (steep) {
		int tmp = x0;
		x0 = y0;
		y0 = tmp;

		tmp = y1;
		y1 = x1;
		x1 = tmp;
	}

	if (x0 > x1) {
		int tmp = x0;
		x0 = x1;
		x1 = tmp;

		tmp = y0;
		y0 = y1;
		y1 = tmp;
	}

	int dx = x1 - x0;
	int dy = abs (y1 - y0);

	double err = 0;
	double derr = double (dy) / dx;

	int y = y0;
	int const ystep = y0 < y1 ? 1 : -1;

	for (int x = x0; x <= x1; ++x) {
		if (steep) {
			PortMatrixNode n = position_to_node (y * grid_spacing (), x * grid_spacing ());
			if (n.row.bundle && n.column.bundle) {
				p.push_back (n);
			}
		} else {
			PortMatrixNode n = position_to_node (x * grid_spacing (), y * grid_spacing ());
			if (n.row.bundle && n.column.bundle) {
				p.push_back (n);
			}
		}

		err += derr;

		if (err >= 0.5) {
			y += ystep;
			err -= 1;
		}
	}

	return p;
}

bool
PortMatrixGrid::toggle_state (PortMatrixNode::State s) const
{
	return (s == PortMatrixNode::NOT_ASSOCIATED || s == PortMatrixNode::PARTIAL);
}
