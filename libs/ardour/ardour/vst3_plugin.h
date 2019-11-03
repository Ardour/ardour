/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_vst3_plugin_h_
#define _ardour_vst3_plugin_h_

#include "pbd/signals.h"
#include "vst3/vst3.h"

#include "ardour/plugin.h"

namespace ARDOUR {

class LIBARDOUR_API VST3Plugin : public ARDOUR::Plugin
{
public:
	VST3Plugin (AudioEngine&, Session&, std::string const&);
	VST3Plugin (const VST3Plugin&);
	~VST3Plugin ();

	std::string unique_id () const { return get_info ()->unique_id; }
	const char* name ()      const { return get_info ()->name.c_str (); }
	const char* label ()     const { return get_info ()->name.c_str (); }
	const char* maker ()     const { return get_info ()->creator.c_str (); }

	uint32_t parameter_count () const;
	float    default_value (uint32_t port);
	void     set_parameter (uint32_t port, float val);
	float    get_parameter (uint32_t port) const;
	int      get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t nth_parameter (uint32_t port, bool& ok) const;

	bool parameter_is_audio (uint32_t) const;
	bool parameter_is_control (uint32_t) const;
	bool parameter_is_input (uint32_t) const;
	bool parameter_is_output (uint32_t) const;

	std::set<Evoral::Parameter> automatable () const;
	std::string describe_parameter (Evoral::Parameter);

	std::string state_node_name () const { return "vst3"; }

	void add_state (XMLNode*) const;
	int  set_state (const XMLNode&, int version);

	bool        load_preset (PresetRecord);
	std::string do_save_preset (std::string);
	void        do_remove_preset (std::string);

	void activate ();
	void deactivate ();
	void cleanup ();

	int set_block_size (pframes_t);

	int connect_and_run (BufferSet&  bufs,
	                     samplepos_t start, samplepos_t end, double speed,
	                     ChanMapping const& in, ChanMapping const& out,
	                     pframes_t nframes, samplecnt_t offset);

	bool has_editor () const;

	bool configure_io (ChanCount in, ChanCount out);

private:
	samplecnt_t plugin_latency () const;
	void        find_presets ();
};

class LIBARDOUR_API VST3PluginInfo : public PluginInfo
{
public:
	VST3PluginInfo ();
	~VST3PluginInfo (){};

	PluginPtr                         load (Session& session);
	std::vector<Plugin::PresetRecord> get_presets (bool user_only) const;
};

} // namespace ARDOUR
#endif
