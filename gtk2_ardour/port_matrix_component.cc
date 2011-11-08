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
 *  @param m Port matrix that we're in.
 *  @param b Port matrix body that we're in.
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
		g_object_unref (_pixmap);
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
			g_object_unref (_pixmap);
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

/** @param g Group.
 *  @return Visible size of the group in grid units, taking visibility and show_only_bundles into account.
 */
uint32_t
PortMatrixComponent::group_size (boost::shared_ptr<const PortGroup> g) const
{
	uint32_t s = 0;

	PortGroup::BundleList const & bundles = g->bundles ();
	if (_matrix->show_only_bundles()) {
		s = bundles.size();
	} else {
		for (PortGroup::BundleList::const_iterator i = bundles.begin(); i != bundles.end(); ++i) {
			s += _matrix->count_of_our_type_min_1 ((*i)->bundle->nchannels());
		}
	}

	return s;
}

/** @param bc Channel.
 *  @param groups List of groups.
 *  @return Position of bc in groups in grid units, taking show_only_bundles into account.
 */
uint32_t
PortMatrixComponent::channel_to_position (ARDOUR::BundleChannel bc, boost::shared_ptr<const PortGroup> group) const
{
	uint32_t p = 0;

	PortGroup::BundleList const & bundles = group->bundles ();

	for (PortGroup::BundleList::const_iterator i = bundles.begin(); i != bundles.end(); ++i) {

		if ((*i)->bundle == bc.bundle) {

			/* found the bundle */

			if (_matrix->show_only_bundles()) {
				return p;
			} else {
				return p + bc.channel;
			}

		}

		/* move past this bundle */

		if (_matrix->show_only_bundles()) {
			p += 1;
		} else {
			p += _matrix->count_of_our_type_min_1 ((*i)->bundle->nchannels());
		}
	}

	return 0;
}


ARDOUR::BundleChannel
PortMatrixComponent::position_to_channel (double p, double, boost::shared_ptr<const PortGroup> group) const
{
	p /= grid_spacing ();

	PortGroup::BundleList const & bundles = group->bundles ();
	for (PortGroup::BundleList::const_iterator j = bundles.begin(); j != bundles.end(); ++j) {

		if (_matrix->show_only_bundles()) {

			if (p < 1) {
				return ARDOUR::BundleChannel ((*j)->bundle, -1);
			} else {
				p -= 1;
			}

		} else {

			uint32_t const s = _matrix->count_of_our_type_min_1 ((*j)->bundle->nchannels());
			if (p < s) {
				return ARDOUR::BundleChannel ((*j)->bundle, (*j)->bundle->type_channel_to_overall (_matrix->type (), p));
			} else {
				p -= s;
			}

		}

	}

	return ARDOUR::BundleChannel (boost::shared_ptr<ARDOUR::Bundle> (), -1);
}
