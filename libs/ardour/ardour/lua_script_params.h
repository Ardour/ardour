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
#ifndef _ardour_lua_script_params_h_
#define _ardour_lua_script_params_h_

#include "lua/luastate.h"

#include "ardour/libardour_visibility.h"
#include "ardour/luascripting.h"

namespace luabridge {
	class LuaRef;
}

/* Semantically these are static functions of the LuaScripting class
 * but are kept separately to minimize header includes.
 *
 * LuaScripting itself is a standalone abstraction (not depending on luabridge)
 * luascripting.h is included by session.h (this file is not).
 *
 * The implementation of these functions is in libs/ardour/luascripting.cc
 */
namespace ARDOUR { namespace LuaScriptParams {

	LIBARDOUR_API LuaScriptParamList script_params (const LuaScriptInfoPtr&, const std::string &);
	LIBARDOUR_API LuaScriptParamList script_params (const std::string &, const std::string &, bool file=true);
	LIBARDOUR_API LuaScriptParamList script_params (LuaState&, const std::string &, const std::string &, bool file=true);
	LIBARDOUR_API void params_to_ref (luabridge::LuaRef *tbl_args, const LuaScriptParamList&);
	LIBARDOUR_API void ref_to_params (LuaScriptParamList&, luabridge::LuaRef *tbl_args);

} } // namespace

#endif // _ardour_lua_script_params_h_
