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
	bool set_processor_param (boost::shared_ptr<Processor> proc, uint32_t which, float val);
	/** set a plugin control-input parameter value
	 *
	 * This is a wrapper around set_processor_param which looks up the Processor by plugin-insert.
	 *
	 * @param proc Plugin-Insert
	 * @param which control-input to set (starting at 0)
	 * @param value value to set
	 * @returns true on success, false on error or out-of-bounds value
	 */
	bool set_plugin_insert_param (boost::shared_ptr<PluginInsert> pi, uint32_t which, float val);

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

} } /* namespace */

#endif // _ardour_lua_api_h_
