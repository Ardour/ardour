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
	, presets (other.presets)
{
}

Plugin::~Plugin ()
{
}

const Plugin::PresetRecord*
Plugin::preset_by_label(const string& label)
{
	// FIXME: O(n)
	for (map<string,PresetRecord>::const_iterator i = presets.begin(); i != presets.end(); ++i) {
		if (i->second.label == label) {
			return &i->second;
		}
	}
	return NULL;
}

const Plugin::PresetRecord*
Plugin::preset_by_uri(const string& uri)
{
	map<string,PresetRecord>::const_iterator pr = presets.find(uri);
	if (pr != presets.end()) {
		return &pr->second;
	} else {
		return NULL;
	}
}

vector<Plugin::PresetRecord>
Plugin::get_presets()
{
	vector<PresetRecord> result;
	uint32_t id;
	std::string unique (unique_id());

	/* XXX problem: AU plugins don't have numeric ID's.
	   Solution: they have a different method of providing presets.
	   XXX sub-problem: implement it.
	*/

	if (!isdigit (unique[0])) {
		return result;
	}

	id = atol (unique.c_str());

	lrdf_uris* set_uris = lrdf_get_setting_uris(id);

	if (set_uris) {
		for (uint32_t i = 0; i < (uint32_t) set_uris->count; ++i) {
			if (char* label = lrdf_get_label(set_uris->items[i])) {
				PresetRecord rec(set_uris->items[i], label);
				result.push_back(rec);
				presets.insert(std::make_pair(set_uris->items[i], rec));
			}
		}
		lrdf_free_uris(set_uris);
	}

	return result;
}

bool
Plugin::load_preset(const string preset_uri)
{
	lrdf_defaults* defs = lrdf_get_setting_values(preset_uri.c_str());

	if (defs) {
		for (uint32_t i = 0; i < (uint32_t) defs->count; ++i) {
			// The defs->items[i].pid < defs->count check is to work around
			// a bug in liblrdf that saves invalid values into the presets file.
			if (((uint32_t) defs->items[i].pid < (uint32_t) defs->count) && parameter_is_input (defs->items[i].pid)) {
				set_parameter(defs->items[i].pid, defs->items[i].value);
			}
		}
		lrdf_free_setting_values(defs);
	}

	return true;
}

bool
Plugin::save_preset (string name, string domain)
{
	lrdf_portvalue portvalues[parameter_count()];
	lrdf_defaults defaults;
	uint32_t id;
	std::string unique (unique_id());

	/* XXX problem: AU plugins don't have numeric ID's.
	   Solution: they have a different method of providing/saving presets.
	   XXX sub-problem: implement it.
	*/

	if (!isdigit (unique[0])) {
		return false;
	}

	id = atol (unique.c_str());

	defaults.count = parameter_count();
	defaults.items = portvalues;

	for (uint32_t i = 0; i < parameter_count(); ++i) {
		if (parameter_is_input (i)) {
			portvalues[i].pid = i;
			portvalues[i].value = get_parameter(i);
		}
	}

	char* envvar;
	if ((envvar = getenv ("HOME")) == 0) {
		warning << _("Could not locate HOME.  Preset not saved.") << endmsg;
		return false;
	}

	string source(string_compose("file:%1/.%2/rdf/ardour-presets.n3", envvar, domain));

	char* uri = lrdf_add_preset (source.c_str(), name.c_str(), id, &defaults);
	
	/* XXX: why is the uri apparently kept as the key in the `presets' map and also in the PresetRecord? */

	presets.insert (make_pair (uri, PresetRecord (uri, name)));
	free (uri);

	string path = string_compose("%1/.%2", envvar, domain);
	if (g_mkdir_with_parents (path.c_str(), 0775)) {
		warning << string_compose(_("Could not create %1.  Preset not saved. (%2)"), path, strerror(errno)) << endmsg;
		return false;
	}

	path += "/rdf";
	if (g_mkdir_with_parents (path.c_str(), 0775)) {
		warning << string_compose(_("Could not create %1.  Preset not saved. (%2)"), path, strerror(errno)) << endmsg;
		return false;
	}

	if (lrdf_export_by_source(source.c_str(), source.substr(5).c_str())) {
		warning << string_compose(_("Error saving presets file %1."), source) << endmsg;
		return false;
	}

	return true;
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


