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
#include "ardour/plugin_manager.h"

#include "LuaBridge/LuaBridge.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

int
ARDOUR::LuaAPI::datatype_ctor_nil (lua_State *L)
{
	DataType dt (DataType::NIL);
	luabridge::Stack <DataType>::push (L, dt);
	return 1;
}

int
ARDOUR::LuaAPI::datatype_ctor_audio (lua_State *L)
{
	DataType dt (DataType::AUDIO);
	// NB luabridge will copy construct the object and manage lifetime.
	luabridge::Stack <DataType>::push (L, dt);
	return 1;
}

int
ARDOUR::LuaAPI::datatype_ctor_midi (lua_State *L)
{
	DataType dt (DataType::MIDI);
	luabridge::Stack <DataType>::push (L, dt);
	return 1;
}

boost::shared_ptr<Processor>
ARDOUR::LuaAPI::new_luaproc (Session *s, const string& name)
{
	if (!s) {
		return boost::shared_ptr<Processor> ();
	}

	LuaScriptInfoPtr spi;
	ARDOUR::LuaScriptList & _scripts (LuaScripting::instance ().scripts (LuaScriptInfo::DSP));
	for (LuaScriptList::const_iterator i = _scripts.begin(); i != _scripts.end(); ++i) {
		if (name == (*i)->name) {
			spi = *i;
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

PluginInfoPtr
ARDOUR::LuaAPI::new_plugin_info (const string& name, ARDOUR::PluginType type)
{
	PluginManager& manager = PluginManager::instance();
	PluginInfoList all_plugs;
	all_plugs.insert(all_plugs.end(), manager.ladspa_plugin_info().begin(), manager.ladspa_plugin_info().end());
#ifdef WINDOWS_VST_SUPPORT
	all_plugs.insert(all_plugs.end(), manager.windows_vst_plugin_info().begin(), manager.windows_vst_plugin_info().end());
#endif
#ifdef LXVST_SUPPORT
	all_plugs.insert(all_plugs.end(), manager.lxvst_plugin_info().begin(), manager.lxvst_plugin_info().end());
#endif
#ifdef AUDIOUNIT_SUPPORT
	all_plugs.insert(all_plugs.end(), manager.au_plugin_info().begin(), manager.au_plugin_info().end());
#endif
#ifdef LV2_SUPPORT
	all_plugs.insert(all_plugs.end(), manager.lv2_plugin_info().begin(), manager.lv2_plugin_info().end());
#endif

	for (PluginInfoList::const_iterator i = all_plugs.begin(); i != all_plugs.end(); ++i) {
		if (((*i)->name == name || (*i)->unique_id == name) && (*i)->type == type) {
			return *i;
		}
	}
	return PluginInfoPtr ();
}

boost::shared_ptr<Processor>
ARDOUR::LuaAPI::new_plugin (Session *s, const string& name, ARDOUR::PluginType type, const string& preset)
{
	if (!s) {
		return boost::shared_ptr<Processor> ();
	}

	PluginInfoPtr pip = new_plugin_info (name, type);

	if (!pip) {
		return boost::shared_ptr<Processor> ();
	}

	PluginPtr p = pip->load (*s);
	if (!p) {
		return boost::shared_ptr<Processor> ();
	}

	if (!preset.empty()) {
		const Plugin::PresetRecord *pr = p->preset_by_label (preset);
		if (pr) {
			p->load_preset (*pr);
		}
	}

	return boost::shared_ptr<Processor> (new PluginInsert (*s, p));
}

bool
ARDOUR::LuaAPI::set_plugin_insert_param (boost::shared_ptr<PluginInsert> pi, uint32_t which, float val)
{
	boost::shared_ptr<Plugin> plugin = pi->plugin();
	if (!plugin) { return false; }

	bool ok=false;
	uint32_t controlid = plugin->nth_parameter (which, ok);
	if (!ok) { return false; }
	if (!plugin->parameter_is_input (controlid)) { return false; }

	ParameterDescriptor pd;
	if (plugin->get_parameter_descriptor (controlid, pd) != 0) { return false; }
	if (val < pd.lower || val > pd.upper) { return false; }

	boost::shared_ptr<AutomationControl> c = pi->automation_control (Evoral::Parameter(PluginAutomation, 0, controlid));
	c->set_value (val, PBD::Controllable::NoGroup);
	return true;
}

bool
ARDOUR::LuaAPI::set_processor_param (boost::shared_ptr<Processor> proc, uint32_t which, float val)
{
	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (proc);
	if (!pi) { return false; }
	return set_plugin_insert_param (pi, which, val);
}

int
ARDOUR::LuaOSC::Address::send (lua_State *L)
{
	Address * const luaosc = luabridge::Userdata::get <Address> (L, 1, false);
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
	luabridge::Stack<bool>::push (L, (rv == 0));
	return 1;
}
