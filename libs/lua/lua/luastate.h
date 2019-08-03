/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#ifndef LUA_STATE_H
#define LUA_STATE_H

#include <string>
#include <sigc++/sigc++.h>

#include "lua/liblua_visibility.h"
#include "lua/lua.h"

class LIBLUA_API LuaState {
public:
	LuaState();
	LuaState(lua_State *ls);
	~LuaState();

	int do_command (std::string);
	int do_file (std::string);
	void collect_garbage ();
	void collect_garbage_step (int debt = 0);
	void tweak_rt_gc ();
	void sandbox (bool rt_safe = false);

	sigc::signal<void,std::string> Print;

	lua_State* getState () { return L; }

protected:
	lua_State* L;

private:
	void init ();
  static int _print (lua_State *L);
	void print (std::string text);

};

#endif
