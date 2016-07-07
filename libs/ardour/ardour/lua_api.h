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
#ifndef _ardour_lua_api_h_
#define _ardour_lua_api_h_

#include <string>
#include <lo/lo.h>
#include <boost/shared_ptr.hpp>

#include "ardour/libardour_visibility.h"

#include "ardour/processor.h"
#include "ardour/session.h"

namespace ARDOUR { namespace LuaAPI {

	/** convenience constructor for DataType::NIL with managed lifetime
	 * @returns DataType::NIL
	 */
	int datatype_ctor_null (lua_State *lua);
	/** convenience constructor for DataType::AUDIO with managed lifetime
	 * @returns DataType::AUDIO
	 */
	int datatype_ctor_audio (lua_State *L);
	/** convenience constructor for DataType::MIDI with managed lifetime
	 * @returns DataType::MIDI
	 */
	int datatype_ctor_midi (lua_State *L);

	/** Create a null processor shared pointer
	 *
	 * This is useful for Track:bounce() to indicate no processing.
	 */
	boost::shared_ptr<ARDOUR::Processor> nil_processor ();

	/** create a new Lua Processor (Plugin)
	 *
	 * @param s Session Handle
	 * @param p Identifier or Name of the Processor
	 * @returns Processor object (may be nil)
	 */
	boost::shared_ptr<ARDOUR::Processor> new_luaproc (ARDOUR::Session *s, const std::string& p);

	/** search a Plugin
	 *
	 * @param id Plugin Name, ID or URI
	 * @param type Plugin Type
	 * @returns PluginInfo or nil if not found
	 */
	boost::shared_ptr<ARDOUR::PluginInfo> new_plugin_info (const std::string& id, ARDOUR::PluginType type);

	/** create a new Plugin Instance
	 *
	 * @param s Session Handle
	 * @param id Plugin Name, ID or URI
	 * @param type Plugin Type
	 * @returns Processor or nil
	 */
	boost::shared_ptr<ARDOUR::Processor> new_plugin (ARDOUR::Session *s, const std::string& id, ARDOUR::PluginType type, const std::string& preset = "");

	/** set a plugin control-input parameter value
	 *
	 * @param proc Plugin-Processor
	 * @param which control-input to set (starting at 0)
	 * @param value value to set
	 * @returns true on success, false on error or out-of-bounds value
	 */
	bool set_processor_param (boost::shared_ptr<ARDOUR::Processor> proc, uint32_t which, float val);
	/** set a plugin control-input parameter value
	 *
	 * This is a wrapper around set_processor_param which looks up the Processor by plugin-insert.
	 *
	 * @param proc Plugin-Insert
	 * @param which control-input to set (starting at 0)
	 * @param value value to set
	 * @returns true on success, false on error or out-of-bounds value
	 */
	bool set_plugin_insert_param (boost::shared_ptr<ARDOUR::PluginInsert> pi, uint32_t which, float val);

	/**
	 * A convenience function to get a Automation Lists and ParamaterDescriptor
	 * for a given plugin control.
	 *
	 * This is equivalent to the following lua code
	 * @code
	 * function (processor, param_id)
	 *  local plugininsert = processor:to_insert ()
	 *  local plugin = plugininsert:plugin(0)
	 *  local _, t = plugin:get_parameter_descriptor(param_id, ARDOUR.ParameterDescriptor ())
	 *  local ctrl = Evoral.Parameter (ARDOUR.AutomationType.PluginAutomation, 0, param_id)
	 *  local ac = pi:automation_control (ctrl, false)
	 *  local acl = ac:alist()
	 *  return ac:alist(), ac:to_ctrl():list(), t[2]
	 * end
	 * @endcode
	 *
	 * Example usage: get the third input parameter of first plugin on the given route
	 * (Ardour starts counting at zero).
	 * @code
	 * local al, cl, pd = ARDOUR.LuaAPI.plugin_automation (route:nth_plugin (0), 3)
	 * @endcode
	 * @returns 3 parameters: AutomationList, ControlList, ParamaterDescriptor
	 */
	int plugin_automation (lua_State *lua);

	/**
	 * A convenience function for colorspace HSL to RGB conversion.
	 * All ranges are 0..1
	 *
	 * Example:
	 * @code
	 * local r, g, b, a = ARDOUR.LuaAPI.hsla_to_rgba (hue, saturation, luminosity, alpha)
	 * @endcode
	 * @returns 4 parameters: red, green, blue, alpha (in range 0..1)
	 */
	int hsla_to_rgba (lua_State *lua);
} } /* namespace */

namespace ARDOUR { namespace LuaOSC {
	/** OSC transmitter
	 *
	 * A Class to send OSC messages.
	 */
	class Address {
		/*
		 * OSC is kinda special, lo_address is a void* and lo_send() has varags
		 * and typed arguments which makes it hard to bind, even lo_cpp.
		 */
		public:
			/** Construct a new OSC transmitter object
			 * @param uri the destination uri e.g. "osc.udp://localhost:7890"
			 */
			Address (std::string uri) {
				_addr = lo_address_new_from_url (uri.c_str());
			}

			~Address () { if (_addr) { lo_address_free (_addr); } }
			/** Transmit an OSC message
			 *
			 * Path (string) and type (string) must always be given.
			 * The number of following args must match the type.
			 * Supported types are:
			 *
			 *  'i': integer (lua number)
			 *
			 *  'f': float (lua number)
			 *
			 *  'd': double (lua number)
			 *
			 *  'h': 64bit integer (lua number)
			 *
			 *  's': string (lua string)
			 *
			 *  'c': character (lua string)
			 *
			 *  'T': boolean (lua bool) -- this is not implicily True, a lua true/false must be given
			 *
			 *  'F': boolean (lua bool) -- this is not implicily False, a lua true/false must be given
			 *
			 * @param lua: lua arguments: path, types, ...
			 * @returns boolean true if successful, false on error.
			 */
			int send (lua_State *lua);
		private:
			lo_address _addr;
	};

}

class LuaTableRef {
	public:
		LuaTableRef ();
		~LuaTableRef ();

		int get (lua_State* L);
		int set (lua_State* L);

	private:
		struct LuaTableEntry {
			LuaTableEntry (int kt, int vt)
				: keytype (kt)
				, valuetype (vt)
			{ }

			int keytype;
			std::string k_s;
			unsigned int k_n;

			int valuetype;
			// LUA_TUSERDATA
			const void* c;
			void* p;
			// LUA_TBOOLEAN
			bool b;
			// LUA_TSTRING:
			std::string s;
			// LUA_TNUMBER:
			double n;
		};

		std::vector<LuaTableEntry> _data;

		static void* findclasskey (lua_State *L, const void* key);
		template<typename T>
		static void assign (luabridge::LuaRef* rv, T key, const LuaTableEntry& s);
};

} /* namespace */

#endif // _ardour_lua_api_h_
