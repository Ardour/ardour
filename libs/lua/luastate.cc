/*
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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

#include <assert.h>
#include "lua/luastate.h"

// from lauxlib.c
static int panic (lua_State *L) {
	lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n",
			lua_tostring(L, -1));
	return 0;  /* return to Lua to abort */
}

LuaState::LuaState()
	: L (luaL_newstate ())
{
	assert (L);
	init ();
}

LuaState::LuaState(lua_State *ls)
	: L (ls)
{
	assert (L);
	init ();
}

LuaState::~LuaState() {
	lua_close (L);
}

void
LuaState::init() {
	lua_atpanic (L, &panic);
	luaL_openlibs (L);
	lua_pushlightuserdata (L, this);
	lua_pushcclosure (L, &LuaState::_print, 1);
	lua_setglobal (L, "print");
}

int
LuaState::do_command (std::string cmd) {
	int result = luaL_dostring (L, cmd.c_str());
	if (result != 0) {
		print ("Error: " + std::string (lua_tostring (L, -1)));
	}
	return result;
}

int
LuaState::do_file (std::string fn) {
	int result = luaL_dofile (L, fn.c_str());
	if (result != 0) {
		print ("Error: " + std::string (lua_tostring (L, -1)));
	}
	return result;
}

void
LuaState::collect_garbage () {
	lua_gc (L, LUA_GCCOLLECT, 0);
}

void
LuaState::collect_garbage_step (int debt) {
	lua_gc (L, LUA_GCSTEP, debt);
}

void
LuaState::tweak_rt_gc () {
	/* GC runs same speed as  memory allocation */
	lua_gc (L, LUA_GCSETPAUSE, 100);
	lua_gc (L, LUA_GCSETSTEPMUL, 100);
}

void
LuaState::sandbox (bool rt_safe) {
	do_command ("dofile = nil require = nil dofile = nil package = nil debug = nil os.exit = nil os.setlocale = nil rawget = nil rawset = nil coroutine = nil module = nil");
	if (rt_safe) {
		do_command ("os = nil io = nil loadfile = nil");
	}
}


void
LuaState::print (std::string text) {
	Print (text); /* EMIT SIGNAL */
}

int
LuaState::_print (lua_State *L) {
	LuaState* const luaState = static_cast <LuaState*> (lua_touserdata (L, lua_upvalueindex (1)));
	std::string text;
	int n = lua_gettop(L);  /* number of arguments */
	int i;
	lua_getglobal(L, "tostring");
	for (i=1; i<=n; i++) {
		const char *s;
		size_t l;
		lua_pushvalue(L, -1);  /* function to be called */
		lua_pushvalue(L, i);   /* value to print */
		lua_call(L, 1, 1);
		s = lua_tolstring(L, -1, &l);  /* get result */
		if (s == NULL)
			return luaL_error(L, "'tostring' must return a string to 'print'");
		if (i > 1) text += " ";
		text += std::string (s, l);
		lua_pop(L, 1);  /* pop result */
	}
	luaState->print (text);
	return 0;
}
