/*
 * Copyright (C) 2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "canvas/framed_curve.h"

using namespace ArdourCanvas;
using std::min;
using std::max;

FramedCurve::FramedCurve (Canvas* c)
	: PolyItem (c)
	, n_samples (0)
	, points_per_segment (16)
	, curve_fill (Inside)
{
}

FramedCurve::FramedCurve (Item* parent)
	: PolyItem (parent)
	, n_samples (0)
	, points_per_segment (16)
	, curve_fill (Inside)
{
}

/** When rendering the curve, we will always draw a fixed number of straight
 * line segments to span the x-axis extent of the curve. More segments:
 * smoother visual rendering. Less rendering: closer to a visibily poly-line
 * render.
 */
void
FramedCurve::set_points_per_segment (uint32_t n)
{
	/* this only changes our appearance rather than the bounding box, so we
	   just need to schedule a redraw rather than notify the parent of any
	   changes
	*/
	points_per_segment = max (n, (uint32_t) 3);
	interpolate ();
	redraw ();
}

void
FramedCurve::compute_bounding_box () const
{
	PolyItem::compute_bounding_box ();

	/* possibly add extents of any point indicators here if we ever do that */
}

void
FramedCurve::set (Points const& p)
{
	PolyItem::set (p);
	interpolate ();
}

void
FramedCurve::interpolate ()
{
	Points curve_points = _points;

	if (curve_points.size()) {
		curve_points.erase (curve_points.begin());
	}
	samples.clear ();

	if (_points.size() == 3) {
		samples.push_back (curve_points.front());
		samples.push_back (curve_points.back());
		n_samples = 2;
	} else {
		InterpolatedCurve::interpolate (curve_points, points_per_segment, CatmullRomCentripetal, false, samples);
		n_samples = samples.size();
	}
}

void
FramedCurve::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (!_outline || _points.size() < 3 || !_bounding_box) {
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

	setup_outline_context (context);
	if (_points.size() == 3) {

		/* straight line */

		Duple window_space;
		Points::const_iterator it = _points.begin();

		Duple first_point = Duple (0.0, 0.0);
		Duple last_point = Duple (0.0, 0.0);

		window_space = item_to_window (*it);
		if (window_space.x <= draw.x0) {
			first_point = Duple (draw.x0, window_space.y);
		} else {
			first_point = Duple (window_space.x, window_space.y);
		}
		context->move_to (first_point.x, first_point.y);

		++it;
		window_space = item_to_window (*it, false);
		if (window_space.x <= draw.x0) {
			context->line_to (draw.x0, window_space.y);
		} else {
			context->line_to (window_space.x, window_space.y);
		}

		window_space = item_to_window (_points.back(), false);
		if (window_space.x >= draw.x1) {
			last_point = Duple (draw.x1, window_space.y);
		} else {
			last_point = Duple (window_space.x, window_space.y);
		}

		context->line_to (last_point.x, last_point.y);

		switch (curve_fill) {
			case None:
				context->stroke();
				break;
			case Inside:
				context->stroke_preserve ();
				window_space = item_to_window (Duple(0.0, draw.height()));
				context->line_to (last_point.x, window_space.y);
				window_space = item_to_window (Duple(0.0, draw.height()));
				context->line_to (first_point.x, window_space.y);
				context->close_path();
				setup_fill_context(context);
				context->fill ();
				break;
			case Outside:
				context->stroke_preserve ();
				window_space = item_to_window (Duple(_points.back().x, 0.0));
				context->line_to (last_point.x, window_space.y);
				window_space = item_to_window (Duple(_points.front().x, 0.0));
				context->line_to (first_point.x, window_space.y);
				context->close_path();
				setup_fill_context(context);
				context->fill ();
				break;
		}
	} else {
		/* curve of at least 3 points */

		/* find left and right-most sample */
		Duple window_space;
		Points::size_type left = 0;
		Points::size_type right = n_samples - 1;

		for (Points::size_type idx = 0; idx < n_samples - 1; ++idx) {
			window_space = item_to_window (Duple (samples[idx].x, 0.0));
			if (window_space.x >= draw.x0) {
				break;
			}
			left = idx;
		}

		for (Points::size_type idx = left; idx < n_samples - 1; ++idx) {
			window_space = item_to_window (Duple (samples[idx].x, 0.0));
			if (window_space.x > draw.x1) {
				right = idx;
				break;
			}
		}

		const Duple first_sample = Duple (samples[left].x, samples[left].y);

		/* move to the first sample's x and the draw height */
		window_space = item_to_window (Duple (first_sample.x, draw.height()));
		context->move_to (window_space.x, window_space.y);

		/* draw line to first sample and then between samples */
		for (uint32_t idx = left; idx <= right; ++idx) {
			window_space = item_to_window (Duple (samples[idx].x, samples[idx].y), false);
			context->line_to (window_space.x, window_space.y);
		}

		/* a redraw may have been requested between the last sample and the last point.
		   if so, draw a line to the last _point.
		*/
		Duple last_sample = Duple (samples[right].x, samples[right].y);

		if (draw.x1 > last_sample.x) {
			last_sample = Duple (_points.back().x, _points.back().y);
			window_space = item_to_window (last_sample, false);
			context->line_to (window_space.x, window_space.y);
		}

		switch (curve_fill) {
			case None:
				context->stroke();
				break;
			case Inside:
				context->stroke_preserve ();
				/* close the frame, possibly using the last _point's x rather than samples[right].x */
				window_space = item_to_window (Duple (last_sample.x, draw.height()));
				context->line_to (window_space.x, window_space.y);
				window_space = item_to_window (Duple (first_sample.x, draw.height()));
				context->line_to (window_space.x, window_space.y);
				context->close_path();
				setup_fill_context(context);
				context->fill ();
				break;
			case Outside:
				context->stroke_preserve ();
				window_space = item_to_window (Duple (last_sample.x, 0.0));
				context->line_to (window_space.x, window_space.y);
				window_space = item_to_window (Duple (first_sample.x, 0.0));
				context->line_to (window_space.x, window_space.y);
				context->close_path();
				setup_fill_context(context);
				context->fill ();
				break;
		}
	}
	context->restore ();

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
FramedCurve::covers (Duple const & pc) const
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
