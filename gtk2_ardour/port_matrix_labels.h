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

#ifndef __port_matrix_labels_h__
#define __port_matrix_labels_h__

#include "port_matrix_component.h"

namespace ARDOUR {
	class BundleChannel;
}

class PortMatrixLabels : public PortMatrixComponent
{
public:
	PortMatrixLabels (PortMatrix* m, PortMatrixBody* b) : PortMatrixComponent (m, b) {}
	virtual ~PortMatrixLabels () {}

	void draw_extra (cairo_t *);

	void clear_channel_highlights ();
	void add_channel_highlight (ARDOUR::BundleChannel const &);

private:
	virtual void render_channel_name (cairo_t *, Gdk::Color, Gdk::Color, double, double, ARDOUR::BundleChannel const &) = 0;
	virtual void render_bundle_name (cairo_t *, Gdk::Color, Gdk::Color, double, double, boost::shared_ptr<ARDOUR::Bundle>) = 0;
	virtual double channel_x (ARDOUR::BundleChannel const &) const = 0;
	virtual double channel_y (ARDOUR::BundleChannel const &) const = 0;
	virtual void queue_draw_for (ARDOUR::BundleChannel const &) = 0;

	std::vector<ARDOUR::BundleChannel> _channel_highlights;
};

#endif
