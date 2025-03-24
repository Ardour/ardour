/*
 * Copyright (C) 2007-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
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

#pragma once

#include <cstdint>
#include <string>

namespace Gtk {
	class Menu;
	namespace Menu_Helpers {
		class MenuList;
	}
}

namespace ARDOUR {
	class InstrumentInfo;
}

inline static void clamp_to_0_127(uint8_t &val)
{
	if ((127 < val) && (val < 192)) {
		val = 127;
	} else if ((192 <= val) && (val < 255)) {
		val = 0;
	}
}


void
build_controller_menu (Gtk::Menu& menu, ARDOUR::InstrumentInfo const & instrument_info, uint16_t channel_mask,
                       std::function<void (Gtk::Menu_Helpers::MenuList&, int, const std::string&)> add_single,
                       std::function<void (Gtk::Menu_Helpers::MenuList&, uint16_t, int, const std::string&)> add_multi);
