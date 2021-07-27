/*
 * Copyright (C) 2014-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#include "canvas/xfade_curve.h"
#include "canvas/interpolated_curve.h"

using namespace ArdourCanvas;
using std::min;
using std::max;

static const bool show_bg_fades = true;

XFadeCurve::XFadeCurve (Canvas* c)
	: Item (c)
	, points_per_segment (32)
	, _xfadeposition (Start)
	, _outline_color (0x000000ff)
	, _fill_color (0x22448880)
	, show_background_fade (show_bg_fades)
{
}

XFadeCurve::XFadeCurve (Canvas* c, XFadePosition pos)
	: Item (c)
	, points_per_segment (32)
	, _xfadeposition (pos)
	, _outline_color (0x000000ff)
	, _fill_color (0x22448880)
	, show_background_fade (show_bg_fades)
{
}

XFadeCurve::XFadeCurve (Item* parent)
	: Item (parent)
	, points_per_segment (32)
	, _xfadeposition (Start)
	, _outline_color (0x000000ff)
	, _fill_color (0x22448880)
	, show_background_fade (show_bg_fades)
{
}

XFadeCurve::XFadeCurve (Item* parent, XFadePosition pos)
	: Item (parent)
	, points_per_segment (32)
	, _xfadeposition (pos)
	, _outline_color (0x000000ff)
	, _fill_color (0x22448880)
	, show_background_fade (show_bg_fades)
{
}

void
XFadeCurve::compute_bounding_box () const
{
	if (!_in.points.empty() && !_out.points.empty()) {

		Rect bbox;
		Points::const_iterator i;

		if (!_in.points.empty()) {
			i = _in.points.begin();
			bbox.x0 = bbox.x1 = i->x;
			bbox.y0 = bbox.y1 = i->y;

			++i;

			while (i != _in.points.end()) {
				bbox.x0 = min (bbox.x0, i->x);
				bbox.y0 = min (bbox.y0, i->y);
				bbox.x1 = max (bbox.x1, i->x);
				bbox.y1 = max (bbox.y1, i->y);
				++i;
			}
		} else {
			i = _out.points.begin();
			bbox.x0 = bbox.x1 = i->x;
			bbox.y0 = bbox.y1 = i->y;
		}

		if (!_out.points.empty()) {
			i = _out.points.begin();
			while (i != _out.points.end()) {
				bbox.x0 = min (bbox.x0, i->x);
				bbox.y0 = min (bbox.y0, i->y);
				bbox.x1 = max (bbox.x1, i->x);
				bbox.y1 = max (bbox.y1, i->y);
				++i;
			}
		}

		_bounding_box = bbox.expand (1.0);

	} else {
		_bounding_box = Rect ();
	}

	bb_clean ();
}

void
XFadeCurve::set_inout (Points const & in, Points const & out)
{
	if (_in.points == in && _out.points == out) {
		return;
	}
	begin_change ();
	_in.points = in;
	_out.points = out;
	_bounding_box_dirty = true;
	interpolate ();
	end_change ();
}

void
XFadeCurve::set_points_per_segment (uint32_t n)
{
	points_per_segment = n;
	interpolate ();
	redraw ();
}

void
XFadeCurve::interpolate ()
{
	_in.samples.clear ();
	InterpolatedCurve::interpolate (_in.points, points_per_segment, CatmullRomCentripetal, false, _in.samples);
	_in.n_samples = _in.samples.size();

	_out.samples.clear ();
	InterpolatedCurve::interpolate (_out.points, points_per_segment, CatmullRomCentripetal, false, _out.samples);
	_out.n_samples = _out.samples.size();
}

Cairo::Path *
XFadeCurve::get_path(Rect const & area, Cairo::RefPtr<Cairo::Context> context, CanvasCurve const &c) const
{
	assert(c.points.size() > 1);
	context->begin_new_path ();
	Duple window_space;

	if (c.points.size () == 2) {

		window_space = item_to_window (c.points.front(), false);
		context->move_to (window_space.x, window_space.y);
		window_space = item_to_window (c.points.back(), false);
		context->line_to (window_space.x, window_space.y);

	} else {

		/* find left and right-most sample */
		Points::size_type left = 0;
		Points::size_type right = c.n_samples - 1;

		assert (left < right);
		// we should really really do a binary search rather than iterate
		for (Points::size_type idx = 0; idx < c.n_samples - 1; ++idx) {
			left = idx;
			window_space = item_to_window (Duple (c.samples[idx].x, 0.0), false);
			if (window_space.x >= area.x0) break;
		}
		for (Points::size_type idx = c.n_samples - 1; right > left;) {
			if (--idx <= left) break;
			window_space = item_to_window (Duple (c.samples[idx].x, 0.0), false);
			if (window_space.x <= area.x1) break;
			right = idx;
		}

		assert(left < right);
		assert(left < c.n_samples);
		assert(right < c.n_samples);

		/* draw line between samples */
		window_space = item_to_window (Duple (c.samples[left].x, c.samples[left].y), false);
		context->move_to (window_space.x, window_space.y);
		for (uint32_t idx = left + 1; idx <= right; ++idx) {
			window_space = item_to_window (Duple (c.samples[idx].x, c.samples[idx].y), false);
			context->line_to (window_space.x, window_space.y);
		}
	}
	return context->copy_path ();
}

void
XFadeCurve::close_path(Rect const & area, Cairo::RefPtr<Cairo::Context> context, CanvasCurve const &c, bool inside) const
{
	Duple window_space;
	if (inside) {
		window_space = item_to_window (Duple(c.points.back().x, area.height()), false);
		context->line_to (window_space.x, window_space.y);
		window_space = item_to_window (Duple(c.points.front().x, area.height()), false);
		context->line_to (window_space.x, window_space.y);
		context->close_path();
	} else {
		window_space = item_to_window (Duple(c.points.back().x, 0.0), false);
		context->line_to (window_space.x, window_space.y);
		window_space = item_to_window (Duple(c.points.front().x, 0.0), false);
		context->line_to (window_space.x, window_space.y);
		context->close_path();
	}
}

void
XFadeCurve::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (!_bounding_box) { return; }
	if (_in.points.size() < 2) { return; }
	if (_out.points.size() < 2) { return; }

	Rect self = item_to_window (_bounding_box);
	Rect d = self.intersection (area);
	assert (d);
	Rect draw = d;

	context->save ();
	context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
	context->clip ();

	/* expand drawing area by several pixels on each side to avoid cairo stroking effects at the boundary.
	 * they will still occur, but cairo's clipping will hide them.
	 */
	draw = draw.expand (4.0);

	Cairo::Path *path_in = get_path(draw, context, _in);
	Cairo::Path *path_out = get_path(draw, context, _out);

	Gtkmm2ext::Color outline_shaded = _outline_color;
	outline_shaded = 0.5 * (outline_shaded & 0xff) + (outline_shaded & ~0xff);

	Gtkmm2ext::Color fill_shaded = _fill_color;
	fill_shaded = 0.5 * (fill_shaded & 0xff) + (fill_shaded & ~0xff);

#define IS_START (_xfadeposition == Start)

	/* fill primary fade */
	context->begin_new_path ();
	context->append_path (IS_START ? *path_in : *path_out);
	close_path(draw, context, IS_START ?_in : _out, false);
	Gtkmm2ext::set_source_rgba (context, _fill_color);
	context->fill ();

	if (show_background_fade) {
		/* fill background fade */
		context->save ();
		context->begin_new_path ();
		context->append_path (IS_START ? *path_in : *path_out);
		close_path(draw, context, IS_START ? _in : _out, true);
		context->set_fill_rule (Cairo::FILL_RULE_EVEN_ODD);
		context->clip ();
		context->begin_new_path ();
		context->append_path (IS_START ? *path_out: *path_in);
		close_path(draw, context, IS_START ? _out : _in, true);
		Gtkmm2ext::set_source_rgba (context, fill_shaded);
		context->set_fill_rule (Cairo::FILL_RULE_WINDING);
		context->fill ();
		context->restore ();
	}

	/* draw lines over fills */
	/* fade in line */
	if (IS_START || show_background_fade) {
		Gtkmm2ext::set_source_rgba (context, IS_START ? _outline_color : outline_shaded);
		context->set_line_width (IS_START ? 1.0 : .5);

		context->begin_new_path ();
		context->append_path (*path_in);
		context->stroke();
	}

	/* fade out line */
	if (!IS_START || show_background_fade) {
		Gtkmm2ext::set_source_rgba (context, IS_START ? outline_shaded :_outline_color);
		context->set_line_width (IS_START ? .5 : 1.0);

		context->begin_new_path ();
		context->append_path (*path_out);
		context->stroke();
	}

	context->restore ();

	delete path_in;
	delete path_out;
}
