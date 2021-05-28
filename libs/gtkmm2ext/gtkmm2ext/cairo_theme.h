/*
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#ifndef _libgtkmm2ext_cairo_theme_h_
#define _libgtkmm2ext_cairo_theme_h_

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext
{

class LIBGTKMM2EXT_API CairoTheme
{
public:
	static void set_flat_buttons (bool yn);
	static void set_boxy_buttons (bool yn);
	static void set_widget_prelight (bool yn);

	static bool flat_buttons ()
	{
		return _flat_buttons;
	}

	static bool boxy_buttons ()
	{
		return _boxy_buttons;
	}

	static bool widget_prelight ()
	{
		return _widget_prelight;
	}

private:
	static bool _flat_buttons;
	static bool _boxy_buttons;
	static bool _widget_prelight;
};

} // namespace Gtkmm2ext
#endif
