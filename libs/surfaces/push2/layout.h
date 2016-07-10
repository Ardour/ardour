/*
    Copyright (C) 2016 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_push2_layout_h__
#define __ardour_push2_layout_h__

#include <stdint.h>

#include <cairomm/refptr.h>

namespace ARDOUR {
	class Session;
}

namespace Cairo {
	class Context;
}

namespace ArdourSurface {

class Push2;

class Push2Layout
{
  public:
	Push2Layout (Push2& p, ARDOUR::Session& s);
	virtual ~Push2Layout ();

	bool mapped() const;

	virtual bool redraw (Cairo::RefPtr<Cairo::Context>) const = 0;

	virtual void button_upper (uint32_t n) {}
	virtual void button_lower (uint32_t n) {}
	virtual void button_up ()  {}
	virtual void button_down ()  {}
	virtual void button_right ()  {}
	virtual void button_left ()  {}
	virtual void button_select_press () {}
	virtual void button_select_release () {}

	virtual void strip_vpot (int, int) = 0;
	virtual void strip_vpot_touch (int, bool) = 0;

  protected:
	Push2& p2;
	ARDOUR::Session& session;
};

} /* namespace */

#endif /* __ardour_push2_layout_h__ */
