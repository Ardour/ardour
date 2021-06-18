/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_push2_splash_h__
#define __ardour_push2_splash_h__

#include <cairomm/surface.h>

#include "layout.h"
#include "push2.h"

namespace ArdourSurface {

class SplashLayout : public Push2Layout
{
   public:
	SplashLayout (Push2& p, ARDOUR::Session&, std::string const &);
	~SplashLayout ();

	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;

	void strip_vpot (int, int) {}
	void strip_vpot_touch (int, bool) {}

  private:
	Cairo::RefPtr<Cairo::ImageSurface> img;
};

} /* namespace */

#endif /* __ardour_push2_splash_h__ */
