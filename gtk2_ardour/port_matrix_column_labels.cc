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
#include "gtkmm2ext/keyboard.h"
#include "ardour/bundle.h"
#include "port_matrix_column_labels.h"
#include "port_matrix.h"
#include "port_matrix_body.h"
#include "utils.h"

#include "i18n.h"

using namespace std;

PortMatrixColumnLabels::PortMatrixColumnLabels (PortMatrix* m, PortMatrixBody* b)
	: PortMatrixLabels (m, b),
	  _overhang (0)
{

}

void
PortMatrixColumnLabels::compute_dimensions ()
{
	cairo_surface_t* surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 200, 200);
	cairo_t* cr = cairo_create (surface);

	/* width of the longest bundle name */
	_longest_bundle_name = 0;
	/* width of the longest channel name */
	_longest_channel_name = 0;

	/* Compute dimensions using all port groups, so that we allow for the largest and hence
	   we can change between visible groups without the size of the labels jumping around.
	*/

	for (PortGroupList::List::const_iterator i = _matrix->columns()->begin(); i != _matrix->columns()->end(); ++i) {
		PortGroup::BundleList const c = _matrix->columns()->bundles();
		for (PortGroup::BundleList::const_iterator j = c.begin (); j != c.end(); ++j) {

			cairo_text_extents_t ext;
			cairo_text_extents (cr, (*j)->bundle->name().c_str(), &ext);
			if (ext.width > _longest_bundle_name) {
				_longest_bundle_name = ext.width;
			}

			for (uint32_t k = 0; k < (*j)->bundle->nchannels().n_total(); ++k) {

				if (!_matrix->should_show ((*j)->bundle->channel_type(k))) {
					continue;
				}

				cairo_text_extents (
					cr,
					(*j)->bundle->channel_name (k).c_str(),
					&ext
					);

				if (ext.width > _longest_channel_name) {
					_longest_channel_name = ext.width;
				}
			}
		}
	}

	/* height metrics */
	cairo_text_extents_t ext;
	cairo_text_extents (cr, X_("AQRjpy"), &ext);
	_text_height = ext.height;
	_descender_height = ext.height + ext.y_bearing;

	/* width of the whole thing */
	if (_matrix->visible_columns()) {
		_width = group_size (_matrix->visible_columns()) * grid_spacing ();
	} else {
		_width = 0;
	}

	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	/* height of the whole thing */

	int a = _longest_bundle_name + 4 * name_pad();
	if (!_matrix->show_only_bundles()) {
		a += _longest_channel_name;
	}

	_height =  a * sin (angle()) + _text_height * cos (angle());
	_overhang = _height / tan (angle ());
	_width += _overhang;
}

double
PortMatrixColumnLabels::basic_text_x_pos (int) const
{
	return grid_spacing() / 2 +
		_text_height / (2 * sin (angle ()));
}

void
PortMatrixColumnLabels::render (cairo_t* cr)
{
	/* BACKGROUND */

	set_source_rgb (cr, background_colour());
	cairo_rectangle (cr, 0, 0, _width, _height);
	cairo_fill (cr);

        /* BUNDLE PARALLELOGRAM-TYPE-THING AND NAME */

	double x = 0;
	int N = 0;

	PortGroup::BundleList const & bundles = _matrix->visible_columns()->bundles ();
	for (PortGroup::BundleList::const_iterator i = bundles.begin (); i != bundles.end(); ++i) {

		Gdk::Color c = (*i)->has_colour ? (*i)->colour : get_a_bundle_colour (N);
		render_bundle_name (cr, background_colour (), c, x, 0, (*i)->bundle);

		if (_matrix->show_only_bundles()) {
			x += grid_spacing();
		} else {
			x += _matrix->count_of_our_type_min_1 ((*i)->bundle->nchannels()) * grid_spacing();
		}

		++N;
	}

	/* PORT NAMES */

	if (!_matrix->show_only_bundles()) {
		x = 0;
		N = 0;

		for (PortGroup::BundleList::const_iterator i = bundles.begin (); i != bundles.end(); ++i) {

			uint32_t const C = _matrix->count_of_our_type ((*i)->bundle->nchannels ());

			for (uint32_t j = 0; j < C; ++j) {
				Gdk::Color c = (*i)->has_colour ? (*i)->colour : get_a_bundle_colour (N);

				ARDOUR::BundleChannel bc (
					(*i)->bundle,
					(*i)->bundle->type_channel_to_overall (_matrix->type (), j)
					);
				
				render_channel_name (cr, background_colour (), c, x, 0, bc);
				x += grid_spacing();
			}

			if (C == 0) {
				x += grid_spacing ();
			}

			++N;
		}
	}
}

double
PortMatrixColumnLabels::component_to_parent_x (double x) const
{
	return x - _body->xoffset() + _parent_rectangle.get_x();
}

double
PortMatrixColumnLabels::parent_to_component_x (double x) const
{
	return x + _body->xoffset() - _parent_rectangle.get_x();
}

double
PortMatrixColumnLabels::component_to_parent_y (double y) const
{
	/* Column labels don't scroll vertically, so y conversion does not depend on yoffset */
	return y + _parent_rectangle.get_y();
}

double
PortMatrixColumnLabels::parent_to_component_y (double y) const
{
	/* Column labels don't scroll vertically, so y conversion does not depend on yoffset */
	return y - _parent_rectangle.get_y();
}

void
PortMatrixColumnLabels::mouseover_changed (list<PortMatrixNode> const &)
{
	list<PortMatrixNode> const m = _body->mouseover ();
	for (list<PortMatrixNode>::const_iterator i = m.begin(); i != m.end(); ++i) {

		ARDOUR::BundleChannel c = i->column;
		ARDOUR::BundleChannel r = i->row;

		if (PortMatrix::bundle_with_channels (c.bundle) && PortMatrix::bundle_with_channels (r.bundle)) {
			add_channel_highlight (c);
		} else if (c.bundle) {
			_body->highlight_associated_channels (_matrix->column_index(), c);
		}
	}
}

vector<pair<double, double> >
PortMatrixColumnLabels::port_name_shape (double xoff, double yoff) const
{
	vector<pair<double, double> > shape;

	double const lc = _longest_channel_name + name_pad();
	double const w = grid_spacing();

	if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {

		double x_ = xoff + _height / tan (angle()) + w;
		double y_ = yoff;
		shape.push_back (make_pair (x_, y_));
		x_ -= w;
		shape.push_back (make_pair (x_, y_));
		x_ -= lc * cos (angle());
		y_ += lc * sin (angle());
		shape.push_back (make_pair (x_, y_));
		x_ += w * pow (sin (angle()), 2);
		y_ += w * sin (angle()) * cos (angle());
		shape.push_back (make_pair (x_, y_));

	} else {

		double x_ = xoff;
		double y_ = yoff + _height;
		shape.push_back (make_pair (x_, y_));
		x_ += w;
		shape.push_back (make_pair (x_, y_));
		x_ += lc * cos (angle());
		y_ -= lc * sin (angle());
		shape.push_back (make_pair (x_, y_));
		x_ -= grid_spacing() * pow (sin (angle()), 2);
		y_ -= grid_spacing() * sin (angle()) * cos (angle());
		shape.push_back (make_pair (x_, y_));
	}

	return shape;
}

void
PortMatrixColumnLabels::render_bundle_name (
	cairo_t* cr, Gdk::Color fg_colour, Gdk::Color bg_colour, double xoff, double yoff, boost::shared_ptr<ARDOUR::Bundle> b
	)
{
	set_source_rgb (cr, bg_colour);

	double w = 0;
	if (_matrix->show_only_bundles()) {
		w = grid_spacing ();
	} else {
		w = _matrix->count_of_our_type_min_1 (b->nchannels()) * grid_spacing();
	}

	double x_ = xoff;

	uint32_t y = yoff;
	y += _height;

	double y_ = y;
	cairo_move_to (cr, x_, y_);
	x_ += w;
	cairo_line_to (cr, x_, y_);
	x_ += _height / tan (angle ());
	y_ -= _height;
	cairo_line_to (cr, x_, y_);
	x_ -= w;
	cairo_line_to (cr, x_, y_);
	cairo_line_to (cr, xoff, y);
	cairo_fill_preserve (cr);
	set_source_rgb (cr, fg_colour);
	cairo_set_line_width (cr, label_border_width());
	cairo_stroke (cr);

	set_source_rgb (cr, text_colour());

	double const q = ((grid_spacing() * sin (angle())) - _text_height) / 2 + _descender_height;

	if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {

		double rl = 0;
		if (_matrix->show_only_bundles()) {
			rl = name_pad();
		} else {
			rl = 3 * name_pad() + _longest_channel_name;
		}
		cairo_move_to (
			cr,
			xoff + grid_spacing() - q * sin (angle ()) + rl * cos (angle()),
			yoff + _height - q * cos (angle ()) - rl * sin (angle())
			);

	} else {

		cairo_move_to (
			cr,
			xoff + grid_spacing() - q * sin (angle ()),
			yoff + _height - q * cos (angle ())
			);
	}

	cairo_save (cr);
	cairo_rotate (cr, -angle());
	cairo_show_text (cr, b->name().c_str());
	cairo_restore (cr);
}

void
PortMatrixColumnLabels::render_channel_name (
	cairo_t* cr, Gdk::Color fg_colour, Gdk::Color bg_colour, double xoff, double yoff, ARDOUR::BundleChannel const &bc
	)
{
	vector<pair<double, double> > const shape = port_name_shape (xoff, yoff);

	cairo_move_to (cr, shape[0].first, shape[0].second);
	for (uint32_t i = 1; i < 4; ++i) {
		cairo_line_to (cr, shape[i].first, shape[i].second);
	}
	cairo_line_to (cr, shape[0].first, shape[0].second);

	set_source_rgb (cr, bg_colour);
	cairo_fill_preserve (cr);
	set_source_rgb (cr, fg_colour);
	cairo_set_line_width (cr, label_border_width());
	cairo_stroke (cr);

	set_source_rgb (cr, text_colour());

	double const q = ((grid_spacing() * sin (angle())) - _text_height) / 2 + _descender_height;

	if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {

		cairo_move_to (
			cr,
			xoff + grid_spacing() - q * sin (angle ()),
			yoff + _height - q * cos (angle ())
			);


	} else {

		double const rl = 3 * name_pad() + _longest_bundle_name;
		cairo_move_to (
			cr,
			xoff + grid_spacing() - q * sin (angle ()) + rl * cos (angle ()),
			yoff + _height - q * cos (angle ()) - rl * sin (angle())
			);
	}

	if (_matrix->count_of_our_type (bc.bundle->nchannels()) > 1) {

		/* only plot the name if the bundle has more than one channel;
		   the name of a single channel is assumed to be redundant */

		cairo_save (cr);
		cairo_rotate (cr, -angle());

		cairo_show_text (
			cr,
			bc.bundle->channel_name(bc.channel).c_str()
			);

		cairo_restore (cr);
	}
}

double
PortMatrixColumnLabels::channel_x (ARDOUR::BundleChannel const &bc) const
{
	return channel_to_position (bc, _matrix->visible_columns()) * grid_spacing ();
}

double
PortMatrixColumnLabels::channel_y (ARDOUR::BundleChannel const &) const
{
	return 0;
}

void
PortMatrixColumnLabels::queue_draw_for (ARDOUR::BundleChannel const & bc)
{
	if (!bc.bundle) {
		return;
	}

	if (_matrix->show_only_bundles()) {

		_body->queue_draw_area (
			component_to_parent_x (channel_x (bc)) - 1,
			component_to_parent_y (0) - 1,
			grid_spacing() + _height * tan (angle()) + 2,
			_height + 2
			);

	} else {

		double const x = channel_x (bc);
		double const lc = _longest_channel_name + name_pad();
		double const h = lc * sin (angle ()) + grid_spacing() * sin (angle()) * cos (angle());

		if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {

			_body->queue_draw_area (
				component_to_parent_x (x) - 1,
				component_to_parent_y (_height - h) - 1,
				grid_spacing() + lc * cos (angle()) + 2,
				h + 2
				);

		} else if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {

			double const x_ = x + _height / tan (angle()) - lc * cos (angle());

			_body->queue_draw_area (
				component_to_parent_x (x_) - 1,
				component_to_parent_y (0) - 1,
				grid_spacing() + lc * cos (angle()) + 2,
				h + 2
				);

		}

	}
}

ARDOUR::BundleChannel
PortMatrixColumnLabels::position_to_channel (double p, double o, boost::shared_ptr<const PortGroup> group) const
{
	uint32_t const cx = p - (_height - o) * tan (angle ());
	return PortMatrixComponent::position_to_channel (cx, o, group);
}

void
PortMatrixColumnLabels::button_press (double x, double y, GdkEventButton* ev)
{
	ARDOUR::BundleChannel w = position_to_channel (x, y, _matrix->visible_columns());

	if (
		(_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM && y > (_height - _longest_bundle_name * sin (angle ()))) ||
		(_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT && y < (_longest_bundle_name * sin (angle ())))
		) {

		w.channel = -1;
	}

	if (Gtkmm2ext::Keyboard::is_delete_event (ev) && w.channel != -1) {
		_matrix->remove_channel (w);
	} else if (ev->button == 3) {
		_matrix->popup_menu (
			w,
			ARDOUR::BundleChannel (),
			ev->time
			);
	}
}

void
PortMatrixColumnLabels::motion (double x, double y)
{
	ARDOUR::BundleChannel const w = position_to_channel (x, y, _matrix->visible_columns());

	if (w.bundle == 0) {
		_body->set_mouseover (PortMatrixNode ());
		return;
	}

	uint32_t const bh = _longest_channel_name * sin (angle ()) + _text_height / cos (angle ());

	if (
		(_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM && y > bh) ||
		(_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT && y < (_height - bh))
		) {

		/* if the mouse is over a bundle name, highlight all channels in the bundle */

		list<PortMatrixNode> n;

		for (uint32_t i = 0; i < w.bundle->nchannels().n_total(); ++i) {
			if (!_matrix->should_show (w.bundle->channel_type (i))) {
				continue;
			}
			
			ARDOUR::BundleChannel const bc (w.bundle, i);
			n.push_back (PortMatrixNode (ARDOUR::BundleChannel (), bc));
		}

		_body->set_mouseover (n);

	} else {

		_body->set_mouseover (PortMatrixNode (ARDOUR::BundleChannel (), w));
	}
}
