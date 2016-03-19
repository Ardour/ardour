/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#include <cstring>

#include "pbd/error.h"
#include "pbd/compose.h"

#include "ardour/lua_api.h"
#include "ardour/luaproc.h"
#include "ardour/luascripting.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

boost::shared_ptr<Processor>
ARDOUR::LuaAPI::new_luaproc (Session *s, const string& name)
{
	if (!s) {
		return boost::shared_ptr<Processor> ();
	}

	LuaScriptInfoPtr spi;
	ARDOUR::LuaScriptList & _scripts (LuaScripting::instance ().scripts (LuaScriptInfo::DSP));
	for (LuaScriptList::const_iterator s = _scripts.begin(); s != _scripts.end(); ++s) {
		if (name == (*s)->name) {
			spi = *s;
			break;
		}
	}

	if (!spi) {
		warning << _("Script with given name was not found\n");
		return boost::shared_ptr<Processor> ();
	}

	PluginPtr p;
	try {
		LuaPluginInfoPtr lpi (new LuaPluginInfo(spi));
		p = (lpi->load (*s));
	} catch (...) {
		warning << _("Failed to instantiate Lua Processor\n");
		return boost::shared_ptr<Processor> ();
	}

	return boost::shared_ptr<Processor> (new PluginInsert (*s, p));
}

int
ARDOUR::LuaAPI::LuaOSCAddress::send (lua_State *L)
{
	LuaOSCAddress * const luaosc = luabridge::Userdata::get <LuaOSCAddress> (L, 1, false);
	if (!luaosc) {
		return luaL_error (L, "Invalid pointer to OSC.Address");
	}
	if (!luaosc->_addr) {
		return luaL_error (L, "Invalid Destination Address");
	}

	int top = lua_gettop(L);
	if (top < 3) {
    return luaL_argerror (L, 1, "invalid number of arguments, :send (path, type, ...)");
	}

	const char* path = luaL_checkstring (L, 2);
	const char* type = luaL_checkstring (L, 3);
	assert (path && type);

	if ((int) strlen(type) != top - 3) {
    return luaL_argerror (L, 3, "type description does not match arguments");
	}

	lo_message msg = lo_message_new ();

	for (int i = 4; i <= top; ++i) {
		char t = type[i - 4];
		int lt = lua_type(L, i);
		int ok = -1;
		switch(lt) {
			case LUA_TSTRING:
				if (t == LO_STRING) {
					ok = lo_message_add_string (msg, luaL_checkstring(L, i));
				} else if (t ==  LO_CHAR) {
					char c = luaL_checkstring (L, i) [0];
					ok = lo_message_add_char (msg, c);
				}
				break;
			case LUA_TBOOLEAN:
				if (t == LO_TRUE || t == LO_FALSE) {
					if (lua_toboolean (L, i)) {
						ok = lo_message_add_true (msg);
					} else {
						ok = lo_message_add_false (msg);
					}
				}
				break;
			case LUA_TNUMBER:
				if (t == LO_INT32) {
					ok = lo_message_add_int32 (msg, (int32_t) luaL_checkinteger(L, i));
				}
				else if (t == LO_FLOAT) {
					ok = lo_message_add_float (msg, (float) luaL_checknumber(L, i));
				}
				else if (t == LO_DOUBLE) {
					ok = lo_message_add_double (msg, (double) luaL_checknumber(L, i));
				}
				else if (t == LO_INT64) {
					ok = lo_message_add_double (msg, (int64_t) luaL_checknumber(L, i));
				}
				break;
			default:
				break;
		}
		if (ok != 0) {
			return luaL_argerror (L, i, "type description does not match parameter");
		}
	}

	int rv = lo_send_message (luaosc->_addr, path, msg);
	lo_message_free (msg);
	luabridge::Stack<int>::push (L, rv);
	return 1;
}
