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

#include <stdio.h>
#include <string.h>
#include <iostream>

#include "ardour/luabindings.h"
#include "ardour/revision.h"
#include "luainstance.h"
#include "LuaBridge/LuaBridge.h"

#ifdef WAF_BUILD
#include "gtk2ardour-version.h"
#endif

int main (int argc, char **argv)
{
#ifdef LUABINDINGDOC
	luabridge::setPrintBindings (true);
	LuaState lua;
	lua_State* L = lua.getState ();
#ifdef LUADOCOUT
	printf ("-- %s\n", ARDOUR::revision);
	printf ("doc = {\n");
#else
	printf ("[\n");
	printf ("{\"version\" :  \"%s\"},\n\n", ARDOUR::revision);
#endif
	LuaInstance::register_classes (L);
	LuaInstance::register_hooks (L);
	ARDOUR::LuaBindings::dsp (L);
#ifdef LUADOCOUT
	printf ("}\n");
#else
	printf ("{} ]\n");
#endif
	return 0;
#else
	return 1;
#endif
}
