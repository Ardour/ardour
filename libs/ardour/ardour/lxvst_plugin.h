/*
    Copyright (C) 2004 Paul Davis

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

#ifndef __ardour_lxvst_plugin_h__
#define __ardour_lxvst_plugin_h__

#include <list>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <dlfcn.h>

#include "pbd/stateful.h"
#include "ardour/plugin.h"

struct _VSTFXHandle;
struct _VSTFX;
typedef struct _VSTFXHandle VSTFXHandle;
typedef struct _VSTFX VSTFX;
struct _AEffect;
typedef struct _AEffect AEffect;

namespace ARDOUR {
class AudioEngine;
class Session;

class LXVSTPlugin : public ARDOUR::Plugin
{
  public:
	LXVSTPlugin (ARDOUR::AudioEngine&, ARDOUR::Session&, VSTFXHandle* handle);
	LXVSTPlugin (const LXVSTPlugin &);
	~LXVSTPlugin ();

	/* Plugin interface */

	std::string unique_id() const;
	const char * label() const;
	const char * name() const;
	const char * maker() const;
	uint32_t parameter_count() const;
	float default_value (uint32_t port);
	framecnt_t signal_latency() const;
	void set_parameter (uint32_t port, float val);
	float get_parameter (uint32_t port) const;
	int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	std::set<Evoral::Parameter> automatable() const;
	uint32_t nth_parameter (uint32_t port, bool& ok) const;
	void activate ();
	void deactivate ();
	int set_block_size (pframes_t);

	int connect_and_run (BufferSet&,
			ChanMapping in, ChanMapping out,
			pframes_t nframes, framecnt_t offset);

	std::string describe_parameter (Evoral::Parameter);
	std::string state_node_name() const { return "lxvst"; }
	void print_parameter (uint32_t, char*, uint32_t len) const;

	bool parameter_is_audio(uint32_t) const { return false; }
	bool parameter_is_control(uint32_t) const { return true; }
	bool parameter_is_input(uint32_t) const { return true; }
	bool parameter_is_output(uint32_t) const { return false; }

	bool load_preset (PresetRecord);
	int first_user_preset_index () const;

	bool has_editor () const;

	int set_state (XMLNode const &, int);

	AEffect * plugin () const { return _plugin; }
	VSTFX * vstfx () const { return _vstfx; }

private:

	void do_remove_preset (std::string name);
	std::string do_save_preset (std::string name);
	gchar* get_chunk (bool) const;
	int set_chunk (gchar const *, bool);
	XMLTree * presets_tree () const;
	std::string presets_file () const;
	void find_presets ();
	bool load_user_preset (PresetRecord);
	bool load_plugin_preset (PresetRecord);
	void add_state (XMLNode *) const;

	VSTFXHandle* handle;
	VSTFX*      _vstfx;
	AEffect*    _plugin;
	bool         been_resumed;
};

class LXVSTPluginInfo : public PluginInfo
{
  public:
	LXVSTPluginInfo ();
	~LXVSTPluginInfo () {}

	PluginPtr load (Session& session);
};

typedef boost::shared_ptr<LXVSTPluginInfo> LXVSTPluginInfoPtr;

} // namespace ARDOUR

#endif /* __ardour_lxvst_plugin_h__ */
