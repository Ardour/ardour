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

#include "port_matrix_component.h"
#include "port_matrix.h"
#include "port_matrix_body.h"

using namespace std;

/** Constructor.
 *  @param p Port matrix that we're in.
 */
PortMatrixComponent::PortMatrixComponent (PortMatrix* m, PortMatrixBody* b)
	: _matrix (m),
	  _body (b),
	  _pixmap (0),
	  _render_required (true),
	  _dimension_computation_required (true)
{
	
}

/** Destructor */
PortMatrixComponent::~PortMatrixComponent ()
{
	if (_pixmap) {
		gdk_pixmap_unref (_pixmap);
	}
}

void
PortMatrixComponent::setup ()
{
	_dimension_computation_required = true;
	_render_required = true;
}

GdkPixmap *
PortMatrixComponent::get_pixmap (GdkDrawable *drawable)
{
	if (_render_required) {

		if (_dimension_computation_required) {
			compute_dimensions ();
			_dimension_computation_required = false;
			_body->component_size_changed ();
		}

		/* we may be zero width or height; if so, just
		   use the smallest allowable pixmap */
		if (_width == 0) {
			_width = 1;
		}
		if (_height == 0) {
			_height = 1;
		}

		/* make a pixmap of the right size */
		if (_pixmap) {
			gdk_pixmap_unref (_pixmap);
		}
		_pixmap = gdk_pixmap_new (drawable, _width, _height, -1);

		/* render */
		cairo_t* cr = gdk_cairo_create (_pixmap);
		render (cr);
		cairo_destroy (cr);

		_render_required = false;
	}

	return _pixmap;
}

void
PortMatrixComponent::set_source_rgb (cairo_t *cr, Gdk::Color const & c)
{
	cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
}

void
PortMatrixComponent::set_source_rgba (cairo_t *cr, Gdk::Color const & c, double a)
{
	cairo_set_source_rgba (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p(), a);
}

pair<uint32_t, uint32_t>
PortMatrixComponent::dimensions ()
{
	if (_dimension_computation_required) {
		compute_dimensions ();
		_dimension_computation_required = false;
		_body->component_size_changed ();
	}

	return make_pair (_width, _height);
}

Gdk::Color
PortMatrixComponent::background_colour ()
{
	return _matrix->get_style()->get_bg (Gtk::STATE_NORMAL);
}

uint32_t
PortMatrixComponent::group_width (boost::shared_ptr<const PortGroup> g) const
{
	uint32_t width = 0;
	
	if (g->visible()) {
		PortGroup::BundleList const & bundles = g->bundles ();
		if (_matrix->show_only_bundles()) {
			width = bundles.size() * column_width ();
		} else {
			for (PortGroup::BundleList::const_iterator i = bundles.begin(); i != bundles.end(); ++i) {
				width += i->bundle->nchannels() * column_width ();
			}
		}
	} else {
		width = column_width ();
	}

	return width;
}

uint32_t
PortMatrixComponent::group_height (boost::shared_ptr<const PortGroup> g) const
{
	uint32_t height = 0;

	if (g->visible ()) {
		PortGroup::BundleList const & bundles = g->bundles ();
		if (_matrix->show_only_bundles()) {
			height = bundles.size() * row_height ();
		} else {
			for (PortGroup::BundleList::const_iterator i = bundles.begin(); i != bundles.end(); ++i) {
				height += i->bundle->nchannels() * row_height ();
			}
		}
	} else {
		height = row_height ();
	}

	return height;
}


pair<boost::shared_ptr<PortGroup>, ARDOUR::BundleChannel>
PortMatrixComponent::y_position_to_group_and_channel (double y) const
{
	PortGroupList::List::const_iterator i = _matrix->rows()->begin();

	while (i != _matrix->rows()->end()) {

		uint32_t const gh = group_height (*i);

		if (y < gh) {

			/* it's in this group */

			PortGroup::BundleList const & bundles = (*i)->bundles ();
			for (PortGroup::BundleList::const_iterator j = bundles.begin(); j != bundles.end(); ++j) {

				if (_matrix->show_only_bundles()) {
					
					if (y < row_height()) {
						return make_pair (*i, ARDOUR::BundleChannel (j->bundle, 0));
					} else {
						y -= row_height ();
					}
					
				} else {

					uint32_t const h = j->bundle->nchannels () * row_height ();
					if (y < h) {
						return make_pair (*i, ARDOUR::BundleChannel (j->bundle, y / row_height()));
					} else {
					        y -= h;
					}

				}

			}

		} else {

			y -= gh;

		}

		++i;
	}

	return make_pair (boost::shared_ptr<PortGroup> (), ARDOUR::BundleChannel (boost::shared_ptr<ARDOUR::Bundle> (), 0));
}
