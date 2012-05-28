/*
    Copyright (C) 2012 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "ardour/ardour.h"
#include "ardour/session.h"
#include "ardour/sg_plugin.h"
#include "ardour/debug.h"

using namespace ARDOUR;
using std::string;

SoundGridPlugin::SoundGridPlugin (AudioEngine& ae, Session& s)
        : Plugin (ae, s)
{
}

SoundGridPlugin::~SoundGridPlugin ()
{
}

SoundGridPluginInfo::SoundGridPluginInfo ()
{
}

std::string
SoundGridPlugin::unique_id() const
{
        return string();
}

const char *
SoundGridPlugin::label() const
{
        return "";
}

const char*
SoundGridPlugin::name() const
{
        return "";
}

const char* 
SoundGridPlugin::maker() const
{
        return "Waves";
}

uint32_t
SoundGridPlugin::parameter_count () const
{
        return 0;
}

float
SoundGridPlugin::default_value (uint32_t)
{
        return 0;
}

float
SoundGridPlugin::get_parameter(uint32_t) const
{
        return 0.0;
}

int
SoundGridPlugin::get_parameter_descriptor (uint32_t, ParameterDescriptor&) const
{
        return 0;
}

uint32_t
SoundGridPlugin::nth_parameter (uint32_t, bool&) const
{
        return 0;
}

void
SoundGridPlugin::activate ()
{
        return;
}

void
SoundGridPlugin::deactivate ()
{
        return;
}

int
SoundGridPlugin::set_block_size (pframes_t)
{
        return 0;
}

std::set<Evoral::Parameter>
SoundGridPlugin::automatable() const
{
        return std::set<Evoral::Parameter> ();
}

std::string
SoundGridPlugin::describe_parameter (Evoral::Parameter)
{
        return string();
}

std::string
SoundGridPlugin::state_node_name() const
{
        return string();
}

void
SoundGridPlugin::print_parameter (uint32_t, char*, uint32_t) const
{
        return;
}


bool
SoundGridPlugin::parameter_is_audio(uint32_t) const
{
        return false;
}

bool
SoundGridPlugin::parameter_is_control(uint32_t) const
{
        return false;
}

bool
SoundGridPlugin::parameter_is_input(uint32_t) const
{
        return false;
}

bool
SoundGridPlugin::parameter_is_output(uint32_t) const
{
        return false;
}

void
SoundGridPlugin::find_presets ()
{
}

void
SoundGridPlugin::add_state (XMLNode *) const
{
}

bool 
SoundGridPlugin::has_editor () const
{
        return true;
}

framecnt_t
SoundGridPlugin::signal_latency() const
{
        return 0;
}

std::string
SoundGridPlugin::do_save_preset (std::string)
{
        return string();
}

void
SoundGridPlugin::do_remove_preset (std::string)
{
}


SoundGridPluginInfo::~SoundGridPluginInfo()
{
}

PluginPtr
SoundGridPluginInfo::load (Session& s)
{
        return boost::shared_ptr<Plugin> (new SoundGridPlugin (s.engine(), s));
}

