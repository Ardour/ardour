/*
 * Copyright (C) 2009 Carl Hetherington <carl@carlh.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ardour/bundle.h"
#include "port_matrix_labels.h"
#include "port_matrix.h"

void
PortMatrixLabels::draw_extra (cairo_t* cr)
{
	for (std::vector<ARDOUR::BundleChannel>::const_iterator i = _channel_highlights.begin(); i != _channel_highlights.end(); ++i) {
		if (_matrix->show_only_bundles()) {
			render_bundle_name (
				cr,
				mouseover_line_colour(),
				highlighted_channel_colour(),
				component_to_parent_x (channel_x (*i)),
				component_to_parent_y (channel_y (*i)),
				i->bundle
				);
		} else {
			render_channel_name (
				cr,
				mouseover_line_colour(),
				highlighted_channel_colour(),
				component_to_parent_x (channel_x (*i)),
				component_to_parent_y (channel_y (*i)),
				*i
				);
		}
	}
}

void
PortMatrixLabels::clear_channel_highlights ()
{
	for (std::vector<ARDOUR::BundleChannel>::const_iterator i = _channel_highlights.begin(); i != _channel_highlights.end(); ++i) {
		queue_draw_for (*i);
	}

	_channel_highlights.clear ();
}

void
PortMatrixLabels::add_channel_highlight (ARDOUR::BundleChannel const & bc)
{
	_channel_highlights.push_back (bc);
	queue_draw_for (bc);
}

