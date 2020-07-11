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

#ifndef __CANVAS_CBOX_H__
#define __CANVAS_CBOX_H__

#include <list>

#include "canvas/constraint_packer.h"

namespace ArdourCanvas
{

class Rectangle;
class BoxConstrainedItem;

class LIBCANVAS_API cBox : public ConstraintPacker
{
public:
	cBox (Canvas *, Orientation);
	cBox (Item *, Orientation);

	void set_collapse_on_hide (bool);
	void set_homogenous (bool);

  protected:
	void child_changed (bool bbox_changed);

  private:
	bool collapse_on_hide;
	bool homogenous;
};

}

#endif /* __CANVAS_CBOX_H__ */
