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
#include <boost/weak_ptr.hpp>
#include <cairo.h>
#include "gtkmm2ext/keyboard.h"
#include "ardour/bundle.h"
#include "port_matrix_row_labels.h"
#include "port_matrix.h"
#include "port_matrix_body.h"
#include "i18n.h"
#include "utils.h"

using namespace std;

PortMatrixRowLabels::PortMatrixRowLabels (PortMatrix* m, PortMatrixBody* b)
	: PortMatrixLabels (m, b)
{

}

void
PortMatrixRowLabels::compute_dimensions ()
{
	cairo_surface_t* surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 200, 200);
	cairo_t* cr = cairo_create (surface);

	_longest_port_name = 0;
	_longest_bundle_name = 0;

	/* Compute maximum dimensions using all port groups, so that we allow for the largest and hence
	   we can change between visible groups without the size of the labels jumping around.
	*/

	for (PortGroupList::List::const_iterator i = _matrix->rows()->begin(); i != _matrix->rows()->end(); ++i) {

		PortGroup::BundleList const r = (*i)->bundles ();
		for (PortGroup::BundleList::const_iterator j = r.begin(); j != r.end(); ++j) {

			for (uint32_t k = 0; k < (*j)->bundle->nchannels().n_total(); ++k) {

				if (!_matrix->should_show ((*j)->bundle->channel_type(k))) {
					continue;
				}

				cairo_text_extents_t ext;
				cairo_text_extents (cr, (*j)->bundle->channel_name(k).c_str(), &ext);
				if (ext.width > _longest_port_name) {
					_longest_port_name = ext.width;
				}
			}

			cairo_text_extents_t ext;
			cairo_text_extents (cr, (*j)->bundle->name().c_str(), &ext);
			if (ext.width > _longest_bundle_name) {
				_longest_bundle_name = ext.width;
			}
		}
	}


	if (_matrix->visible_rows()) {
		_height = group_size (_matrix->visible_rows()) * grid_spacing ();
	} else {
		_height = 0;
	}

	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	_width = _longest_bundle_name +
		name_pad() * 2;

	if (!_matrix->show_only_bundles()) {
		_width += _longest_port_name;
		_width += name_pad() * 2;
	}
}


void
PortMatrixRowLabels::render (cairo_t* cr)
{
	/* BACKGROUND */

	set_source_rgb (cr, background_colour());
	cairo_rectangle (cr, 0, 0, _width, _height);
	cairo_fill (cr);

	/* BUNDLE AND PORT NAMES */

	double y = 0;
	int N = 0;
	int M = 0;

	PortGroup::BundleList const & bundles = _matrix->visible_rows()->bundles ();
	for (PortGroup::BundleList::const_iterator i = bundles.begin(); i != bundles.end(); ++i) {
		render_bundle_name (cr, background_colour (), (*i)->has_colour ? (*i)->colour : get_a_bundle_colour (N), 0, y, (*i)->bundle);

		if (!_matrix->show_only_bundles()) {
			uint32_t const N = _matrix->count_of_our_type ((*i)->bundle->nchannels());
			for (uint32_t j = 0; j < N; ++j) {
				Gdk::Color c = (*i)->has_colour ? (*i)->colour : get_a_bundle_colour (M);
				ARDOUR::BundleChannel bc (
					(*i)->bundle,
					(*i)->bundle->type_channel_to_overall (_matrix->type (), j)
					);
				
				render_channel_name (cr, background_colour (), c, 0, y, bc);
				y += grid_spacing();
				++M;
			}

			if (N == 0) {
				y += grid_spacing ();
			}
			
		} else {
			y += grid_spacing();
		}

		++N;
	}
}

void
PortMatrixRowLabels::button_press (double x, double y, GdkEventButton* ev)
{
	ARDOUR::BundleChannel w = position_to_channel (y, x, _matrix->visible_rows());

	if (
		(_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT && x > (_longest_port_name + name_pad() * 2)) ||
		(_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM && x < (_longest_bundle_name + name_pad() * 2))

		) {
			w.channel = -1;
	}

	if (Gtkmm2ext::Keyboard::is_delete_event (ev) && w.channel != -1) {
		_matrix->remove_channel (w);
	} else if (ev->button == 3) {
		_matrix->popup_menu (
			ARDOUR::BundleChannel (),
			w,
			ev->time
			);
	}
}

double
PortMatrixRowLabels::component_to_parent_x (double x) const
{
	/* Row labels don't scroll horizontally, so x conversion does not depend on xoffset */
	return x + _parent_rectangle.get_x();
}

double
PortMatrixRowLabels::parent_to_component_x (double x) const
{
	/* Row labels don't scroll horizontally, so x conversion does not depend on xoffset */
	return x - _parent_rectangle.get_x();
}

double
PortMatrixRowLabels::component_to_parent_y (double y) const
{
	return y - _body->yoffset() + _parent_rectangle.get_y();
}

double
PortMatrixRowLabels::parent_to_component_y (double y) const
{
	return y + _body->yoffset() - _parent_rectangle.get_y();
}


double
PortMatrixRowLabels::bundle_name_x () const
{
	double x = 0;

	if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT && !_matrix->show_only_bundles ()) {
		x = _longest_port_name + name_pad() * 2;
	}

	return x;
}

double
PortMatrixRowLabels::port_name_x () const
{
	if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {
		return _longest_bundle_name + name_pad() * 2;
	} else {
		return 0;
	}

	return 0;
}

void
PortMatrixRowLabels::render_bundle_name (
	cairo_t* cr, Gdk::Color fg_colour, Gdk::Color bg_colour, double xoff, double yoff, boost::shared_ptr<ARDOUR::Bundle> b
	)
{
	double const x = bundle_name_x ();

	int const n = _matrix->show_only_bundles() ? 1 : _matrix->count_of_our_type_min_1 (b->nchannels());
	set_source_rgb (cr, bg_colour);
	cairo_rectangle (cr, xoff + x, yoff, _longest_bundle_name + name_pad() * 2, grid_spacing() * n);
	cairo_fill_preserve (cr);
	set_source_rgb (cr, fg_colour);
	cairo_set_line_width (cr, label_border_width ());
	cairo_stroke (cr);

	cairo_text_extents_t ext;
	cairo_text_extents (cr, b->name().c_str(), &ext);
	double const off = (grid_spacing() - ext.height) / 2;

 	set_source_rgb (cr, text_colour());
 	cairo_move_to (cr, xoff + x + name_pad(), yoff + name_pad() + off);
 	cairo_show_text (cr, b->name().c_str());
}

void
PortMatrixRowLabels::render_channel_name (
	cairo_t* cr, Gdk::Color fg_colour, Gdk::Color bg_colour, double xoff, double yoff, ARDOUR::BundleChannel const& bc
	)
{
	set_source_rgb (cr, bg_colour);
	cairo_rectangle (cr, port_name_x() + xoff, yoff, _longest_port_name + name_pad() * 2, grid_spacing());
	cairo_fill_preserve (cr);
	set_source_rgb (cr, fg_colour);
	cairo_set_line_width (cr, label_border_width ());
	cairo_stroke (cr);

	if (_matrix->count_of_our_type (bc.bundle->nchannels()) > 1) {

		/* only plot the name if the bundle has more than one channel;
		   the name of a single channel is assumed to be redundant */

		cairo_text_extents_t ext;
		cairo_text_extents (cr, bc.bundle->channel_name(bc.channel).c_str(), &ext);
		double const off = (grid_spacing() - ext.height) / 2;

		set_source_rgb (cr, text_colour());
		cairo_move_to (cr, port_name_x() + xoff + name_pad(), yoff + name_pad() + off);
		cairo_show_text (cr, bc.bundle->channel_name(bc.channel).c_str());
	}
}

double
PortMatrixRowLabels::channel_x (ARDOUR::BundleChannel const &) const
{
	return 0;
}

double
PortMatrixRowLabels::channel_y (ARDOUR::BundleChannel const& bc) const
{
	return channel_to_position (bc, _matrix->visible_rows()) * grid_spacing ();
}

void
PortMatrixRowLabels::queue_draw_for (ARDOUR::BundleChannel const & bc)
{
	if (bc.bundle) {

		if (_matrix->show_only_bundles()) {
			_body->queue_draw_area (
				component_to_parent_x (bundle_name_x()) - 1,
				component_to_parent_y (channel_y (bc)) - 1,
				_longest_bundle_name + name_pad() * 2 + 2,
				grid_spacing() + 2
				);
		} else {
			_body->queue_draw_area (
				component_to_parent_x (port_name_x()) - 1,
				component_to_parent_y (channel_y (bc)) - 1,
				_longest_port_name + name_pad() * 2 + 2,
				grid_spacing() + 2
				);
		}
	}

}

void
PortMatrixRowLabels::mouseover_changed (list<PortMatrixNode> const &)
{
	list<PortMatrixNode> const m = _body->mouseover ();
	for (list<PortMatrixNode>::const_iterator i = m.begin(); i != m.end(); ++i) {

		ARDOUR::BundleChannel c = i->column;
		ARDOUR::BundleChannel r = i->row;

		if (PortMatrix::bundle_with_channels (c.bundle) && PortMatrix::bundle_with_channels (r.bundle)) {
			add_channel_highlight (r);
		} else if (r.bundle) {
			_body->highlight_associated_channels (_matrix->row_index(), r);
		}
	}
}

void
PortMatrixRowLabels::motion (double x, double y)
{
	ARDOUR::BundleChannel const w = position_to_channel (y, x, _matrix->visible_rows());

	uint32_t const bw = _longest_bundle_name + 2 * name_pad();

	bool done = false;

	if (w.bundle) {

		if (
			(_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM && x < bw) ||
			(_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT && x > (_width - bw) && x < _width)

			) {

			/* if the mouse is over a bundle name, highlight all channels in the bundle */

			list<PortMatrixNode> n;

			for (uint32_t i = 0; i < w.bundle->nchannels().n_total(); ++i) {
				if (!_matrix->should_show (w.bundle->channel_type (i))) {
					continue;
				}

				ARDOUR::BundleChannel const bc (w.bundle, i);
				n.push_back (PortMatrixNode (bc, ARDOUR::BundleChannel ()));
			}

			_body->set_mouseover (n);
			done = true;

		} else if (x < _width) {

			_body->set_mouseover (PortMatrixNode (w, ARDOUR::BundleChannel ()));
			done = true;

		}

	}

	if (!done) {
		/* not over any bundle */
		_body->set_mouseover (PortMatrixNode ());
		return;
	}
}
