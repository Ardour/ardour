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
		return boost::shared_ptr<Processor> (0);
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
