/*
    Copyright (C) 2016 Robin Gareus <robin@gareus.org>
    Copyright (C) 2006 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_luaproc_h__
#define __ardour_luaproc_h__

#include <set>
#include <vector>
#include <string>

#include "pbd/reallocpool.h"
#include "pbd/stateful.h"

#include "ardour/types.h"
#include "ardour/plugin.h"
#include "ardour/luascripting.h"
#include "ardour/dsp_filter.h"

#include "lua/luastate.h"

namespace luabridge {
	class LuaRef;
}

namespace ARDOUR {

class LIBARDOUR_API LuaProc : public ARDOUR::Plugin {
public:
	LuaProc (AudioEngine&, Session&, const std::string&);
	LuaProc (const LuaProc &);
	~LuaProc ();

	/* Plugin interface */

	std::string unique_id() const { return get_info()->unique_id; }
	const char* name()  const { return get_info()->name.c_str(); }
	const char* label() const { return get_info()->name.c_str(); }
	const char* maker() const { return get_info()->creator.c_str(); }

	uint32_t    parameter_count() const;
	float       default_value (uint32_t port);
	void        set_parameter (uint32_t port, float val);
	float       get_parameter (uint32_t port) const;
	int         get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t    nth_parameter (uint32_t port, bool& ok) const;

	std::string get_docs () const { return _docs; }
	std::string get_parameter_docs (uint32_t) const;

	PluginOutputConfiguration possible_output () const { return _output_configs; }

	std::set<Evoral::Parameter> automatable() const;

	void activate () { }
	void deactivate () { }
	void cleanup () { }

	int set_block_size (pframes_t /*nframes*/) { return 0; }
	framecnt_t  signal_latency() const { return 0; }

	int connect_and_run (BufferSet& bufs,
			ChanMapping in, ChanMapping out,
			pframes_t nframes, framecnt_t offset);

	std::string describe_parameter (Evoral::Parameter);
	void        print_parameter (uint32_t, char*, uint32_t len) const;
	boost::shared_ptr<ScalePoints> get_scale_points(uint32_t port_index) const;

	bool parameter_is_audio (uint32_t) const { return false; }
	bool parameter_is_control (uint32_t) const { return true; }
	bool parameter_is_input (uint32_t) const;
	bool parameter_is_output (uint32_t) const;

	std::string state_node_name() const { return "luaproc"; }
	void add_state (XMLNode *) const;
	int set_state (const XMLNode&, int version);
	int set_script_from_state (const XMLNode&);

	bool load_preset (PresetRecord) { return false; }
	bool has_editor() const { return false; }

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out, ChanCount* imprecise);
	bool configure_io (ChanCount in, ChanCount out);

	ChanCount output_streams() const { return _configured_out; }
	ChanCount input_streams() const { return _configured_in; }

	std::string do_save_preset (std::string) { return ""; }
	void do_remove_preset (std::string) { }

	bool has_inline_display () { return _lua_has_inline_display; }
	void setup_lua_inline_gui (LuaState *lua_gui);

private:
	void find_presets () { }

	/* END Plugin interface */
protected:
	const std::string& script() const { return _script; }

private:
	PBD::ReallocPool _mempool;
	LuaState lua;
	luabridge::LuaRef * _lua_dsp;
	std::string _script;
	std::string _docs;
	bool _lua_does_channelmapping;
	bool _lua_has_inline_display;

	void queue_draw () { QueueDraw(); /* EMIT SIGNAL */ }
	DSP::DspShm* instance_shm () { return &lshm; }
	DSP::DspShm lshm;

	void init ();
	bool load_script ();
	void lua_print (std::string s);

	boost::shared_ptr<ScalePoints> parse_scale_points (luabridge::LuaRef*);

	std::vector<std::pair<bool, int> > _ctrl_params;
	std::map<int, ARDOUR::ParameterDescriptor> _param_desc;
	std::map<int, std::string> _param_doc;

	float* _control_data;
	float* _shadow_data;

	ChanCount _configured_in;
	ChanCount _configured_out;
	PluginOutputConfiguration _output_configs;

	bool _has_midi_input;
	bool _has_midi_output;

#ifdef WITH_LUAPROC_STATS
	int64_t _stats_avg[2];
	int64_t _stats_max[2];
	int64_t _stats_cnt;
#endif
};

class LIBARDOUR_API LuaPluginInfo : public PluginInfo
{
  public:
	LuaPluginInfo (LuaScriptInfoPtr lsi);
	~LuaPluginInfo () { };

	PluginPtr load (Session& session);
	std::vector<Plugin::PresetRecord> get_presets (bool user_only) const;

	bool is_instrument () const { return _is_instrument ; }
	bool reconfigurable_io() const { return true; }

	bool _is_instrument;
};

typedef boost::shared_ptr<LuaPluginInfo> LuaPluginInfoPtr;

} // namespace ARDOUR

#endif // __ardour_luaproc_h__
