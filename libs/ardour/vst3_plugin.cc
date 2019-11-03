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

#include "pbd/error.h"
#include "ardour/vst3_plugin.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;

VST3Plugin::VST3Plugin (AudioEngine& engine, Session& session, std::string const&)
	: Plugin (engine, session)
{
}

VST3Plugin::VST3Plugin (const VST3Plugin& other)
	: Plugin (other)
{
}

VST3Plugin::~VST3Plugin ()
{
}

uint32_t
VST3Plugin::parameter_count () const
{
	return 0;
}

float
VST3Plugin::default_value (uint32_t port)
{
	return 0;
}

void
VST3Plugin::set_parameter (uint32_t port, float val)
{
}

float
VST3Plugin::get_parameter (uint32_t port) const
{
	return 0.f;
}

int
VST3Plugin::get_parameter_descriptor (uint32_t port, ParameterDescriptor& desc) const
{
	return 0;
}

uint32_t
VST3Plugin::nth_parameter (uint32_t port, bool& ok) const
{
	ok = false;
	return 0;
}

bool
VST3Plugin::parameter_is_audio (uint32_t port) const
{
	return false;
}

bool
VST3Plugin::parameter_is_control (uint32_t port) const
{
	return false;
}

bool
VST3Plugin::parameter_is_input (uint32_t port) const
{
	return false;
}

bool
VST3Plugin::parameter_is_output (uint32_t port) const
{
	return false;
}

std::set<Evoral::Parameter>
VST3Plugin::automatable () const
{
	std::set<Evoral::Parameter> automatables;
	return automatables;
}

std::string
VST3Plugin::describe_parameter (Evoral::Parameter param)
{
	return "??";
}

bool
VST3Plugin::has_editor () const
{
	return false;
}

/* ****************************************************************************/

void
VST3Plugin::add_state (XMLNode* root) const
{
}

int
VST3Plugin::set_state (const XMLNode& node, int version)
{
	return Plugin::set_state (node, version);
}

/* ****************************************************************************/

int
VST3Plugin::set_block_size (pframes_t)
{
	return 0;
}

samplecnt_t
VST3Plugin::plugin_latency () const
{
	return 0;
}

void
VST3Plugin::activate ()
{
}

void
VST3Plugin::deactivate ()
{
}

void
VST3Plugin::cleanup ()
{
}

bool
VST3Plugin::configure_io (ChanCount in, ChanCount out)
{
	return Plugin::configure_io (in, out);
}

int
VST3Plugin::connect_and_run (BufferSet&  bufs,
                             samplepos_t start, samplepos_t end, double speed,
                             ChanMapping const& in_map, ChanMapping const& out_map,
                             pframes_t nframes, samplecnt_t offset)
{
	return 0;
}

/* ****************************************************************************/

bool
VST3Plugin::load_preset (PresetRecord r)
{
	return false;
}

std::string
VST3Plugin::do_save_preset (std::string name)
{
	return "";
}

void
VST3Plugin::do_remove_preset (std::string name)
{
}

void
VST3Plugin::find_presets ()
{
}

/* ****************************************************************************/

VST3PluginInfo::VST3PluginInfo ()
{
	type = ARDOUR::VST3;
}

PluginPtr
VST3PluginInfo::load (Session& session)
{
	return PluginPtr ();
}

std::vector<Plugin::PresetRecord>
VST3PluginInfo::get_presets (bool /*user_only*/) const
{
	std::vector<Plugin::PresetRecord> p;
	return p;
}
