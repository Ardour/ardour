/*
    Copyright (C) 2000-2002 Paul Davis

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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <vector>
#include <string>

#include <cstdlib>
#include <cstdio> // so libraptor doesn't complain
#include <cmath>
#ifndef COMPILER_MSVC
#include <dirent.h>
#endif
#include <sys/stat.h>
#include <cerrno>
#include <utility>

#ifdef HAVE_LRDF
#include <lrdf.h>
#endif

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/xml++.h"

#include "ardour/buffer_set.h"
#include "ardour/chan_count.h"
#include "ardour/chan_mapping.h"
#include "ardour/data_type.h"
#include "ardour/luaproc.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/plugin.h"
#include "ardour/plugin_manager.h"
#include "ardour/port.h"
#include "ardour/session.h"
#include "ardour/types.h"

#ifdef AUDIOUNIT_SUPPORT
#include "ardour/audio_unit.h"
#endif

#ifdef LV2_SUPPORT
#include "ardour/lv2_plugin.h"
#endif

#include "pbd/stl_delete.h"

#include "pbd/i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace ARDOUR { class AudioEngine; }

#ifdef NO_PLUGIN_STATE
static bool seen_get_state_message = false;
static bool seen_set_state_message = false;
#endif

PBD::Signal2<void, std::string, Plugin*> Plugin::PresetsChanged;

bool
PluginInfo::needs_midi_input () const
{
	return (n_inputs.n_midi() != 0);
}

bool
PluginInfo::is_instrument () const
{
	return (n_inputs.n_midi() != 0) && (n_outputs.n_audio() > 0) && (n_inputs.n_audio() == 0);
}

Plugin::Plugin (AudioEngine& e, Session& s)
	: _engine (e)
	, _session (s)
	, _cycles (0)
	, _owner (0)
	, _have_presets (false)
	, _have_pending_stop_events (false)
	, _parameter_changed_since_last_preset (false)
{
	_pending_stop_events.ensure_buffers (DataType::MIDI, 1, 4096);
}

Plugin::Plugin (const Plugin& other)
	: StatefulDestructible()
	, Latent()
	, _engine (other._engine)
	, _session (other._session)
	, _info (other._info)
	, _cycles (0)
	, _owner (other._owner)
	, _have_presets (false)
	, _have_pending_stop_events (false)
	, _parameter_changed_since_last_preset (false)
{
	_pending_stop_events.ensure_buffers (DataType::MIDI, 1, 4096);
}

Plugin::~Plugin ()
{
}

void
Plugin::remove_preset (string name)
{
	Plugin::PresetRecord const * p = preset_by_label (name);
	if (!p) {
		PBD::error << _("Trying to remove nonexistent preset.") << endmsg;
		return;
	}
	if (!p->user) {
		PBD::error << _("Cannot remove plugin factory preset.") << endmsg;
		return;
	}

	do_remove_preset (name);
	_presets.erase (p->uri);

	_last_preset.uri = "";
	_parameter_changed_since_last_preset = false;
	_have_presets = false;
	PresetsChanged (unique_id(), this); /* EMIT SIGNAL */
	PresetRemoved (); /* EMIT SIGNAL */
}

/** @return PresetRecord with empty URI on failure */
Plugin::PresetRecord
Plugin::save_preset (string name)
{
	if (preset_by_label (name)) {
		PBD::error << _("Preset with given name already exists.") << endmsg;
		return Plugin::PresetRecord ();
	}

	string const uri = do_save_preset (name);

	if (!uri.empty()) {
		_presets.insert (make_pair (uri, PresetRecord (uri, name)));
		_have_presets = false;
		PresetsChanged (unique_id(), this); /* EMIT SIGNAL */
		PresetAdded (); /* EMIT SIGNAL */
	}

	return PresetRecord (uri, name);
}

PluginPtr
ARDOUR::find_plugin(Session& session, string identifier, PluginType type)
{
	PluginManager& mgr (PluginManager::instance());
	PluginInfoList plugs;

	switch (type) {
	case ARDOUR::Lua:
		plugs = mgr.lua_plugin_info();
		break;

	case ARDOUR::LADSPA:
		plugs = mgr.ladspa_plugin_info();
		break;

#ifdef LV2_SUPPORT
	case ARDOUR::LV2:
		plugs = mgr.lv2_plugin_info();
		break;
#endif

#ifdef WINDOWS_VST_SUPPORT
	case ARDOUR::Windows_VST:
		plugs = mgr.windows_vst_plugin_info();
		break;
#endif

#ifdef LXVST_SUPPORT
	case ARDOUR::LXVST:
		plugs = mgr.lxvst_plugin_info();
		break;
#endif

#ifdef MACVST_SUPPORT
	case ARDOUR::MacVST:
		plugs = mgr.mac_vst_plugin_info();
		break;
#endif

#ifdef AUDIOUNIT_SUPPORT
	case ARDOUR::AudioUnit:
		plugs = mgr.au_plugin_info();
		break;
#endif

	default:
		return PluginPtr ((Plugin *) 0);
	}

	PluginInfoList::iterator i;

	for (i = plugs.begin(); i != plugs.end(); ++i) {
		if (identifier == (*i)->unique_id){
			return (*i)->load (session);
		}
	}

#ifdef WINDOWS_VST_SUPPORT
	/* hmm, we didn't find it. could be because in older versions of Ardour.
	   we used to store the name of a VST plugin, not its unique ID. so try
	   again.
	*/

	for (i = plugs.begin(); i != plugs.end(); ++i) {
		if (identifier == (*i)->name){
			return (*i)->load (session);
		}
	}
#endif

#ifdef LXVST_SUPPORT
	/* hmm, we didn't find it. could be because in older versions of Ardour.
	   we used to store the name of a VST plugin, not its unique ID. so try
	   again.
	*/

	for (i = plugs.begin(); i != plugs.end(); ++i) {
		if (identifier == (*i)->name){
			return (*i)->load (session);
		}
	}
#endif

	return PluginPtr ((Plugin*) 0);
}

ChanCount
Plugin::output_streams () const
{
	/* LADSPA & VST should not get here because they do not
	   return "infinite" i/o counts.
	*/
	return ChanCount::ZERO;
}

ChanCount
Plugin::input_streams () const
{
	/* LADSPA & VST should not get here because they do not
	   return "infinite" i/o counts.
	*/
	return ChanCount::ZERO;
}

Plugin::IOPortDescription
Plugin::describe_io_port (ARDOUR::DataType dt, bool input, uint32_t id) const
{
	std::stringstream ss;
	switch (dt) {
		case DataType::AUDIO:
			ss << _("Audio") << " ";
			break;
		case DataType::MIDI:
			ss << _("Midi") << " ";
			break;
		default:
			ss << _("?") << " ";
			break;
	}
	if (input) {
		ss << _("In") << " ";
	} else {
		ss << _("Out") << " ";
	}

	ss << (id + 1);

	Plugin::IOPortDescription iod (ss.str());
	return iod;
}

PluginOutputConfiguration
Plugin::possible_output () const
{
	PluginOutputConfiguration oc;
	if (_info) {
		oc.insert (_info->n_outputs.n_audio ());
	}
	return oc;
}

const Plugin::PresetRecord *
Plugin::preset_by_label (const string& label)
{
#ifndef NO_PLUGIN_STATE
	if (!_have_presets) {
		find_presets ();
		_have_presets = true;
	}
#endif
	// FIXME: O(n)
	for (map<string, PresetRecord>::const_iterator i = _presets.begin(); i != _presets.end(); ++i) {
		if (i->second.label == label) {
			return &i->second;
		}
	}

	return 0;
}

const Plugin::PresetRecord *
Plugin::preset_by_uri (const string& uri)
{
#ifndef NO_PLUGIN_STATE
	if (!_have_presets) {
		find_presets ();
		_have_presets = true;
	}
#endif
	map<string, PresetRecord>::const_iterator pr = _presets.find (uri);
	if (pr != _presets.end()) {
		return &pr->second;
	} else {
		return 0;
	}
}

int
Plugin::connect_and_run (BufferSet& bufs,
		framepos_t /*start*/, framepos_t /*end*/, double /*speed*/,
		ChanMapping /*in_map*/, ChanMapping /*out_map*/,
		pframes_t /* nframes */, framecnt_t /*offset*/)
{
	if (bufs.count().n_midi() > 0) {

		/* Track notes that we are sending to the plugin */

		const MidiBuffer& b = bufs.get_midi (0);

		_tracker.track (b.begin(), b.end());

		if (_have_pending_stop_events) {
			/* Transmit note-offs that are pending from the last transport stop */
			bufs.merge_from (_pending_stop_events, 0);
			_have_pending_stop_events = false;
		}
	}

	return 0;
}

void
Plugin::realtime_handle_transport_stopped ()
{
	resolve_midi ();
}

void
Plugin::realtime_locate ()
{
	resolve_midi ();
}

void
Plugin::monitoring_changed ()
{
	resolve_midi ();
}

void
Plugin::resolve_midi ()
{
	/* Create note-offs for any active notes and put them in _pending_stop_events, to be picked
	   up on the next call to connect_and_run ().
	*/

	_pending_stop_events.get_midi(0).clear ();
	_tracker.resolve_notes (_pending_stop_events.get_midi (0), 0);
	_have_pending_stop_events = true;
}

vector<Plugin::PresetRecord>
Plugin::get_presets ()
{
	vector<PresetRecord> p;

#ifndef NO_PLUGIN_STATE
	if (!_have_presets) {
		find_presets ();
		_have_presets = true;
	}

	for (map<string, PresetRecord>::const_iterator i = _presets.begin(); i != _presets.end(); ++i) {
		p.push_back (i->second);
	}
#else
	if (!seen_set_state_message) {
		info << string_compose (_("Plugin presets are not supported in this build of %1. Consider paying for a full version"),
					PROGRAM_NAME)
		     << endmsg;
		seen_set_state_message = true;
	}
#endif

	return p;
}

/** Set parameters using a preset */
bool
Plugin::load_preset (PresetRecord r)
{
	_last_preset = r;
	_parameter_changed_since_last_preset = false;

	_session.set_dirty ();
	PresetLoaded (); /* EMIT SIGNAL */
	return true;
}

void
Plugin::clear_preset ()
{
	_last_preset.uri = "";
	_last_preset.label = "";
	_parameter_changed_since_last_preset = false;

	_session.set_dirty ();
	PresetLoaded (); /* EMIT SIGNAL */
}

void
Plugin::set_parameter (uint32_t /* which */, float /* value */)
{
	_parameter_changed_since_last_preset = true;
	PresetDirty (); /* EMIT SIGNAL */
}

void
Plugin::parameter_changed_externally (uint32_t which, float /* value */)
{
	_parameter_changed_since_last_preset = true;
	_session.set_dirty ();
	ParameterChangedExternally (which, get_parameter (which)); /* EMIT SIGNAL */
	PresetDirty (); /* EMIT SIGNAL */
}

int
Plugin::set_state (const XMLNode& node, int /*version*/)
{
	node.get_property (X_("last-preset-uri"), _last_preset.uri);
	node.get_property (X_("last-preset-label"), _last_preset.label);
	node.get_property (X_("parameter-changed-since-last-preset"), _parameter_changed_since_last_preset);
	return 0;
}

XMLNode &
Plugin::get_state ()
{
	XMLNode* root = new XMLNode (state_node_name ());
	LocaleGuard lg;

	root->set_property (X_("last-preset-uri"), _last_preset.uri);
	root->set_property (X_("last-preset-label"), _last_preset.label);
	root->set_property (X_("parameter-changed-since-last-preset"), _parameter_changed_since_last_preset);

#ifndef NO_PLUGIN_STATE
	add_state (root);
#else
	if (!seen_get_state_message) {
		info << string_compose (_("Saving plugin settings is not supported in this build of %1. Consider paying for the full version"),
					PROGRAM_NAME)
		     << endmsg;
		seen_get_state_message = true;
	}
#endif

	return *root;
}

void
Plugin::set_info (PluginInfoPtr info)
{
	_info = info;
}


