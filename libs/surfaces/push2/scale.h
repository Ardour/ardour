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

#ifndef __ardour_push2_scale_layout_h__
#define __ardour_push2_scale_layout_h__

#include "layout.h"

namespace ARDOUR {
	class Stripable;
}

namespace ArdourSurface {

class ScaleLayout : public Push2Layout
{
   public:
	ScaleLayout (Push2& p, ARDOUR::Session&, Cairo::RefPtr<Cairo::Context>);
	~ScaleLayout ();

	bool redraw (Cairo::RefPtr<Cairo::Context>, bool force) const;

	void button_upper (uint32_t n);
	void button_lower (uint32_t n);

	void strip_vpot (int, int);
	void strip_vpot_touch (int, bool);

   private:
	Push2Menu* scale_menu;
	void build_scale_menu (Cairo::RefPtr<Cairo::Context>);
};

} /* namespace */

#endif /* __ardour_push2_scale_layout_h__ */
