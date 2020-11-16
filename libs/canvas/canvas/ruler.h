/*
 * Copyright (C) 2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __CANVAS_RULER_H__
#define __CANVAS_RULER_H__

#include <string>
#include <vector>

#include <pangomm/fontdescription.h>

#include "canvas/rectangle.h"

namespace ArdourCanvas
{

class LIBCANVAS_API Ruler : public Rectangle
{
public:
	struct Mark {
		enum Style {
			Major,
			Minor,
			Micro
		};
		std::string label;
		double position;
		Style  style;
	};

	struct Metric {
		Metric () : units_per_pixel (0) {}
		virtual ~Metric() {}

		double units_per_pixel;

		/* lower and upper and sample positions, which are also canvas coordinates
		 */

		virtual void get_marks (std::vector<Mark>&, int64_t lower, int64_t upper, int maxchars) const = 0;
	};

	Ruler (Canvas*, const Metric& m);
	Ruler (Canvas*, const Metric& m, Rect const&);
	Ruler (Item*, const Metric& m);
	Ruler (Item*, const Metric& m, Rect const&);

	virtual ~Ruler () {
		delete _font_description;
	}

	void set_range (int64_t lower, int64_t upper);
	void set_font_description (Pango::FontDescription);
	void set_second_font_description (Pango::FontDescription);
	void set_metric (const Metric&);

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;

	void set_divide_colors (Gtkmm2ext::Color top, Gtkmm2ext::Color bottom);
	void set_divide_height (double);
private:
	const Metric* _metric;

	/* lower and upper and bounds for ruler */

	int64_t            _lower;
	int64_t            _upper;
	double           _divide_height;
	Gtkmm2ext::Color _divider_color_top;
	Gtkmm2ext::Color _divider_color_bottom;

	Pango::FontDescription* _font_description;
	Pango::FontDescription* _second_font_description;
	mutable std::vector<Mark> marks;
	mutable bool _need_marks;
};

}


#endif
