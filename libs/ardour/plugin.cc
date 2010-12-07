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
#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>
#include <utility>

#include <lrdf.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/xml++.h"

#include "ardour/ardour.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/plugin.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/plugin_manager.h"

#ifdef HAVE_AUDIOUNITS
#include "ardour/audio_unit.h"
#endif

#ifdef HAVE_SLV2
#include "ardour/lv2_plugin.h"
#endif

#include "pbd/stl_delete.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PBD::Signal0<bool> Plugin::PresetFileExists;

Plugin::Plugin (AudioEngine& e, Session& s)
	: _engine (e)
	, _session (s)
	, _cycles (0)
{
}

Plugin::Plugin (const Plugin& other)
	: StatefulDestructible()
	, Latent()
	, _engine (other._engine)
	, _session (other._session)
	, _info (other._info)
	, _cycles (0)
{
	
}

Plugin::~Plugin ()
{
	
}

void
Plugin::remove_preset (string name)
{
	do_remove_preset (name);
	_presets.erase (preset_by_label (name)->uri);
	PresetRemoved (); /* EMIT SIGNAL */
}

bool
Plugin::save_preset (string name)
{
	string const uri = do_save_preset (name);

	if (!uri.empty()) {
		_presets.insert (make_pair (uri, PresetRecord (uri, name)));
		PresetAdded (); /* EMIT SIGNAL */
	}
	
	return !uri.empty ();
}

PluginPtr
ARDOUR::find_plugin(Session& session, string identifier, PluginType type)
{
	PluginManager *mgr = PluginManager::the_manager();
	PluginInfoList plugs;

	switch (type) {
	case ARDOUR::LADSPA:
		plugs = mgr->ladspa_plugin_info();
		break;

#ifdef HAVE_SLV2
	case ARDOUR::LV2:
		plugs = mgr->lv2_plugin_info();
		break;
#endif

#ifdef VST_SUPPORT
	case ARDOUR::VST:
		plugs = mgr->vst_plugin_info();
		break;
#endif

#ifdef HAVE_AUDIOUNITS
	case ARDOUR::AudioUnit:
		plugs = mgr->au_plugin_info();
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

#ifdef VST_SUPPORT
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

const Plugin::PresetRecord *
Plugin::preset_by_label (const string& label)
{
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
	map<string, PresetRecord>::const_iterator pr = _presets.find (uri);
	if (pr != _presets.end()) {
		return &pr->second;
	} else {
		return 0;
	}
}
