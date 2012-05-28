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

#ifndef __ardour_sg_plugin_h__
#define __ardour_sg_plugin_h__

#include <boost/shared_ptr.hpp>

#include "ardour/types.h"
#include "ardour/plugin.h"

namespace ARDOUR {

class AudioEngine;

class SoundGridPluginInfo : public PluginInfo {
  public:
    SoundGridPluginInfo ();
    ~SoundGridPluginInfo();

    PluginPtr load (Session&);
};

class SoundGridPlugin : public ARDOUR::Plugin
{
  public:
    SoundGridPlugin (AudioEngine&, Session&);
    ~SoundGridPlugin ();

    std::string unique_id() const;
    const char * label() const;
    const char * name() const;
    const char * maker() const;
    uint32_t parameter_count () const;
    float default_value (uint32_t port);
    float get_parameter(uint32_t which) const;

    int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
    uint32_t nth_parameter (uint32_t which, bool& ok) const;
    void activate ();
    void deactivate ();

    int set_block_size (pframes_t nframes);

    std::set<Evoral::Parameter> automatable() const;
    std::string describe_parameter (Evoral::Parameter);
    std::string state_node_name() const;
    void print_parameter (uint32_t, char*, uint32_t len) const;

    bool parameter_is_audio(uint32_t) const;
    bool parameter_is_control(uint32_t) const;
    bool parameter_is_input(uint32_t) const;
    bool parameter_is_output(uint32_t) const;

    std::string do_save_preset (std::string);
    void do_remove_preset (std::string);
    bool has_editor () const;
    framecnt_t signal_latency() const;

  protected:
    void find_presets ();
    void add_state (XMLNode *) const;
};

} // namespace ARDOUR

#endif /* __ardour_sg_rack_h__ */
