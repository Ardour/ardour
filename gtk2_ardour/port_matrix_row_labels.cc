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
#include <gtkmm/menu.h>
#include <gtkmm/menushell.h>
#include <gtkmm/menu_elems.h>
#include <cairo/cairo.h>
#include "ardour/bundle.h"
#include "port_matrix_row_labels.h"
#include "port_matrix.h"
#include "i18n.h"

PortMatrixRowLabels::PortMatrixRowLabels (PortMatrix* p, PortMatrixBody* b, Location l)
	: PortMatrixComponent (b), _port_matrix (p), _menu (0), _location (l)
{
	
}

PortMatrixRowLabels::~PortMatrixRowLabels ()
{
	delete _menu;
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
	ARDOUR::BundleList const r = _body->row_ports().bundles();
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

		_height += (*i)->nchannels() * row_height();
	}

	_highest_group_name = 0;
	for (PortGroupList::const_iterator i = _body->row_ports().begin(); i != _body->row_ports().end(); ++i) {
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
		_longest_port_name +
		_longest_bundle_name +
		name_pad() * 6;
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
	if (_location == LEFT) {
		x = 0;
	} else if (_location == RIGHT) {
		x = _width - _highest_group_name - 2 * name_pad();
	}

	double y = 0;
	int g = 0;
	for (PortGroupList::const_iterator i = _body->row_ports().begin(); i != _body->row_ports().end(); ++i) {

		if (!(*i)->visible() || ((*i)->bundles().empty() && (*i)->ports.empty()) ) {
			continue;
		}
			
		/* compute height of this group */
		double h = 0;
		for (ARDOUR::BundleList::const_iterator j = (*i)->bundles().begin(); j != (*i)->bundles().end(); ++j) {
			h += (*j)->nchannels() * row_height();
		}
		h += (*i)->ports.size() * row_height();

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

	x = 0;
	if (_location == LEFT) {
		x = _highest_group_name + 2 * name_pad();
	} else if (_location == RIGHT) {
		x = _longest_port_name + name_pad() * 2;
	}

	y = 0;
	ARDOUR::BundleList const r = _body->row_ports().bundles();
	for (ARDOUR::BundleList::const_iterator i = r.begin(); i != r.end(); ++i) {

		Gdk::Color const colour = get_a_bundle_colour (i - r.begin ());
		set_source_rgb (cr, colour);
		cairo_rectangle (cr, x, y, _longest_bundle_name + name_pad() * 2, row_height() * (*i)->nchannels());
		cairo_fill_preserve (cr);
		set_source_rgb (cr, background_colour());
		cairo_set_line_width (cr, label_border_width ());
		cairo_stroke (cr);

		double off = 0;
		if ((*i)->nchannels () > 0) {
			/* use the extent of our first channel name so that the bundle name is vertically aligned with it */
			cairo_text_extents_t ext;
			cairo_text_extents (cr, (*i)->channel_name(0).c_str(), &ext);
			off = (row_height() - ext.height) / 2;
		} else {
			off = row_height() / 2;
		}

		set_source_rgb (cr, text_colour());
		cairo_move_to (cr, x + name_pad(), y + name_pad() + off);
		cairo_show_text (cr, (*i)->name().c_str());
		
		y += row_height() * (*i)->nchannels ();
	}
	

	/* PORT NAMES */

	y = 0;
	for (ARDOUR::BundleList::const_iterator i = r.begin(); i != r.end(); ++i) {
		for (uint32_t j = 0; j < (*i)->nchannels(); ++j) {
			render_port_name (cr, get_a_bundle_colour (i - r.begin()), 0, y, PortMatrixBundleChannel (*i, j));
			y += row_height();
		}
	}
}

void
PortMatrixRowLabels::button_press (double x, double y, int b, uint32_t t)
{
	if (b != 3) {
		return;
	}

	if ( (_location ==  LEFT && x > (_longest_bundle_name + name_pad() * 2)) ||
	     (_location == RIGHT && x < (_longest_port_name + name_pad() * 2))
		) {

		delete _menu;
		
		_menu = new Gtk::Menu;
		_menu->set_name ("ArdourContextMenu");
		
		Gtk::Menu_Helpers::MenuList& items = _menu->items ();
		
		uint32_t row = y / row_height ();

		boost::shared_ptr<ARDOUR::Bundle> bundle;
		uint32_t channel = 0;

		ARDOUR::BundleList const r = _body->row_ports().bundles();
		for (ARDOUR::BundleList::const_iterator i = r.begin(); i != r.end(); ++i) {
			if (row < (*i)->nchannels ()) {
				bundle = *i;
				channel = row;
				break;
			} else {
				row -= (*i)->nchannels ();
			}
		}

		if (bundle) {
			char buf [64];

			if (_port_matrix->can_rename_channels ()) {
				snprintf (buf, sizeof (buf), _("Rename '%s'..."), bundle->channel_name (channel).c_str());
				items.push_back (
					Gtk::Menu_Helpers::MenuElem (
						buf,
						sigc::bind (sigc::mem_fun (*this, &PortMatrixRowLabels::rename_channel_proxy), bundle, channel)
						)
					);
			}
			
			snprintf (buf, sizeof (buf), _("Remove '%s'"), bundle->channel_name (channel).c_str());
			items.push_back (
				Gtk::Menu_Helpers::MenuElem (
					buf,
					sigc::bind (sigc::mem_fun (*this, &PortMatrixRowLabels::remove_channel_proxy), bundle, channel)
					)
				);

			_menu->popup (1, t);
		}
	}
}


void
PortMatrixRowLabels::remove_channel_proxy (boost::weak_ptr<ARDOUR::Bundle> b, uint32_t c)
{
	boost::shared_ptr<ARDOUR::Bundle> sb = b.lock ();
	if (!sb) {
		return;
	}

	_port_matrix->remove_channel (sb, c);

}

void
PortMatrixRowLabels::rename_channel_proxy (boost::weak_ptr<ARDOUR::Bundle> b, uint32_t c)
{
	boost::shared_ptr<ARDOUR::Bundle> sb = b.lock ();
	if (!sb) {
		return;
	}

	_port_matrix->rename_channel (sb, c);
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
PortMatrixRowLabels::port_name_x () const
{
	if (_location == LEFT) {
		return _longest_bundle_name + _highest_group_name + name_pad() * 4;
	} else if (_location == RIGHT) {
		return 0;
	}

	return 0;
}

void
PortMatrixRowLabels::render_port_name (
	cairo_t* cr, Gdk::Color colour, double xoff, double yoff, PortMatrixBundleChannel const& bc
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
PortMatrixRowLabels::channel_y (PortMatrixBundleChannel const& bc) const
{
	return bc.nchannels (_body->row_ports().bundles()) * row_height();
}

void
PortMatrixRowLabels::queue_draw_for (PortMatrixNode const& n)
{
	if (n.row.bundle) {

		_body->queue_draw_area (
			component_to_parent_x (port_name_x()),
			component_to_parent_y (channel_y (n.row)),
			_longest_port_name + name_pad() * 2,
			row_height()
			);
	}

}

void
PortMatrixRowLabels::mouseover_changed (PortMatrixNode const& old)
{
	queue_draw_for (old);
	queue_draw_for (_body->mouseover());
}

void
PortMatrixRowLabels::draw_extra (cairo_t* cr)
{
	if (_body->mouseover().row.bundle) {
		render_port_name (
			cr,
			mouseover_port_colour (),
			component_to_parent_x (0),
			component_to_parent_y (channel_y (_body->mouseover().row)),
			_body->mouseover().row
			);
	}
}
