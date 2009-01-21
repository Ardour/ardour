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
	for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::const_iterator i = _body->row_bundles().begin(); i != _body->row_bundles().end(); ++i) {
		for (uint32_t j = 0; j < (*i)->nchannels(); ++j) {
			cairo_text_extents_t ext;
			cairo_text_extents (cr, (*i)->channel_name(j).c_str(), &ext);
			if (ext.width > _longest_port_name) {
				_longest_port_name = ext.width;
			}
		}
	}

	_longest_bundle_name = 0;
	for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::const_iterator i = _body->row_bundles().begin(); i != _body->row_bundles().end(); ++i) {
		cairo_text_extents_t ext;
		cairo_text_extents (cr, (*i)->name().c_str(), &ext);
		if (ext.width > _longest_bundle_name) {
			_longest_bundle_name = ext.width;
		}
	}

	cairo_destroy (cr);
	gdk_pixmap_unref (pm);

	_height = 0;
	for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::const_iterator i = _body->row_bundles().begin(); i != _body->row_bundles().end(); ++i) {
		_height += (*i)->nchannels() * row_height();
	}

	_width = _longest_port_name + name_pad() * 4 + _longest_bundle_name;
}


void
PortMatrixRowLabels::render (cairo_t* cr)
{
	/* BACKGROUND */
	
	set_source_rgb (cr, background_colour());
	cairo_rectangle (cr, 0, 0, _width, _height);
	cairo_fill (cr);

	/* SIDE BUNDLE NAMES */

	uint32_t x = 0;
	if (_location == LEFT) {
		x = name_pad();
	} else if (_location == RIGHT) {
		x = _longest_port_name + name_pad() * 3;
	}

	uint32_t y = 0;
	for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::const_iterator i = _body->row_bundles().begin(); i != _body->row_bundles().end(); ++i) {

		Gdk::Color const colour = get_a_bundle_colour (i - _body->row_bundles().begin ());
		set_source_rgb (cr, colour);
		cairo_rectangle (cr, 0, y, _width, row_height() * (*i)->nchannels());
		cairo_fill_preserve (cr);
		set_source_rgb (cr, background_colour());
		cairo_set_line_width (cr, label_border_width ());
		cairo_stroke (cr);

		uint32_t off = 0;
		if ((*i)->nchannels () > 0) {
			/* use the extent of our first channel name so that the bundle name is vertically aligned with it */
			cairo_text_extents_t ext;
			cairo_text_extents (cr, (*i)->channel_name(0).c_str(), &ext);
			off = (row_height() - ext.height) / 2;
		} else {
			off = row_height() / 2;
		}

		set_source_rgb (cr, text_colour());
		cairo_move_to (cr, x, y + name_pad() + off);
		cairo_show_text (cr, (*i)->name().c_str());
		
		y += row_height() * (*i)->nchannels ();
	}
	

	/* SIDE PORT NAMES */

	y = 0;
	for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::const_iterator i = _body->row_bundles().begin(); i != _body->row_bundles().end(); ++i) {
		for (uint32_t j = 0; j < (*i)->nchannels(); ++j) {

			uint32_t x = 0;
			if (_location == LEFT) {
				x = _longest_bundle_name + name_pad() * 2;
			} else if (_location == RIGHT) {
				x = 0;
			}

			Gdk::Color const colour = get_a_bundle_colour (i - _body->row_bundles().begin ());
			set_source_rgb (cr, colour);
			cairo_rectangle (
				cr,
				x,
				y,
				_longest_port_name + (name_pad() * 2),
				row_height()
				);
			cairo_fill_preserve (cr);
			set_source_rgb (cr, background_colour());
			cairo_set_line_width (cr, label_border_width ());
			cairo_stroke (cr);

			cairo_text_extents_t ext;
			cairo_text_extents (cr, (*i)->channel_name(j).c_str(), &ext);
			uint32_t const off = (row_height() - ext.height) / 2;

			set_source_rgb (cr, text_colour());
			cairo_move_to (cr, x + name_pad(), y + name_pad() + off);
			cairo_show_text (cr, (*i)->channel_name(j).c_str());

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
		
		for (std::vector<boost::shared_ptr<ARDOUR::Bundle> >::const_iterator i = _body->row_bundles().begin(); i != _body->row_bundles().end(); ++i) {
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
