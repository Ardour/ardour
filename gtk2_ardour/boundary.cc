/*
 * Copyright (C) 2025 Paul Davis <paul@linuxaudiosystems.com>
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

#include "boundary.h"
#include "ui_config.h"

void
StartBoundaryRect::render  (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rectangle::render (area, context);

	ArdourCanvas::Rect self (item_to_window (_rect));
	const double scale = UIConfiguration::instance().get_ui_scale();
	const double radius = 10. * scale;

	context->arc (self.x1, (self.y0 + (self.height() / 2.)) - radius, radius, -(M_PI/2.), (M_PI/2.));
	Gtkmm2ext::set_source_rgba (context, _outline_color);
	context->fill ();

}

bool
StartBoundaryRect::covers (ArdourCanvas::Duple const & point) const
{
	ArdourCanvas::Rect self (item_to_window (_rect));
	const double scale = UIConfiguration::instance().get_ui_scale();

	if ((point.x < self.x0) || (fabs ((point.x - self.x1) < (20. * scale)))) {
		/* before the start, or within 20 (scaled) pixels of the boundary, on the right */
		return true;
	}

	/* Approximate the semicircle handle with a square */

	const double radius = 10. * scale;
	double cy = self.y0 + (self.height() / 2.);

	if (point.x >= self.x1 && point.x < self.x1 + radius &&
	    point.y >= cy - radius && point.y < cy + radius) {
		/*inside rectangle that approximates the handle */
		return true;
	}

	return false;
}

void
StartBoundaryRect::compute_bounding_box() const
{
	Rectangle::compute_bounding_box ();
	const double scale = UIConfiguration::instance().get_ui_scale();
	const double radius = 10. * scale;
	_bounding_box = _bounding_box.expand (0., radius + _outline_width + 1.0, 0., 0.);
}

void
EndBoundaryRect::render  (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rectangle::render (area, context);

	ArdourCanvas::Rect self (item_to_window (_rect));
	const double scale = UIConfiguration::instance().get_ui_scale();
	const double radius = 10. * scale;

	context->arc (self.x0, (self.y0 + (self.height() / 2.)) - radius, radius, (M_PI/2.), -(M_PI/2.));
	Gtkmm2ext::set_source_rgba (context, _outline_color);
	context->fill ();
}

bool
EndBoundaryRect::covers (ArdourCanvas::Duple const & point) const
{
	ArdourCanvas::Rect self (item_to_window (_rect));
	const double scale = UIConfiguration::instance().get_ui_scale();

	if ((point.x >= self.x0) || ((self.x0 - point.x) < (20. * scale))) {
		/* paste the edge, or within 20 (scaled) pixels of the edge */
		return true;
	}

	/* Approximate the semicircle handle with a square */

	const double radius = 10. * scale;
	double cy = self.y0 + (self.height() / 2.);

	if (point.x <= self.x0 && point.x >= self.x0 - radius && point.y >= cy - radius && point.y < cy + radius) {
		/* within a rectangle approximating the handle */
		return true;
	}

	return false;
}

void
EndBoundaryRect::compute_bounding_box() const
{
	Rectangle::compute_bounding_box ();
	const double scale = UIConfiguration::instance().get_ui_scale();
	const double radius = 10. * scale;
	_bounding_box = _bounding_box.expand (0., 0., 0., radius + _outline_width);
}
