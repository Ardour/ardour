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

#ifndef __ardour_push2_splash_h__
#define __ardour_push2_splash_h__

#include <cairomm/surface.h>

#include "layout.h"
#include "push2.h"

namespace ARDOUR {
	class Stripable;
}

namespace ArdourSurface {

class SplashLayout : public Push2Layout
{
   public:
	SplashLayout (Push2& p, ARDOUR::Session&);
	~SplashLayout ();

	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;

  private:
	Cairo::RefPtr<Cairo::ImageSurface> img;
};

} /* namespace */

#endif /* __ardour_push2_splash_h__ */
