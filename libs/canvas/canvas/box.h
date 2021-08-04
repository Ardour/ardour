/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __CANVAS_BOX_H__
#define __CANVAS_BOX_H__

#include "canvas/rectangle.h"

namespace ArdourCanvas
{

class Rectangle;

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
class LIBCANVAS_API Box : public Rectangle
{
public:
	enum Orientation {
		Vertical,
		Horizontal
	};

	Box (Canvas *, Orientation);
	Box (Item *, Orientation);
	Box (Item *, Duple const & position, Orientation);

	void set_spacing (double s);
	void set_padding (double top, double right = -1.0, double bottom = -1.0, double left = -1.0);
	void set_margin (double top, double right = -1.0, double bottom = -1.0, double left = -1.0);

	/* aliases so that CSS box model terms work */
	void set_border_width (double w) { set_outline_width (w); }
	void set_border_color (Gtkmm2ext::Color c)  { set_outline_color (c); }

	void add (Item*);
	void add_front (Item*);
	void layout ();

	void set_collapse_on_hide (bool);
	void set_homogenous (bool);

	void compute_bounding_box () const;
	void size_request (double& w, double& h) const;
	void size_allocate_children (Rect const & r);
	void _size_allocate (Rect const & r);

  protected:
	Orientation orientation;
	double spacing;
	double top_padding, right_padding, bottom_padding, left_padding;
	double top_margin, right_margin, bottom_margin, left_margin;

	void child_changed (bool bbox_changed);
  private:
	bool collapse_on_hide;
	bool homogenous;
	mutable bool ignore_child_changes;

	void reposition_children (Distance width, Distance height, bool width_shrink, bool height_shrink);
};

class LIBCANVAS_API VBox : public Box
{
  public:
	VBox (Canvas *);
	VBox (Item *);
	VBox (Item *, Duple const & position);
};

class LIBCANVAS_API HBox : public Box
{
  public:
	HBox (Canvas *);
	HBox (Item *);
	HBox (Item *, Duple const & position);
};

}

#endif
