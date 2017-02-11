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
#include <vamp-hostsdk/Plugin.h>

#include "evoral/Note.hpp"

#include "ardour/libardour_visibility.h"

#include "ardour/processor.h"
#include "ardour/session.h"

namespace ARDOUR {
	class Readable;
}

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

	/** get a plugin control parameter value
	 *
	 * @param proc Plugin-Processor
	 * @param which control port to set (starting at 0, including ports of type input and output))
	 * @param ok boolean variable contains true or false after call returned. to be checked by caller before using value.
	 * @returns value
	 */
	float get_processor_param (boost::shared_ptr<Processor> proc, uint32_t which, bool &ok);

	/** reset a processor to its default values (only works for plugins )
	 *
	 * This is a wrapper which looks up the Processor by plugin-insert.
	 *
	 * @param proc Plugin-Insert
	 * @returns true on success, false when the processor is not a plugin
	 */
	bool reset_processor_to_default (boost::shared_ptr<Processor> proc);

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

	/** get a plugin control parameter value
	 *
	 * @param proc Plugin-Insert
	 * @param which control port to query (starting at 0, including ports of type input and output)
	 * @param ok boolean variable contains true or false after call returned. to be checked by caller before using value.
	 * @returns value
	 */
	float get_plugin_insert_param (boost::shared_ptr<ARDOUR::PluginInsert> pi, uint32_t which, bool &ok);

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

	/**
	 * Creates a filename from a series of elements using the correct separator for filenames.
	 *
	 * No attempt is made to force the resulting filename to be an absolute path.
	 * If the first element is a relative path, the result will be a relative path.
	 */
	int build_filename (lua_State *lua);

	/**
	 * Generic conversion from audio sample count to timecode.
	 * (TimecodeType, sample-rate, sample-pos)
	 */
	int sample_to_timecode (lua_State *L);

	/**
	 * Generic conversion from timecode to audio sample count.
	 * (TimecodeType, sample-rate, hh, mm, ss, ff)
	 */
	int timecode_to_sample (lua_State *L);

	/**
	 * Use current session settings to convert
	 * audio-sample count into hh, mm, ss, ff
	 * timecode (this include session pull up/down).
	 */
	int sample_to_timecode_lua (lua_State *L);

	/**
	 * Use current session settings to convert
	 * timecode (hh, mm, ss, ff) to audio-sample
	 * count (this include session pull up/down).
	 */
	int timecode_to_sample_lua (lua_State *L);

	class Vamp {
	/** Vamp Plugin Interface
	 *
	 * Vamp is an audio processing plugin system for plugins that extract descriptive information
	 * from audio data - typically referred to as audio analysis plugins or audio feature
	 * extraction plugins.
	 *
	 * This interface allows to load a plugins and directly access it using the Vamp Plugin API.
	 *
	 * A convenience method is provided to analyze Ardour::Readable objects (Regions).
	 */
		public:
			Vamp (const std::string&, float sample_rate);
			~Vamp ();

			/** Search for all available available Vamp plugins.
			 * @returns list of plugin-keys
			 */
			static std::vector<std::string> list_plugins ();

			::Vamp::Plugin* plugin () { return _plugin; }

			/** high-level abstraction to process a single channel of the given Readable.
			 *
			 * If the plugin is not yet initialized, initialize() is called.
			 *
			 * if @cb is not nil, it is called with the immediate
			 * Vamp::Plugin::Features on every process call.
			 *
			 * @param r readable
			 * @param channel channel to process
			 * @param fn lua callback function
			 * @return 0 on success
			 */
			int analyze (boost::shared_ptr<ARDOUR::Readable> r, uint32_t channel, luabridge::LuaRef fn);

			/** call plugin():reset() and clear intialization flag */
			void reset ();

			/** initialize the plugin for use with analyze().
			 *
			 * This is equivalent to plugin():initialise (1, ssiz, bsiz)
			 * and prepares a plugin for analyze.
			 * (by preferred step and block sizes are used. if the plugin
			 * does not specify them or they're larger than 8K, both are set to 1024)
			 *
			 * Manual initialization is only required to set plugin-parameters
			 * which depend on prior initialization of the plugin.
			 *
			 * @code
			 * vamp:reset ()
			 * vamp:initialize ()
			 * vamp:plugin():setParameter (0, 1.5, nil)
			 * vamp:analyze (r, 0)
			 * @endcode
			 */
			bool initialize ();

			bool initialized () const { return _initialized; }

			/** process given array of audio-samples.
			 *
			 * This is a lua-binding for vamp:plugin():process ()
			 *
			 * @param d audio-data, the vector must match the configured channel count
			 *    and hold a complete buffer for every channel as set during
			 *    plugin():initialise()
			 * @param rt timestamp matching the provided buffer.
			 * @returns features extracted from that data (if the plugin is causal)
			 */
			::Vamp::Plugin::FeatureSet process (const std::vector<float*>& d, ::Vamp::RealTime rt);

		private:
			::Vamp::Plugin* _plugin;
			float           _sample_rate;
			framecnt_t      _bufsize;
			framecnt_t      _stepsize;
			bool            _initialized;

	};

	boost::shared_ptr<Evoral::Note<Evoral::Beats> >
		new_noteptr (uint8_t, Evoral::Beats, Evoral::Beats, uint8_t, uint8_t);

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
