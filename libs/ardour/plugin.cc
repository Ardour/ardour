/*
 * Copyright (C) 2000-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2015 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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
#include "ardour/lv2_plugin.h"
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

#include "pbd/stl_delete.h"

#include "pbd/i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace ARDOUR { class AudioEngine; }

PBD::Signal3<void, std::string, Plugin*, bool> Plugin::PresetsChanged;

bool
PluginInfo::needs_midi_input () const
{
	return (n_inputs.n_midi() != 0);
}

Plugin::Plugin (AudioEngine& e, Session& s)
	: _engine (e)
	, _session (s)
	, _cycles (0)
	, _owner (0)
	, _for_impulse_analysis (false)
	, _have_presets (false)
	, _have_pending_stop_events (false)
	, _parameter_changed_since_last_preset (false)
	, _immediate_events(6096) // FIXME: size?
{
	_pending_stop_events.ensure_buffers (DataType::MIDI, 1, 4096);
	PresetsChanged.connect_same_thread(_preset_connection, boost::bind (&Plugin::invalidate_preset_cache, this, _1, _2, _3));
}

Plugin::Plugin (const Plugin& other)
	: StatefulDestructible()
	, HasLatency()
	, _engine (other._engine)
	, _session (other._session)
	, _info (other._info)
	, _cycles (0)
	, _owner (other._owner)
	, _for_impulse_analysis (false)
	, _have_presets (false)
	, _have_pending_stop_events (false)
	, _last_preset (other._last_preset)
	, _parameter_changed_since_last_preset (false)
	, _immediate_events(6096) // FIXME: size?
{
	_pending_stop_events.ensure_buffers (DataType::MIDI, 1, 4096);

	PresetsChanged.connect_same_thread(_preset_connection, boost::bind (&Plugin::invalidate_preset_cache, this, _1, _2, _3));
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
	PresetsChanged (unique_id(), this, false); /* EMIT SIGNAL */
	PresetRemoved (); /* EMIT SIGNAL */
}

Plugin::PresetRecord
Plugin::save_preset (string name)
{
	Plugin::PresetRecord const* p = preset_by_label (name);
	if (p && !p->user) {
		PBD::error << _("A factory presets with given name already exists.") << endmsg;
		return Plugin::PresetRecord ();
	}

	string const uri = do_save_preset (name);

	if (uri.empty()) {
		/* save failed, clean up preset */
		do_remove_preset (name);
		PBD::error << _("Failed to save plugin preset.") << endmsg;
		return Plugin::PresetRecord ();
	}

	if (p) {
		_presets.erase (p->uri);
		_parameter_changed_since_last_preset = false;
	}

	_presets.insert (make_pair (uri, PresetRecord (uri, name)));
	_have_presets = false;
	PresetsChanged (unique_id(), this, true); /* EMIT SIGNAL */
	PresetAdded (); /* EMIT SIGNAL */

	return PresetRecord (uri, name);
}

void
Plugin::invalidate_preset_cache (std::string const& id, Plugin* plugin, bool added)
{
	if (this == plugin || id != unique_id ()) {
		return;
	}

	// TODO: use a shared cache in _info (via PluginInfo::get_presets)

	_presets.clear ();
	_have_presets = false;

	if (added) {
		PresetAdded (); /* EMIT SIGNAL */
	} else {
		PresetRemoved (); /* EMIT SIGNAL */
	}
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

	case ARDOUR::LV2:
		plugs = mgr.lv2_plugin_info();
		break;

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

#ifdef VST3_SUPPORT
	case ARDOUR::VST3:
		plugs = mgr.vst3_plugin_info();
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

#if defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT
	/* hmm, we didn't find it. could be because in older versions of Ardour.
	 * we used to store the name of a VST plugin, not its unique ID. so try
	 * again.
	 */

	if (type == ARDOUR::LXVST || type == ARDOUR::Windows_VST) {
		for (i = plugs.begin(); i != plugs.end(); ++i) {
			if (identifier == (*i)->name){
				return (*i)->load (session);
			}
		}
	}
#endif

#ifdef AUDIOUNIT_SUPPORT
	if (type == ARDOUR::AudioUnit) {
		/* old versions saved three 32bit (really OSType) integers
		 * e.g. 112233-445566-778899 (due to stringstream misinterpreting OSType)
		 * instead of using Apple's UTCreateStringForOSType, which results in
		 * "aaaa - bbbb - cccc"
		 */
		identifier = AUPluginInfo::convert_old_unique_id (identifier);
		for (i = plugs.begin(); i != plugs.end(); ++i) {
			if (identifier == (*i)->unique_id){
				return (*i)->load (session);
			}
		}
	}

#endif

	return PluginPtr ();
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

	std::stringstream gn;
	gn << ss.str();

	ss << (id + 1);
	gn << (id / 2 + 1) << " L/R";

	Plugin::IOPortDescription iod (ss.str());
	iod.group_name = gn.str();
	iod.group_channel = id % 2;
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
	if (!_have_presets) {
		find_presets ();
		_have_presets = true;
	}

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
	if (uri.empty ()) {
		return 0;
	}
	if (!_have_presets) {
		find_presets ();
		_have_presets = true;
	}

	map<string, PresetRecord>::const_iterator pr = _presets.find (uri);
	if (pr != _presets.end()) {
		return &pr->second;
	} else {
		return 0;
	}
}

bool
Plugin::write_immediate_event (Evoral::EventType event_type, size_t size, const uint8_t* buf)
{
	if (!Evoral::midi_event_is_valid (buf, size)) {
		return false;
	}
	return (_immediate_events.write (0, event_type, size, buf) == size);
}

int
Plugin::connect_and_run (BufferSet& bufs,
		samplepos_t /*start*/, samplepos_t /*end*/, double /*speed*/,
		ChanMapping const& /*in_map*/, ChanMapping const& /*out_map*/,
		pframes_t nframes, samplecnt_t /*offset*/)
{
	if (bufs.count().n_midi() > 0) {

		if (_immediate_events.read_space() && nframes > 0) {
			_immediate_events.read (bufs.get_midi (0), 0, 1, nframes - 1, true);
		}

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
Plugin::realtime_locate (bool for_loop_end)
{
	if (!for_loop_end) {
		resolve_midi ();
	}
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

	if (!_have_presets) {
		find_presets ();
		_have_presets = true;
	}

	for (map<string, PresetRecord>::const_iterator i = _presets.begin(); i != _presets.end(); ++i) {
		p.push_back (i->second);
	}

	return p;
}

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
Plugin::set_parameter (uint32_t /* which */, float /* value */, sampleoffset_t /* when */)
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

void
Plugin::state_changed ()
{
	_parameter_changed_since_last_preset = true;
	_session.set_dirty ();
	PresetDirty (); /* EMIT SIGNAL */
}

int
Plugin::set_state (const XMLNode& node, int /*version*/)
{
	std::string preset_uri;
	const Plugin::PresetRecord* r = 0;
	if (node.get_property (X_("last-preset-uri"), preset_uri)) {
		r = preset_by_uri (preset_uri);
	}
	if (r) {
		_last_preset = *r;
		node.get_property (X_("parameter-changed-since-last-preset"), _parameter_changed_since_last_preset); // XXX
	} else {
		_last_preset.uri = "";
		_last_preset.valid = false;
	}
	return 0;
}

XMLNode &
Plugin::get_state ()
{
	XMLNode* root = new XMLNode (state_node_name ());

	root->set_property (X_("last-preset-uri"), _last_preset.uri);
	root->set_property (X_("last-preset-label"), _last_preset.label);
	root->set_property (X_("parameter-changed-since-last-preset"), _parameter_changed_since_last_preset);

	add_state (root);

	return *root;
}

std::string
Plugin::parameter_label (uint32_t which) const
{
	if (which >= parameter_count ()) {
		return "";
	}
	ParameterDescriptor pd;
	get_parameter_descriptor (which, pd);
	return pd.label;
}

bool
PluginInfo::is_effect () const
{
	return (!is_instrument () && !is_utility ()  && !is_analyzer ());
}

bool
PluginInfo::is_instrument () const
{
	if (category == "Instrument") {
		return true;
	}

	// second check: if we have  midi input and audio output, we're likely an instrument
	return (n_inputs.n_midi() != 0) && (n_outputs.n_audio() > 0) && (n_inputs.n_audio() == 0);
}

bool
PluginInfo::is_utility () const
{
	/* XXX beware of translations, e.g. LV2 categories */
	return (category == "Utility" || category == "MIDI" || category == "Generator");
}

bool
PluginInfo::is_analyzer () const
{
	return (category == "Analyser" || category == "Anaylsis" || category == "Analyzer");
}
