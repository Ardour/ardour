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
#include "port_matrix_column_labels.h"
#include "port_matrix.h"
#include "port_matrix_body.h"
#include "utils.h"

using namespace std;

PortMatrixColumnLabels::PortMatrixColumnLabels (PortMatrix* m, PortMatrixBody* b)
	: PortMatrixLabels (m, b),
	  _overhang (0)
{

}

void
PortMatrixColumnLabels::compute_dimensions ()
{
	GdkPixmap* pm = gdk_pixmap_new (NULL, 1, 1, 24);
	gdk_drawable_set_colormap (pm, gdk_colormap_get_system());
	cairo_t* cr = gdk_cairo_create (pm);

	/* width of the longest bundle name */
	_longest_bundle_name = 0;
	/* width of the longest channel name */
	_longest_channel_name = 0;
	/* height of highest bit of text (apart from group names) */
	_highest_text = 0;
	/* width of the whole thing */
	_width = 0;
	_highest_group_name = 0;

	for (PortGroupList::List::const_iterator i = _matrix->columns()->begin(); i != _matrix->columns()->end(); ++i) {
		PortGroup::BundleList const c = _matrix->columns()->bundles();
		for (PortGroup::BundleList::const_iterator j = c.begin (); j != c.end(); ++j) {

			cairo_text_extents_t ext;
			cairo_text_extents (cr, j->bundle->name().c_str(), &ext);
			if (ext.width > _longest_bundle_name) {
				_longest_bundle_name = ext.width;
			}

			if (ext.height > _highest_text) {
				_highest_text = ext.height;
			}

			for (uint32_t k = 0; k < j->bundle->nchannels (); ++k) {

				cairo_text_extents (
					cr,
					j->bundle->channel_name (k).c_str(),
					&ext
					);

				if (ext.width > _longest_channel_name) {
					_longest_channel_name = ext.width;
				}

				if (ext.height > _highest_text) {
					_highest_text = ext.height;
				}
			}
		}

		_width += group_size (*i) * grid_spacing ();

		cairo_text_extents_t ext;
		cairo_text_extents (cr, (*i)->name.c_str(), &ext);
		if (ext.height > _highest_group_name) {
			_highest_group_name = ext.height;
		}
	}

	cairo_destroy (cr);
	gdk_pixmap_unref (pm);

	/* height of the whole thing */

	int a = _longest_bundle_name + 4 * name_pad();
	if (!_matrix->show_only_bundles()) {
		a += _longest_channel_name;
	}

	double const parallelogram_height =  a * sin (angle()) + _highest_text * cos (angle());

	_height = parallelogram_height + _highest_group_name + 2 * name_pad();

	_overhang = parallelogram_height / tan (angle ());
	_width += _overhang;
}

double
PortMatrixColumnLabels::basic_text_x_pos (int) const
{
	return grid_spacing() / 2 +
		_highest_text / (2 * sin (angle ()));
}

void
PortMatrixColumnLabels::render (cairo_t* cr)
{
	/* BACKGROUND */

	set_source_rgb (cr, background_colour());
	cairo_rectangle (cr, 0, 0, _width, _height);
	cairo_fill (cr);

	/* PORT GROUP NAME */

	double x = 0;
	double y = 0;

	if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {
		x = slanted_height() / tan (angle());
		y = _highest_group_name + name_pad();
	} else {
		x = 0;
		y = _height - name_pad();
	}

	int g = 0;
	for (PortGroupList::List::const_iterator i = _matrix->columns()->begin(); i != _matrix->columns()->end(); ++i) {

		/* compute width of this group */
		uint32_t w = 0;
		if (!(*i)->visible()) {
			w = grid_spacing ();
		} else {
			if (_matrix->show_only_bundles()) {
				w = (*i)->bundles().size() * grid_spacing();
			} else {
				w = (*i)->total_channels() * grid_spacing();
			}
		}

		if (w == 0) {
			continue;
		}

		/* rectangle */
		set_source_rgb (cr, get_a_group_colour (g));
		double const rh = _highest_group_name + 2 * name_pad();
		if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {
			cairo_rectangle (cr, x, 0, w, rh);
		} else {
			cairo_rectangle (cr, x, _height - rh, w, rh);
		}
		cairo_fill (cr);

		string const upper = Glib::ustring ((*i)->name).uppercase ();
		pair<string, double> const display = fit_to_pixels (cr, upper, w);

		/* plot it */
		set_source_rgb (cr, text_colour());
		cairo_move_to (cr, x + (w - display.second) / 2, y);
		cairo_show_text (cr, display.first.c_str());

		x += w;
		++g;

	}

        /* BUNDLE PARALLELOGRAM-TYPE-THING AND NAME */

	x = 0;
	int N = 0;

	for (PortGroupList::List::const_iterator i = _matrix->columns()->begin(); i != _matrix->columns()->end(); ++i) {

		if ((*i)->visible ()) {

			PortGroup::BundleList const & bundles = (*i)->bundles ();
			for (PortGroup::BundleList::const_iterator j = bundles.begin (); j != bundles.end(); ++j) {

				Gdk::Color c = j->has_colour ? j->colour : get_a_bundle_colour (N);
				render_bundle_name (cr, background_colour (), c, x, 0, j->bundle);

				if (_matrix->show_only_bundles()) {
					x += grid_spacing();
				} else {
					x += j->bundle->nchannels () * grid_spacing();
				}

				++N;
			}

		} else {

			x += grid_spacing ();

		}
	}


	/* PORT NAMES */

	if (!_matrix->show_only_bundles()) {
		x = 0;
		N = 0;
		for (PortGroupList::List::const_iterator i = _matrix->columns()->begin(); i != _matrix->columns()->end(); ++i) {

			if ((*i)->visible ()) {

				PortGroup::BundleList const & bundles = (*i)->bundles ();
				for (PortGroup::BundleList::const_iterator j = bundles.begin (); j != bundles.end(); ++j) {

					for (uint32_t k = 0; k < j->bundle->nchannels(); ++k) {
						Gdk::Color c = j->has_colour ? j->colour : get_a_bundle_colour (N);
						render_channel_name (cr, background_colour (), c, x, 0, ARDOUR::BundleChannel (j->bundle, k));
						x += grid_spacing();
					}

					++N;
				}

			} else {

				x += grid_spacing ();

			}
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
	return y + _parent_rectangle.get_y();
}

double
PortMatrixColumnLabels::parent_to_component_y (double y) const
{
	return y - _parent_rectangle.get_y();
}

void
PortMatrixColumnLabels::mouseover_changed (PortMatrixNode const &)
{
	clear_channel_highlights ();
	if (_body->mouseover().column.bundle && _body->mouseover().row.bundle) {
		add_channel_highlight (_body->mouseover().column);
	}
}

vector<pair<double, double> >
PortMatrixColumnLabels::port_name_shape (double xoff, double yoff) const
{
	vector<pair<double, double> > shape;

	double const lc = _longest_channel_name + name_pad();
	double const w = grid_spacing();

	if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {

		double x_ = xoff + slanted_height() / tan (angle()) + w;
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
		w = b->nchannels() * grid_spacing();
	}

	double x_ = xoff;

	uint32_t y = yoff;
	if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {
		y += _height;
	} else {
		y += slanted_height();
	}

	double y_ = y;
	cairo_move_to (cr, x_, y_);
	x_ += w;
	cairo_line_to (cr, x_, y_);
	x_ += slanted_height() / tan (angle ());
	y_ -= slanted_height();
	cairo_line_to (cr, x_, y_);
	x_ -= w;
	cairo_line_to (cr, x_, y_);
	cairo_line_to (cr, xoff, y);
	cairo_fill_preserve (cr);
	set_source_rgb (cr, fg_colour);
	cairo_set_line_width (cr, label_border_width());
	cairo_stroke (cr);

	set_source_rgb (cr, text_colour());

	if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {

		double rl = 0;
		if (_matrix->show_only_bundles()) {
			rl = name_pad();
		} else {
			rl = 3 * name_pad() + _longest_channel_name;
		}
		cairo_move_to (
			cr,
			xoff + basic_text_x_pos (0) + rl * cos (angle()),
			yoff + _height - rl * sin (angle())
			);

	} else {

		cairo_move_to (
			cr,
			xoff + basic_text_x_pos (0),
			yoff + slanted_height() - name_pad() * sin (angle())
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

	if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {

		cairo_move_to (
			cr,
			xoff + basic_text_x_pos(bc.channel),
			yoff + _height - name_pad() * sin (angle())
			);

	} else {

		double const rl = 3 * name_pad() + _longest_bundle_name;
		cairo_move_to (
			cr,
			xoff + basic_text_x_pos(bc.channel) + rl * cos (angle ()),
			yoff + slanted_height() - rl * sin (angle())
			);
	}

	cairo_save (cr);
	cairo_rotate (cr, -angle());

	cairo_show_text (
		cr,
		bc.bundle->channel_name(bc.channel).c_str()
		);

	cairo_restore (cr);
}

double
PortMatrixColumnLabels::channel_x (ARDOUR::BundleChannel const &bc) const
{
	return channel_to_position (bc, _matrix->columns()) * grid_spacing ();
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

			double const x_ = x + slanted_height() / tan (angle()) - lc * cos (angle());

			_body->queue_draw_area (
				component_to_parent_x (x_) - 1,
				component_to_parent_y (0) - 1,
				grid_spacing() + lc * cos (angle()) + 2,
				h + 2
				);

		}

	}
}

void
PortMatrixColumnLabels::button_press (double x, double y, int b, uint32_t t)
{
	uint32_t cx = 0;
	uint32_t const gh = _highest_group_name + 2 * name_pad();

	bool group_name = false;
	if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {
		if (y > (_height - gh)) {
			group_name = true;
			cx = x;
		} else {
			cx = x - (_height - gh - y) * tan (angle ());
		}
	} else {
		if (y < gh) {
			group_name = true;
			cx = x - _overhang;
		} else {
			cx = x - (_height - y) * tan (angle ());
		}
	}

	pair<boost::shared_ptr<PortGroup>, ARDOUR::BundleChannel> gc = position_to_group_and_channel (cx / grid_spacing(), _matrix->columns());

	if (b == 1) {

		if (group_name && gc.first) {
			gc.first->set_visible (!gc.first->visible ());
		} else if (gc.second.bundle) {
			_body->highlight_associated_channels (_matrix->column_index(), gc.second);
		}

	} else if (b == 3) {

			_matrix->popup_menu (
				gc,
				make_pair (boost::shared_ptr<PortGroup> (), ARDOUR::BundleChannel ()),
				t
				);
	}
}

