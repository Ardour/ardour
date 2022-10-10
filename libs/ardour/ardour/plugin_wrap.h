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

#ifndef __ardour_pluginwrap_h__
#define __ardour_pluginwrap_h__

#include "ardour/types.h"
#include "ardour/plugin.h"

namespace ARDOUR {

class LIBARDOUR_API PluginWrap : public ARDOUR::Plugin {

public:
	LuaProc (AudioEngine&, Session&, const std::string&);
	LuaProc (const LuaProc &);

	std::string state_node_name() const { return "plugwrap"; }
	void add_state (XMLNode *) const;
	int set_state (const XMLNode&, int version);

	std::string unique_id() const { return get_info()->unique_id; }
	const char* name()  const { return get_info()->name.c_str(); }
	const char* label() const { return get_info()->name.c_str(); }
	const char* maker() const { return get_info()->creator.c_str(); }

	uint32_t    parameter_count() const;
	float       default_value (uint32_t port);
	void        set_parameter (uint32_t port, float val, sampleoffset_t);
	float       get_parameter (uint32_t port) const;
	int         get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t    nth_parameter (uint32_t port, bool& ok) const;

	std::string get_docs () const { return _docs; }
	std::string get_parameter_docs (uint32_t) const;

	void activate () { }
	void deactivate () { }
	void cleanup () { }

	int set_block_size (pframes_t /*nframes*/) { return 0; }
	samplecnt_t signal_latency() const { return _signal_latency; }

	int connect_and_run (BufferSet& bufs,
			samplepos_t start, samplepos_t end, double speed,
			ChanMapping in, ChanMapping out,
			pframes_t nframes, samplecnt_t offset);

	std::set<Evoral::Parameter> automatable() const;

	std::string describe_parameter (Evoral::Parameter);
	boost::shared_ptr<ScalePoints> get_scale_points(uint32_t port_index) const;

	bool parameter_is_audio (uint32_t) const { return false; }
	bool parameter_is_control (uint32_t) const { return true; }
	bool parameter_is_input (uint32_t) const;
	bool parameter_is_output (uint32_t) const;

	uint32_t designated_bypass_port ();

	bool load_preset (PresetRecord);
	std::string do_save_preset (std::string);
	void do_remove_preset (std::string);

	bool has_editor() const { return false; }

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out, ChanCount* imprecise);
	bool configure_io (ChanCount in, ChanCount out);

	ChanCount output_streams() const { return _configured_out; }
	ChanCount input_streams() const { return _configured_in; }

private:
	void find_presets ();

};

class LIBARDOUR_API PluginWrapInfo : public PluginInfo
{
public:
	PluginWrapInfo (LuaScriptInfoPtr lsi);

	PluginPtr load (Session& session);
	std::vector<Plugin::PresetRecord> get_presets (bool user_only) const;
};

typedef boost::shared_ptr<PluginWrapInfo> PluginWrapInfoPtr;

} // namespace ARDOUR

#endif
