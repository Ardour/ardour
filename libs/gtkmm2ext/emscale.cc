/*
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <algorithm>
#include <gdk/gdk.h>
#include <pangomm/layout.h>

#include "gtkmm2ext/emscale.h"

#include "pbd/i18n.h"

using namespace Gtkmm2ext;

std::map<std::string,EmScale> EmScale::_emscales;

EmScale::EmScale (const Pango::FontDescription& fd)
	: _font (fd)
	, _char_pixel_width (-1)
	, _char_pixel_height (-1)
	, _char_avg_pixel_width (-1)
{
}

void
EmScale::recalc_char_pixel_geometry ()
{
	if (_char_pixel_height > 0 && _char_pixel_width > 0) {
		return;
	}

	Glib::RefPtr<Pango::Context> pc = Glib::wrap (gdk_pango_context_get_for_screen (gdk_screen_get_default()));
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (pc);

	layout->set_font_description (_font);

	int w, h;
	std::string x = _("ABCDEFGHIJLKMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
	layout->set_text (x);
	layout->get_pixel_size (w, h);
	_char_pixel_height = std::max(4, h);
	// number of actual chars in the string (not bytes)
	// Glib to the rescue.
	Glib::ustring gx(x);
	_char_avg_pixel_width = w / (float)gx.size();
	_char_pixel_width = std::max(4, (int) ceil (_char_avg_pixel_width));
}

EmScale&
EmScale::by_font (const Pango::FontDescription& fd)
{
	std::map<std::string,EmScale>::iterator i = _emscales.find (fd.to_string());

	if (i == _emscales.end()) {
		i = _emscales.insert (std::make_pair (fd.to_string(), EmScale (fd))).first;
	}

	return i->second;
}
