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

/** @param g Group.
 *  @return Visible size of the group in grid units, taking visibility and show_only_bundles into account.
 */
uint32_t
PortMatrixComponent::group_size (boost::shared_ptr<const PortGroup> g) const
{
	uint32_t s = 0;

	if (g->visible()) {
		PortGroup::BundleList const & bundles = g->bundles ();
		if (_matrix->show_only_bundles()) {
			s = bundles.size();
		} else {
			for (PortGroup::BundleList::const_iterator i = bundles.begin(); i != bundles.end(); ++i) {
				s += i->bundle->nchannels();
			}
		}
	} else {
		s = 1;
	}

	return s;
}

/** @param bc Channel.
 *  @param groups List of groups.
 *  @return Position of bc in groups in grid units, taking visibility and show_only_bundles into account.
 */
uint32_t
PortMatrixComponent::channel_to_position (ARDOUR::BundleChannel bc, PortGroupList const * groups) const
{
	uint32_t p = 0;

	for (PortGroupList::List::const_iterator i = groups->begin(); i != groups->end(); ++i) {

		PortGroup::BundleList const & bundles = (*i)->bundles ();

		for (PortGroup::BundleList::const_iterator j = bundles.begin(); j != bundles.end(); ++j) {

			if (j->bundle == bc.bundle) {

				/* found the bundle */

				if (_matrix->show_only_bundles() || !(*i)->visible()) {
					return p;
				} else {
					return p + bc.channel;
				}

			}

			if ((*i)->visible()) {

				/* move past this bundle */

				if (_matrix->show_only_bundles()) {
					p += 1;
				} else {
					p += j->bundle->nchannels ();
				}
			}
		}

		if (!(*i)->visible()) {
			/* if this group isn't visible we won't have updated p, so do it now */
			p += 1;
		}
	}

	return 0;
}


pair<boost::shared_ptr<PortGroup>, ARDOUR::BundleChannel>
PortMatrixComponent::position_to_group_and_channel (uint32_t p, PortGroupList const * groups) const
{
	PortGroupList::List::const_iterator i = groups->begin ();

	while (i != groups->end()) {

		uint32_t const gs = group_size (*i);

		if (p < gs) {

			/* it's in this group */

			if (!(*i)->visible()) {
				return make_pair (*i, ARDOUR::BundleChannel (boost::shared_ptr<ARDOUR::Bundle> (), 0));
			}

			PortGroup::BundleList const & bundles = (*i)->bundles ();
			for (PortGroup::BundleList::const_iterator j = bundles.begin(); j != bundles.end(); ++j) {

				if (_matrix->show_only_bundles()) {

					if (p == 0) {
						return make_pair (*i, ARDOUR::BundleChannel (j->bundle, 0));
					} else {
						p -= 1;
					}

				} else {

					uint32_t const s = j->bundle->nchannels ();
					if (p < s) {
						return make_pair (*i, ARDOUR::BundleChannel (j->bundle, p));
					} else {
					        p -= s;
					}

				}

			}

		} else {

			p -= gs;

		}

		++i;
	}

	return make_pair (boost::shared_ptr<PortGroup> (), ARDOUR::BundleChannel (boost::shared_ptr<ARDOUR::Bundle> (), 0));
}
