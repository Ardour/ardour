/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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
#ifndef _luasignal_h_
#define _luasignal_h_
namespace LuaSignal {

#define ENGINE(name,c,p) name,
#define STATIC(name,c,p) name,
#define SESSION(name,c,p) name,

	enum LuaSignal {
#		include "luasignal_syms.h"
		LAST_SIGNAL
	};

#undef ENGINE
#undef SESSION
#undef STATIC

extern const char *luasignalstr[];
inline const char* enum2str (LuaSignal i) { return luasignalstr[i]; }
LuaSignal str2luasignal (const std::string &);

} // namespace
#endif
