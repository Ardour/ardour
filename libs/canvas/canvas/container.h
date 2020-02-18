/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __CANVAS_CONTAINER_H__
#define __CANVAS_CONTAINER_H__

#include "canvas/item.h"

namespace ArdourCanvas
{

/** a Container is an item which has no content of its own
 * but renders its children in some geometrical arrangement.
 *
 * Imagined examples of containers:
 *
 *   Container: renders each child at the child's self-determined position
 *   Box: renders each child along an axis (vertical or horizontal)
 *   Table/Grid: renders each child within a two-dimensional grid
 *
 *   Other?
 */
class LIBCANVAS_API Container : public Item
{
public:
	Container (Canvas *);
	Container (Item *);
	Container (Item *, Duple const & position);

	/** The compute_bounding_box() method is likely to be identical
	 * in all containers (the union of the children's bounding boxes).
	 * It can be overridden as necessary.
	 */
	void compute_bounding_box () const;

	/** The render() method is likely to be identical in all containers
	 *  (just call Item::render_children()). It can be overridden as necessary.
	 */
	void render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;

	/** The prepare_for_render() method is likely to be identical in all
	 * containers (just call Item::prepare_for_render_children()). It can be
	 * overridden as necessary.
	 */
	void prepare_for_render (Rect const & area) const;
};

}

#endif
