/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cmath>
#include <iostream>

#include "gtkmm2ext/utils.h"
#include "widgets/bracelet.h"

using namespace ArdourWidgets;

Bracelet::Bracelet (int steps)
	: _steps (steps)
	, width (0.)
	, fill_color (0xff0000ff)
	, outline_color (0x000000ff)
{
	active.assign (_steps, false);
}

Bracelet::~Bracelet ()
{
}

void
Bracelet::render (Cairo::RefPtr<Cairo::Context> const& ctxt, cairo_rectangle_t*)
{
	if (width == 0.) {
		return;
	}

	ctxt->translate (width/2., width/2.);

	float main_radius = width/2. - 30;
	float minor_radius = main_radius / 5.;

	Gtkmm2ext::set_source_rgba (ctxt->cobj(), outline_color);
	ctxt->set_line_width (1.0);
	ctxt->arc (0., 0., main_radius, 0, 2*M_PI);
	ctxt->stroke ();

	Gtkmm2ext::set_source_rgba (ctxt->cobj(), outline_color);

	for (int n = 0; n < _steps; ++n) {

		ctxt->arc (0., -main_radius, minor_radius, 0., 2. * M_PI);

		if (active[n]) {
			ctxt->fill ();
		} else {
			ctxt->save ();
			Gtkmm2ext::set_source_rgba (ctxt->cobj(), fill_color);
			ctxt->fill_preserve ();
			ctxt->restore ();
			ctxt->stroke ();
		}

		ctxt->rotate ((2*M_PI) / _steps);
	}
}

void
Bracelet::on_size_allocate (Gtk::Allocation &alloc)
{
	CairoWidget::on_size_allocate (alloc);

	width = std::min (alloc.get_width(), alloc.get_height());
}

void
Bracelet::fill (int step)
{
	if (step < _steps) {
		active[step] = true;
	}
	queue_draw ();
}

void
Bracelet::fill (std::vector<int> const & steps)
{
	for (auto s : steps) {
		if (s < _steps) {
			active[s] = true;
		}
	}

	queue_draw ();
}

void
Bracelet::clear ()
{
	for (int n = 0; n < _steps; ++n) {
		active[n] = false;
	}

	queue_draw ();
}

void
Bracelet::set_fill_color (int c)
{
	fill_color = c;
}

void
Bracelet::set_outline_color (int c)
{
	outline_color = c;
}
