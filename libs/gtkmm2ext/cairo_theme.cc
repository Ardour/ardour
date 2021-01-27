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

#include "gtkmm2ext/cairo_theme.h"

using namespace Gtkmm2ext;

bool CairoTheme::_flat_buttons    = false;
bool CairoTheme::_boxy_buttons    = false;
bool CairoTheme::_widget_prelight = true;

void
CairoTheme::set_flat_buttons (bool yn)
{
	_flat_buttons = yn;
}

void
CairoTheme::set_boxy_buttons (bool yn)
{
	_boxy_buttons = yn;
}

void
CairoTheme::set_widget_prelight (bool yn)
{
	_widget_prelight = yn;
}
