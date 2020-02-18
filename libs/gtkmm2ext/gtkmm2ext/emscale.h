/*
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

#ifndef __libgtkmm2ext_emscale_h__
#define __libgtkmm2ext_emscale_h__

#include <map>
#include <string>

#include <pangomm/fontdescription.h>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext
{

class LIBGTKMM2EXT_API EmScale
{
    public:
	EmScale (const Pango::FontDescription&);

	unsigned int char_pixel_width() { if (_char_pixel_width < 1) recalc_char_pixel_geometry() ; return _char_pixel_width; }
	unsigned int char_pixel_height() { if (_char_pixel_height < 1) recalc_char_pixel_geometry() ; return _char_pixel_height; }
	float char_avg_pixel_width() { if (_char_pixel_width < 1) recalc_char_pixel_geometry() ; return _char_avg_pixel_width; }

	static EmScale& by_font (const Pango::FontDescription&);

    private:
	Pango::FontDescription _font;
	unsigned int           _char_pixel_width;
	unsigned int           _char_pixel_height;
	float                  _char_avg_pixel_width;

	void recalc_char_pixel_geometry ();

	static std::map<std::string,EmScale> _emscales;
};

} // namespace

#endif /* __libgtkmm2ext_emscale_h__ */
