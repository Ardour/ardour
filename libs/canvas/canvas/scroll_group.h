/*
 * Copyright (C) 2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015 David Robillard <d@drobilla.net>
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

#ifndef __CANVAS_SCROLL_GROUP_H__
#define __CANVAS_SCROLL_GROUP_H__

#include "canvas/container.h"

namespace ArdourCanvas {

/** A ScrollGroup has no contents of its own, but renders
 *  its children in a way that reflects the most recent
 *  call to its scroll_to() method.
 */
class LIBCANVAS_API ScrollGroup : public Container
{
  public:
	enum ScrollSensitivity {
		ScrollsVertically = 0x1,
		ScrollsHorizontally = 0x2
	};

	ScrollGroup (Canvas*, ScrollSensitivity);
	ScrollGroup (Item*, ScrollSensitivity);

	void scroll_to (Duple const& d);
	Duple scroll_offset() const { return _scroll_offset; }

	bool covers_canvas (Duple const& d) const;
	bool covers_window (Duple const& d) const;

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;

	ScrollSensitivity sensitivity() const { return _scroll_sensitivity; }

  private:
	ScrollSensitivity _scroll_sensitivity;
	Duple             _scroll_offset;
};

}

#endif
