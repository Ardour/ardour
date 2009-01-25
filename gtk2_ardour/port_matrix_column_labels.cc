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

PortMatrixColumnLabels::PortMatrixColumnLabels (PortMatrixBody* b, Location l)
	: PortMatrixComponent (b), _location (l)
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

	ARDOUR::BundleList const c = _body->column_ports().bundles();
	for (ARDOUR::BundleList::const_iterator i = c.begin (); i != c.end(); ++i) {

		cairo_text_extents_t ext;
		cairo_text_extents (cr, (*i)->name().c_str(), &ext);
		if (ext.width > _longest_bundle_name) {
			_longest_bundle_name = ext.width;
		}
		if (ext.height > _highest_text) {
			_highest_text = ext.height;
		}

		for (uint32_t j = 0; j < (*i)->nchannels (); ++j) {
			
			cairo_text_extents (
				cr,
				(*i)->channel_name (j).c_str(),
				&ext
				);
			
			if (ext.width > _longest_channel_name) {
				_longest_channel_name = ext.width;
			}
			if (ext.height > _highest_text) {
				_highest_text = ext.height;
			}
		}

		_width += (*i)->nchannels() * column_width();
	}

	_highest_group_name = 0;
	for (PortGroupList::const_iterator i = _body->column_ports().begin(); i != _body->column_ports().end(); ++i) {
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

	/* height of the whole thing */

	double const parallelogram_height = 
		(_longest_bundle_name + _longest_channel_name + 4 * name_pad()) * sin (angle())
		+ _highest_text * cos (angle());

	_height = parallelogram_height + _highest_group_name + 2 * name_pad();

	_width += parallelogram_height / tan (angle ());
}

double
PortMatrixColumnLabels::basic_text_x_pos (int c) const
{
	return column_width() / 2 +
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
	
	if (_location == TOP) {
		x = (_height - _highest_group_name - 2 * name_pad()) / tan (angle());
		y = _highest_group_name + name_pad();
	} else {
		x = 0;
		y = _height - name_pad();
	}

	int g = 0;
	for (PortGroupList::const_iterator i = _body->column_ports().begin(); i != _body->column_ports().end(); ++i) {

		if (!(*i)->visible() || ((*i)->bundles.empty() && (*i)->ports.empty()) ) {
			continue;
		}

		/* compute width of this group */
		uint32_t w = 0;
		for (ARDOUR::BundleList::const_iterator j = (*i)->bundles.begin(); j != (*i)->bundles.end(); ++j) {
			w += (*j)->nchannels() * column_width();
		}
		w += (*i)->ports.size() * column_width();

		/* rectangle */
		set_source_rgb (cr, get_a_group_colour (g));
		double const rh = _highest_group_name + 2 * name_pad();
		if (_location == TOP) {
			cairo_rectangle (cr, x, 0, w, rh);
		} else if (_location == BOTTOM) {
			cairo_rectangle (cr, x, _height - rh, w, rh);
		}
		cairo_fill (cr);
		
		std::pair<std::string, double> const display = display_port_name (cr, (*i)->name, w);

		/* plot it */
		set_source_rgb (cr, text_colour());
		cairo_move_to (cr, x + (w - display.second) / 2, y);
		cairo_show_text (cr, display.first.c_str());

		x += w;
		++g;
	}

        /* BUNDLE PARALLELOGRAM-TYPE-THING AND NAME */

	x = 0;
	ARDOUR::BundleList const c = _body->column_ports().bundles();
	for (ARDOUR::BundleList::const_iterator i = c.begin (); i != c.end(); ++i) {

		Gdk::Color colour = get_a_bundle_colour (i - c.begin ());
		set_source_rgb (cr, colour);

		double const w = (*i)->nchannels() * column_width();
		double const ph = _height - _highest_group_name - 2 * name_pad();

		double x_ = x;
		
		if (_location == TOP) {
			y = _height;
		} else if (_location == BOTTOM) {
			y = ph;
		}

		double y_ = y;
 		cairo_move_to (cr, x_, y_);
 		x_ += w;
 		cairo_line_to (cr, x_, y_);
 		x_ += ph / tan (angle ());
 		y_ -= ph;
 		cairo_line_to (cr, x_, y_);
 		x_ -= w;
 		cairo_line_to (cr, x_, y_);
 		cairo_line_to (cr, x, y);
		cairo_fill_preserve (cr);
		set_source_rgb (cr, background_colour());
		cairo_set_line_width (cr, label_border_width());
		cairo_stroke (cr);

		set_source_rgb (cr, text_colour());

		if (_location == TOP) {
			
			double const rl = 3 * name_pad() + _longest_channel_name;
			cairo_move_to (
				cr,
				x + basic_text_x_pos (0) + rl * cos (angle()),
				_height - rl * sin (angle())
				);
			
		} else if (_location == BOTTOM) {

			cairo_move_to (
				cr,
				x + basic_text_x_pos (0),
				ph - name_pad() * sin (angle())
				);
		}
			
		cairo_save (cr);
		cairo_rotate (cr, -angle());
		cairo_show_text (cr, (*i)->name().c_str());
		cairo_restore (cr);
		
		x += (*i)->nchannels () * column_width();
	}
	

	/* PORT NAMES */

	x = 0;
	for (ARDOUR::BundleList::const_iterator i = c.begin (); i != c.end(); ++i) {
		
		for (uint32_t j = 0; j < (*i)->nchannels(); ++j) {

			double const lc = _longest_channel_name + name_pad();
			double const w = column_width();
			double const ph = _height - _highest_group_name - 2 * name_pad();

			if (_location == BOTTOM) {

				double x_ = x + ph / tan (angle()) + w;
				double const ix = x_;
				double y_ = 0;
				cairo_move_to (cr, x_, y_);
				x_ -= w;
				cairo_line_to (cr, x_, y_);
				x_ -= lc * cos (angle());
				y_ += lc * sin (angle());
				cairo_line_to (cr, x_, y_);
				x_ += w * pow (sin (angle()), 2);
				y_ += w * sin (angle()) * cos (angle());
				cairo_line_to (cr, x_, y_);
				cairo_line_to (cr, ix, 0);

			} else if (_location == TOP) {

				double x_ = x;
				double y_ = _height;
				cairo_move_to (cr, x_, y_);
				x_ += w;
				cairo_line_to (cr, x_, y_);
				x_ += lc * cos (angle());
				y_ -= lc * sin (angle());
				cairo_line_to (cr, x_, y_);
				x_ -= column_width() * pow (sin (angle()), 2);
				y_ -= column_width() * sin (angle()) * cos (angle());
				cairo_line_to (cr, x_, y_);
				cairo_line_to (cr, x, _height);

			}

			Gdk::Color colour = get_a_bundle_colour (i - c.begin());
			set_source_rgb (cr, colour);
			cairo_fill_preserve (cr);
			set_source_rgb (cr, background_colour());
			cairo_set_line_width (cr, label_border_width());
			cairo_stroke (cr);

			set_source_rgb (cr, text_colour());

			if (_location == TOP) {
				cairo_move_to (cr, x + basic_text_x_pos(j), _height - name_pad() * sin (angle()));
			} else if (_location == BOTTOM) {
				double const rl = 3 * name_pad() + _longest_bundle_name;
				cairo_move_to (cr, x + basic_text_x_pos(j) + rl * cos (angle ()), ph - rl * sin (angle()));
			}
			
			cairo_save (cr);
			cairo_rotate (cr, -angle());

			cairo_show_text (
				cr,
				(*i)->channel_name(j).c_str()
				);
			
			cairo_restore (cr);

			x += column_width();
		}
	}
}

