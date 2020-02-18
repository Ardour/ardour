/*
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Nick Mainsbridge <mainsbridge@gmail.com>
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
#include <exception>
#include <algorithm>

#include "canvas/curve.h"

using namespace ArdourCanvas;
using std::min;
using std::max;

Curve::Curve (Canvas* c)
	: PolyItem (c)
	, n_samples (0)
	, points_per_segment (16)
	, curve_fill (None)
{
}

Curve::Curve (Item* parent)
	: PolyItem (parent)
	, n_samples (0)
	, points_per_segment (16)
	, curve_fill (None)
{
}

/** When rendering the curve, we will always draw a fixed number of straight
 * line segments to span the x-axis extent of the curve. More segments:
 * smoother visual rendering. Less rendering: closer to a visibily poly-line
 * render.
 */
void
Curve::set_points_per_segment (uint32_t n)
{
	/* this only changes our appearance rather than the bounding box, so we
	   just need to schedule a redraw rather than notify the parent of any
	   changes
	*/
	points_per_segment = n;
	interpolate ();
	redraw ();
}

void
Curve::compute_bounding_box () const
{
	PolyItem::compute_bounding_box ();

	/* possibly add extents of any point indicators here if we ever do that */
}

void
Curve::set (Points const& p)
{
	PolyItem::set (p);
	interpolate ();
}

void
Curve::interpolate ()
{
	samples.clear ();
	InterpolatedCurve::interpolate (_points, points_per_segment, CatmullRomCentripetal, false, samples);
	n_samples = samples.size();
}

void
Curve::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (!_outline || _points.size() < 2 || !_bounding_box) {
		return;
	}

	Rect self = item_to_window (_bounding_box);
	Rect d = self.intersection (area);
	assert (d);
	Rect draw = d;

	/* Our approach is to always draw n_segments across our total size.
	 *
	 * This is very inefficient if we are asked to only draw a small
	 * section of the curve. For now we rely on cairo clipping to help
	 * with this.
	 */


	setup_outline_context (context);

	if (_points.size() == 2) {

		/* straight line */

		Duple window_space;

		window_space = item_to_window (_points.front());
		context->move_to (window_space.x, window_space.y);
		window_space = item_to_window (_points.back());
		context->line_to (window_space.x, window_space.y);


		switch (curve_fill) {
			case None:
				context->stroke();
				break;
			case Inside:
				context->stroke_preserve ();
				window_space = item_to_window (Duple(_points.back().x, draw.height()));
				context->line_to (window_space.x, window_space.y);
				window_space = item_to_window (Duple(_points.front().x, draw.height()));
				context->line_to (window_space.x, window_space.y);
				context->close_path();
				setup_fill_context(context);
				context->fill ();
				break;
			case Outside:
				context->stroke_preserve ();
				window_space = item_to_window (Duple(_points.back().x, 0.0));
				context->line_to (window_space.x, window_space.y);
				window_space = item_to_window (Duple(_points.front().x, 0.0));
				context->line_to (window_space.x, window_space.y);
				context->close_path();
				setup_fill_context(context);
				context->fill ();
				break;
		}

	} else {

		/* curve of at least 3 points */

		/* x-axis limits of the curve, in window space coordinates */

		Duple w1 = item_to_window (Duple (_points.front().x, 0.0));
		Duple w2 = item_to_window (Duple (_points.back().x, 0.0));

		/* clamp actual draw to area bound by points, rather than our bounding box which is slightly different */

		context->save ();
		context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
		context->clip ();

		/* expand drawing area by several pixels on each side to avoid cairo stroking effects at the boundary.
		   they will still occur, but cairo's clipping will hide them.
		 */

		draw = draw.expand (4.0);

		/* now clip it to the actual points in the curve */

		if (draw.x0 < w1.x) {
			draw.x0 = w1.x;
		}

		if (draw.x1 >= w2.x) {
			draw.x1 = w2.x;
		}

		/* find left and right-most sample */
		Duple window_space;
		Points::size_type left = 0;
		Points::size_type right = n_samples;

		for (Points::size_type idx = 0; idx < n_samples - 1; ++idx) {
			left = idx;
			window_space = item_to_window (Duple (samples[idx].x, 0.0));
			if (window_space.x >= draw.x0) break;
		}
		for (Points::size_type idx = n_samples; idx > left + 1; --idx) {
			window_space = item_to_window (Duple (samples[idx].x, 0.0));
			if (window_space.x <= draw.x1) break;
			right = idx;
		}

		/* draw line between samples */
		window_space = item_to_window (Duple (samples[left].x, samples[left].y));
		context->move_to (window_space.x, window_space.y);
		for (uint32_t idx = left + 1; idx < right; ++idx) {
			window_space = item_to_window (Duple (samples[idx].x, samples[idx].y));
			context->line_to (window_space.x, window_space.y);
		}

		switch (curve_fill) {
			case None:
				context->stroke();
				break;
			case Inside:
				context->stroke_preserve ();
				window_space = item_to_window (Duple (samples[right-1].x, draw.height()));
				context->line_to (window_space.x, window_space.y);
				window_space = item_to_window (Duple (samples[left].x, draw.height()));
				context->line_to (window_space.x, window_space.y);
				context->close_path();
				setup_fill_context(context);
				context->fill ();
				break;
			case Outside:
				context->stroke_preserve ();
				window_space = item_to_window (Duple (samples[right-1].x, 0.0));
				context->line_to (window_space.x, window_space.y);
				window_space = item_to_window (Duple (samples[left].x, 0.0));
				context->line_to (window_space.x, window_space.y);
				context->close_path();
				setup_fill_context(context);
				context->fill ();
				break;
		}
		context->restore ();
	}

#if 0
	/* add points */
	setup_outline_context (context);
	for (Points::const_iterator p = _points.begin(); p != _points.end(); ++p) {
		Duple window_space (item_to_window (*p));
		context->arc (window_space.x, window_space.y, 5.0, 0.0, 2 * M_PI);
		context->stroke ();
	}
#endif
}

bool
Curve::covers (Duple const & pc) const
{
	Duple point = window_to_item (pc);

	/* O(N) N = number of points, and not accurate */

	for (Points::const_iterator p = _points.begin(); p != _points.end(); ++p) {

		const Coord dx = point.x - (*p).x;
		const Coord dy = point.y - (*p).y;
		const Coord dx2 = dx * dx;
		const Coord dy2 = dy * dy;

		if ((dx2 < 2.0 && dy2 < 2.0) || (dx2 + dy2 < 4.0)) {
			return true;
		}
	}

	return false;
}
