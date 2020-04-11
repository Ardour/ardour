/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#ifndef __CANVAS_POLY_ITEM_H__
#define __CANVAS_POLY_ITEM_H__

#include "canvas/item.h"
#include "canvas/outline.h"
#include "canvas/visibility.h"

namespace ArdourCanvas {

class LIBCANVAS_API PolyItem : public Item
{
public:
	PolyItem (Canvas*);
	PolyItem (Item*);

	virtual void compute_bounding_box () const;

	virtual void  set (Points const&);
	Points const& get () const;

	void dump (std::ostream&) const;

protected:
	void render_path (Rect const&, Cairo::RefPtr<Cairo::Context>) const;

	Points _points;

	/* these return screen-cordidates of the most recent render_path() */
	Duple const& left_edge ()  const { return _left; }
	Duple const& right_edge () const { return _right; }

private:
	static bool interpolate_line (Duple&, Duple const&, Coord const);

	mutable Duple _left;
	mutable Duple _right;
};

}

#endif
