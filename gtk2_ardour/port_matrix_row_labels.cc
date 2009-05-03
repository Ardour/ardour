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
	ARDOUR::BundleList const r = _matrix->rows()->bundles();
	for (ARDOUR::BundleList::const_iterator i = r.begin(); i != r.end(); ++i) {
		for (uint32_t j = 0; j < (*i)->nchannels(); ++j) {
			cairo_text_extents_t ext;
			cairo_text_extents (cr, (*i)->channel_name(j).c_str(), &ext);
			if (ext.width > _longest_port_name) {
				_longest_port_name = ext.width;
			}
		}

		cairo_text_extents_t ext;
		cairo_text_extents (cr, (*i)->name().c_str(), &ext);
		if (ext.width > _longest_bundle_name) {
			_longest_bundle_name = ext.width;
		}

		if (_matrix->show_only_bundles()) {
			_height += row_height ();
		} else {
			_height += (*i)->nchannels() * row_height();
		}
	}

	_highest_group_name = 0;
	for (PortGroupList::List::const_iterator i = _matrix->rows()->begin(); i != _matrix->rows()->end(); ++i) {
		if ((*i)->visible()) {
			cairo_text_extents_t ext;
			cairo_text_extents (cr, (*i)->name.c_str(), &ext);
			if (ext.height > _highest_group_name) {
				_highest_group_name = ext.height;
			}
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

	/* PORT GROUP NAMES */

	double x = 0;
	if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {
		x = 0;
	} else {
		x = _width - _highest_group_name - 2 * name_pad();
	}

	double y = 0;
	int g = 0;
	for (PortGroupList::List::const_iterator i = _matrix->rows()->begin(); i != _matrix->rows()->end(); ++i) {

		if (!(*i)->visible() || (*i)->bundles().empty()) {
			continue;
		}
			
		/* compute height of this group */
		double h = 0;
		if (_matrix->show_only_bundles()) {
			h = (*i)->bundles().size() * row_height();
		} else {
			h = (*i)->total_channels () * row_height();
		}

		/* rectangle */
		set_source_rgb (cr, get_a_group_colour (g));
		double const rw = _highest_group_name + 2 * name_pad();
		cairo_rectangle (cr, x, y, rw, h);
		cairo_fill (cr);
		    
		/* hence what abbreviation (or not) we need for the group name */
		std::pair<std::string, double> display = display_port_name (cr, (*i)->name, h);

		/* plot it */
		set_source_rgb (cr, text_colour());
		cairo_move_to (cr, x + rw - name_pad(), y + (h + display.second) / 2);
		cairo_save (cr);
		cairo_rotate (cr, - M_PI / 2);
		cairo_show_text (cr, display.first.c_str());
		cairo_restore (cr);

		y += h;
		++g;
	}

	/* BUNDLE NAMES */

	y = 0;
	ARDOUR::BundleList const r = _matrix->rows()->bundles();
	for (ARDOUR::BundleList::const_iterator i = r.begin(); i != r.end(); ++i) {
		render_bundle_name (cr, get_a_bundle_colour (i - r.begin ()), 0, y, *i);
		int const n = _matrix->show_only_bundles() ? 1 : (*i)->nchannels();
		y += row_height() * n;
	}
	

	/* PORT NAMES */

        if (!_matrix->show_only_bundles()) {
		y = 0;
		for (ARDOUR::BundleList::const_iterator i = r.begin(); i != r.end(); ++i) {
			for (uint32_t j = 0; j < (*i)->nchannels(); ++j) {
				render_channel_name (cr, get_a_bundle_colour (i - r.begin()), 0, y, ARDOUR::BundleChannel (*i, j));
				y += row_height();
			}
		}
	}
}

void
PortMatrixRowLabels::button_press (double x, double y, int b, uint32_t t)
{
	switch (b) {
	case 1:
		_body->highlight_associated_channels (_matrix->row_index(), y / row_height ());
		break;
	case 3:
		maybe_popup_context_menu (x, y, t);
		break;
	}
}

void
PortMatrixRowLabels::maybe_popup_context_menu (double x, double y, uint32_t t)
{
	if ( (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM && x > (_longest_bundle_name + name_pad() * 2)) ||
	     (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT && x < (_longest_port_name + name_pad() * 2))
		) {

		_matrix->popup_channel_context_menu (_matrix->row_index(), y / row_height(), t);
		
	}
}

double
PortMatrixRowLabels::component_to_parent_x (double x) const
{
	return x + _parent_rectangle.get_x();
}

double
PortMatrixRowLabels::parent_to_component_x (double x) const
{
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
	cairo_t* cr, Gdk::Color colour, double xoff, double yoff, boost::shared_ptr<ARDOUR::Bundle> b
	)
{
	double const x = bundle_name_x ();
	
	int const n = _matrix->show_only_bundles() ? 1 : b->nchannels();
	set_source_rgb (cr, colour);
	cairo_rectangle (cr, xoff + x, yoff, _longest_bundle_name + name_pad() * 2, row_height() * n);
	cairo_fill_preserve (cr);
	set_source_rgb (cr, background_colour());
	cairo_set_line_width (cr, label_border_width ());
	cairo_stroke (cr);

	double const off = row_height() / 2;

// 	if ((*i)->nchannels () > 0 && !_matrix->show_only_bundles()) {
// 		/* use the extent of our first channel name so that the bundle name is vertically aligned with it */
// 		cairo_text_extents_t ext;
// 		cairo_text_extents (cr, (*i)->channel_name(0).c_str(), &ext);
// 		off = (row_height() - ext.height) / 2;
// 	}

 	set_source_rgb (cr, text_colour());
 	cairo_move_to (cr, xoff + x + name_pad(), yoff + name_pad() + off);
 	cairo_show_text (cr, b->name().c_str());
}

void
PortMatrixRowLabels::render_channel_name (
	cairo_t* cr, Gdk::Color colour, double xoff, double yoff, ARDOUR::BundleChannel const& bc
	)
{
	set_source_rgb (cr, colour);
	cairo_rectangle (cr, port_name_x() + xoff, yoff, _longest_port_name + name_pad() * 2, row_height());
	cairo_fill_preserve (cr);
	set_source_rgb (cr, background_colour());
	cairo_set_line_width (cr, label_border_width ());
	cairo_stroke (cr);
	
	cairo_text_extents_t ext;
	cairo_text_extents (cr, bc.bundle->channel_name(bc.channel).c_str(), &ext);
	double const off = (row_height() - ext.height) / 2;
	
	set_source_rgb (cr, text_colour());
	cairo_move_to (cr, port_name_x() + xoff + name_pad(), yoff + name_pad() + off);
	cairo_show_text (cr, bc.bundle->channel_name(bc.channel).c_str());
}

double
PortMatrixRowLabels::channel_x (ARDOUR::BundleChannel const& bc) const
{
	return 0;
}

double
PortMatrixRowLabels::channel_y (ARDOUR::BundleChannel const& bc) const
{
	uint32_t n = 0;

	ARDOUR::BundleList::const_iterator i = _matrix->rows()->bundles().begin();
	while (i != _matrix->rows()->bundles().end() && *i != bc.bundle) {
		if (_matrix->show_only_bundles()) {
			n += 1;
		} else {
			n += (*i)->nchannels ();
		}
		++i;
	}

	if (!_matrix->show_only_bundles()) {
		n += bc.channel;
	}
	
	return n * row_height();
}

void
PortMatrixRowLabels::queue_draw_for (ARDOUR::BundleChannel const & bc)
{
	if (bc.bundle) {

		if (_matrix->show_only_bundles()) {
			_body->queue_draw_area (
				component_to_parent_x (bundle_name_x()),
				component_to_parent_y (channel_y (bc)),
				_longest_bundle_name + name_pad() * 2,
				row_height()
				);
		} else {
			_body->queue_draw_area (
				component_to_parent_x (port_name_x()),
				component_to_parent_y (channel_y (bc)),
				_longest_port_name + name_pad() * 2,
				row_height()
				);
		}
	}

}

void
PortMatrixRowLabels::mouseover_changed (PortMatrixNode const &)
{
	clear_channel_highlights ();
	if (_body->mouseover().row.bundle) {
		add_channel_highlight (_body->mouseover().row);
	}
}
