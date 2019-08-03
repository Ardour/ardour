/*
 * Copyright (C) 2014-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __CANVAS_XFADECURVE_H__
#define __CANVAS_XFADECURVE_H__

#include "canvas/visibility.h"
#include "canvas/item.h"
#include "canvas/curve.h"

namespace ArdourCanvas {

class LIBCANVAS_API XFadeCurve : public Item, public InterpolatedCurve
{
public:
	enum XFadePosition {
		Start,
		End,
	};

	XFadeCurve (Canvas *);
	XFadeCurve (Canvas *, XFadePosition);
	XFadeCurve (Item*);
	XFadeCurve (Item*, XFadePosition);

	void set_fade_position (XFadePosition xfp) { _xfadeposition = xfp; }
	void set_show_background_fade (bool show) { show_background_fade = show; }

	void compute_bounding_box () const;
	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;

	void set_points_per_segment (uint32_t n);
	void set_inout (Points const & in, Points const & out);

	void set_outline_color (Gtkmm2ext::Color c) {
		begin_visual_change ();
		_outline_color = c;
		end_visual_change ();
	};

	void set_fill_color (Gtkmm2ext::Color c) {
		begin_visual_change ();
		_fill_color = c;
		end_visual_change ();
	}

private:
	struct CanvasCurve {
		CanvasCurve() : n_samples(0) { }
		Points points;
		Points samples;
		Points::size_type n_samples;
	};

	Cairo::Path * get_path(Rect const &, Cairo::RefPtr<Cairo::Context>, CanvasCurve const &) const;
	void close_path(Rect const &, Cairo::RefPtr<Cairo::Context>, CanvasCurve const &p, bool) const;

	uint32_t points_per_segment;

	CanvasCurve _in;
	CanvasCurve _out;

	XFadePosition    _xfadeposition;
	Gtkmm2ext::Color _outline_color;
	Gtkmm2ext::Color _fill_color;

	bool show_background_fade;

	void interpolate ();
};

}

#endif
