/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __CANVAS_RECTSET_H__
#define __CANVAS_RECTSET_H__

#include <vector>

#include "canvas/item.h"
#include "canvas/visibility.h"

namespace ArdourCanvas {

class LIBCANVAS_API RectSet : public Item
{
public:
	RectSet (Canvas*);
	RectSet (Item*);

	void compute_bounding_box () const;
	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;

	bool covers (Duple const &) const;

	void begin_add ();
	void end_add ();

	struct ResetRAII {
		ResetRAII (RectSet& rs) : rects (rs) { rects.clear(); rects.begin_add(); }
		~ResetRAII () { rects.end_add (); }
		RectSet& rects;
	};

	void add_rect (int index, Rect const &, Gtkmm2ext::Color);
	void clear ();

	struct ColoredRectangle : Rect {
		ColoredRectangle (int i, Rect const & ra, Gtkmm2ext::Color color_) : Rect (ra), index (i), color (color_) {}
		int index;
		Gtkmm2ext::Color color;
	};

	std::vector<ColoredRectangle> const & rects() const { return _rects; }

private:
	std::vector<ColoredRectangle> _rects;
};

}

#endif /* __CANVAS_RECTSET_H__ */
