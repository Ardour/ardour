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
#include <cairo/cairo.h>
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
	GdkPixmap* pm = gdk_pixmap_new (NULL, 1, 1, 24);
	gdk_drawable_set_colormap (pm, gdk_colormap_get_system());
	cairo_t* cr = gdk_cairo_create (pm);

	_longest_port_name = 0;
	_longest_bundle_name = 0;
	_height = 0;
	_highest_group_name = 0;

	for (PortGroupList::List::const_iterator i = _matrix->rows()->begin(); i != _matrix->rows()->end(); ++i) {

		PortGroup::BundleList const r = (*i)->bundles ();
		for (PortGroup::BundleList::const_iterator j = r.begin(); j != r.end(); ++j) {

			for (uint32_t k = 0; k < j->bundle->nchannels(); ++k) {
				cairo_text_extents_t ext;
				cairo_text_extents (cr, j->bundle->channel_name(k).c_str(), &ext);
				if (ext.width > _longest_port_name) {
					_longest_port_name = ext.width;
				}
			}

			cairo_text_extents_t ext;
			cairo_text_extents (cr, j->bundle->name().c_str(), &ext);
			if (ext.width > _longest_bundle_name) {
				_longest_bundle_name = ext.width;
			}
		}

		_height += group_size (*i) * grid_spacing ();

		cairo_text_extents_t ext;
		cairo_text_extents (cr, (*i)->name.c_str(), &ext);
		if (ext.height > _highest_group_name) {
			_highest_group_name = ext.height;
		}
	}

	cairo_destroy (cr);
	gdk_pixmap_unref (pm);

	_width = _highest_group_name +
		_longest_bundle_name +
		name_pad() * 4;

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
	for (PortGroupList::List::const_iterator i = _matrix->rows()->begin(); i != _matrix->rows()->end(); ++i) {

		if ((*i)->visible ()) {

			PortGroup::BundleList const & bundles = (*i)->bundles ();
			for (PortGroup::BundleList::const_iterator j = bundles.begin(); j != bundles.end(); ++j) {
				render_bundle_name (cr, background_colour (), j->has_colour ? j->colour : get_a_bundle_colour (N), 0, y, j->bundle);

				if (!_matrix->show_only_bundles()) {
					for (uint32_t k = 0; k < j->bundle->nchannels(); ++k) {
						Gdk::Color c = j->has_colour ? j->colour : get_a_bundle_colour (M);
						render_channel_name (cr, background_colour (), c, 0, y, ARDOUR::BundleChannel (j->bundle, k));
						y += grid_spacing();
						++M;
					}
				} else {
					y += grid_spacing();
				}

				++N;
			}

		} else {

			y += grid_spacing ();
		}
	}
}

pair<boost::shared_ptr<PortGroup>, ARDOUR::BundleChannel>
PortMatrixRowLabels::position_to_group_and_channel (double p, double o, PortGroupList const * groups) const
{
	pair<boost::shared_ptr<PortGroup>, ARDOUR::BundleChannel> w = PortMatrixComponent::position_to_group_and_channel (p, o, _matrix->rows());

	uint32_t const gw = (_highest_group_name + 2 * name_pad());

	if (
		(_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM && o < gw) ||
		(_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT && o > (_width - gw))
		) {

		w.second.bundle.reset ();
	}

	return w;
}

void
PortMatrixRowLabels::button_press (double x, double y, int b, uint32_t t)
{
	pair<boost::shared_ptr<PortGroup>, ARDOUR::BundleChannel> w = position_to_group_and_channel (y, x, _matrix->rows());

	if (b == 3) {

		_matrix->popup_menu (
			make_pair (boost::shared_ptr<PortGroup> (), ARDOUR::BundleChannel ()),
			w,
			t
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

	if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {
		x = _highest_group_name + 2 * name_pad();
	} else {
		if (_matrix->show_only_bundles()) {
			x = 0;
		} else {
			x = _longest_port_name + name_pad() * 2;
		}
	}

	return x;
}

double
PortMatrixRowLabels::port_name_x () const
{
	if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {
		return _longest_bundle_name + _highest_group_name + name_pad() * 4;
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

	int const n = _matrix->show_only_bundles() ? 1 : b->nchannels();
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

	cairo_text_extents_t ext;
	cairo_text_extents (cr, bc.bundle->channel_name(bc.channel).c_str(), &ext);
	double const off = (grid_spacing() - ext.height) / 2;

	if (bc.bundle->nchannels() > 1) {

		/* only plot the name if the bundle has more than one channel;
		   the name of a single channel is assumed to be redundant */
		
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
	return channel_to_position (bc, _matrix->rows()) * grid_spacing ();
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
		
		if (c.bundle && r.bundle) {
			add_channel_highlight (r);
		} else if (r.bundle) {
			_body->highlight_associated_channels (_matrix->row_index(), r);
		}
	}
}

void
PortMatrixRowLabels::draw_extra (cairo_t* cr)
{
	PortMatrixLabels::draw_extra (cr);
	
	/* PORT GROUP NAMES */

	double x = 0;
	if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {
		x = component_to_parent_x (0);
	} else {
		x = component_to_parent_x (_width - _highest_group_name - 2 * name_pad());
	}

	double y = component_to_parent_y (0);
	int g = 0;
	for (PortGroupList::List::const_iterator i = _matrix->rows()->begin(); i != _matrix->rows()->end(); ++i) {

		/* compute height of this group */
		double h = 0;
		if (!(*i)->visible()) {
			h = grid_spacing ();
		} else {
			if (_matrix->show_only_bundles()) {
				h = (*i)->bundles().size() * grid_spacing();
			} else {
				h = (*i)->total_channels () * grid_spacing();
			}
		}

		if (h == 0) {
			continue;
		}

		/* rectangle */
		set_source_rgb (cr, get_a_group_colour (g));
		double const rw = _highest_group_name + 2 * name_pad();
		cairo_rectangle (cr, x, y, rw, h);
		cairo_fill (cr);

		/* y area available to draw the label in (trying to keep it visible) */
		double const ty = max (y, 0.0);
		double const by = min (y + h, double (_body->alloc_scroll_height ()));

		/* hence what abbreviation (or not) we need for the group name */
		string const upper = Glib::ustring ((*i)->name).uppercase ();
		pair<string, double> display = fit_to_pixels (cr, upper, by - ty);

		/* plot it */
		set_source_rgb (cr, text_colour());
		cairo_move_to (cr, x + rw - name_pad(), (by + ty + display.second) / 2);
		cairo_save (cr);
		cairo_rotate (cr, - M_PI / 2);
		cairo_show_text (cr, display.first.c_str());
		cairo_restore (cr);

		y += h;
		++g;
	}
}

void
PortMatrixRowLabels::motion (double x, double y)
{
	pair<boost::shared_ptr<PortGroup>, ARDOUR::BundleChannel> const w = position_to_group_and_channel (y, x, _matrix->rows());

	if (w.second.bundle == 0) {
		/* not over any bundle */
		_body->set_mouseover (PortMatrixNode ());
		return;
	}

	uint32_t const bw = _highest_group_name + 2 * name_pad() + _longest_bundle_name + 2 * name_pad();

	if (
		(_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM && x < bw) ||
		(_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT && x > (_width - bw))
		 
			) {

		/* if the mouse is over a bundle name, highlight all channels in the bundle */
		
		list<PortMatrixNode> n;

		for (uint32_t i = 0; i < w.second.bundle->nchannels(); ++i) {
			ARDOUR::BundleChannel const bc (w.second.bundle, i);
			n.push_back (PortMatrixNode (bc, ARDOUR::BundleChannel ()));
		}

		_body->set_mouseover (n);

	} else {
	
		_body->set_mouseover (PortMatrixNode (w.second, ARDOUR::BundleChannel ()));

	}
}
