/*
    Copyright (C) 2000-2006 Paul Davis

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

#include <stdint.h>

#include <sys/types.h>
#include <cstdio>
#include <lrdf.h>
#include <dlfcn.h>
#include <cstdlib>
#include <fstream>

#ifdef WINDOWS_VST_SUPPORT
#include "fst.h"
#include "pbd/basename.h"
#include <cstring>
#endif // WINDOWS_VST_SUPPORT

#ifdef LXVST_SUPPORT
#include "ardour/linux_vst_support.h"
#include "pbd/basename.h"
#include <cstring>
#endif //LXVST_SUPPORT

#include <glibmm/miscutils.h>

#include "pbd/pathscanner.h"
#include "pbd/whitespace.h"

#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/ladspa.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/plugin.h"
#include "ardour/plugin_manager.h"
#include "ardour/rc_configuration.h"

#ifdef LV2_SUPPORT
#include "ardour/lv2_plugin.h"
#endif

#ifdef WINDOWS_VST_SUPPORT
#include "ardour/windows_vst_plugin.h"
#endif

#ifdef LXVST_SUPPORT
#include "ardour/lxvst_plugin.h"
#endif

#ifdef AUDIOUNIT_SUPPORT
#include "ardour/audio_unit.h"
#include <Carbon/Carbon.h>
#endif

#include "pbd/error.h"
#include "pbd/stl_delete.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

PluginManager* PluginManager::_instance = 0;

PluginManager&
PluginManager::instance() 
{
	if (!_instance) {
		_instance = new PluginManager;
	}
	return *_instance;
}

PluginManager::PluginManager ()
	: _windows_vst_plugin_info(0)
	, _lxvst_plugin_info(0)
	, _ladspa_plugin_info(0)
	, _lv2_plugin_info(0)
	, _au_plugin_info(0)
{
	char* s;
	string lrdf_path;

	load_statuses ();

	if ((s = getenv ("LADSPA_RDF_PATH"))){
		lrdf_path = s;
	}

	if (lrdf_path.length() == 0) {
		lrdf_path = "/usr/local/share/ladspa/rdf:/usr/share/ladspa/rdf";
	}

	add_lrdf_data(lrdf_path);
	add_ladspa_presets();
#ifdef WINDOWS_VST_SUPPORT
	if (Config->get_use_windows_vst ()) {
		add_windows_vst_presets ();
	}
#endif /* WINDOWS_VST_SUPPORT */

#ifdef LXVST_SUPPORT
	if (Config->get_use_lxvst()) {
		add_lxvst_presets();
	}
#endif /* Native LinuxVST support*/

	if ((s = getenv ("LADSPA_PATH"))) {
		ladspa_path = s;
	}

	if ((s = getenv ("VST_PATH"))) {
		windows_vst_path = s;
	} else if ((s = getenv ("VST_PLUGINS"))) {
		windows_vst_path = s;
	}

	if ((s = getenv ("LXVST_PATH"))) {
		lxvst_path = s;
	} else if ((s = getenv ("LXVST_PLUGINS"))) {
		lxvst_path = s;
	}

	if (_instance == 0) {
		_instance = this;
	}

	/* the plugin manager is constructed too early to use Profile */

	if (getenv ("ARDOUR_SAE")) {
		ladspa_plugin_whitelist.push_back (1203); // single band parametric
		ladspa_plugin_whitelist.push_back (1772); // caps compressor
		ladspa_plugin_whitelist.push_back (1913); // fast lookahead limiter
		ladspa_plugin_whitelist.push_back (1075); // simple RMS expander
		ladspa_plugin_whitelist.push_back (1061); // feedback delay line (max 5s)
		ladspa_plugin_whitelist.push_back (1216); // gverb
		ladspa_plugin_whitelist.push_back (2150); // tap pitch shifter
	}

	BootMessage (_("Discovering Plugins"));
}


PluginManager::~PluginManager()
{
}


void
PluginManager::refresh ()
{
	DEBUG_TRACE (DEBUG::PluginManager, "PluginManager::refresh\n");

	ladspa_refresh ();
#ifdef LV2_SUPPORT
	lv2_refresh ();
#endif
#ifdef WINDOWS_VST_SUPPORT
	if (Config->get_use_windows_vst()) {
		windows_vst_refresh ();
	}
#endif // WINDOWS_VST_SUPPORT

#ifdef LXVST_SUPPORT
	if(Config->get_use_lxvst()) {
		lxvst_refresh();
	}
#endif //Native linuxVST SUPPORT

#ifdef AUDIOUNIT_SUPPORT
	au_refresh ();
#endif

	PluginListChanged (); /* EMIT SIGNAL */
}

void
PluginManager::ladspa_refresh ()
{
	if (_ladspa_plugin_info)
		_ladspa_plugin_info->clear ();
	else
		_ladspa_plugin_info = new ARDOUR::PluginInfoList ();

	static const char *standard_paths[] = {
		"/usr/local/lib64/ladspa",
		"/usr/local/lib/ladspa",
		"/usr/lib64/ladspa",
		"/usr/lib/ladspa",
		"/Library/Audio/Plug-Ins/LADSPA",
		""
	};

	/* allow LADSPA_PATH to augment, not override standard locations */

	/* Only add standard locations to ladspa_path if it doesn't
	 * already contain them. Check for trailing G_DIR_SEPARATOR too.
	 */

	int i;
	for (i = 0; standard_paths[i][0]; i++) {
		size_t found = ladspa_path.find(standard_paths[i]);
		if (found != ladspa_path.npos) {
			switch (ladspa_path[found + strlen(standard_paths[i])]) {
				case ':' :
				case '\0':
					continue;
				case G_DIR_SEPARATOR :
					if (ladspa_path[found + strlen(standard_paths[i]) + 1] == ':' ||
					    ladspa_path[found + strlen(standard_paths[i]) + 1] == '\0') {
						continue;
					}
			}
		}
		if (!ladspa_path.empty())
			ladspa_path += ":";

		ladspa_path += standard_paths[i];

	}

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("LADSPA: search along: [%1]\n", ladspa_path));

	ladspa_discover_from_path (ladspa_path);
}


int
PluginManager::add_ladspa_directory (string path)
{
	if (ladspa_discover_from_path (path) == 0) {
		ladspa_path += ':';
		ladspa_path += path;
		return 0;
	}
	return -1;
}

static bool ladspa_filter (const string& str, void */*arg*/)
{
	/* Not a dotfile, has a prefix before a period, suffix is "so" */

	return str[0] != '.' && (str.length() > 3 && str.find (".so") == (str.length() - 3));
}

int
PluginManager::ladspa_discover_from_path (string /*path*/)
{
	PathScanner scanner;
	vector<string *> *plugin_objects;
	vector<string *>::iterator x;
	int ret = 0;

	plugin_objects = scanner (ladspa_path, ladspa_filter, 0, false, true);

	if (plugin_objects) {
		for (x = plugin_objects->begin(); x != plugin_objects->end (); ++x) {
			ladspa_discover (**x);
		}
	}

	vector_delete (plugin_objects);
	return ret;
}

static bool rdf_filter (const string &str, void* /*arg*/)
{
	return str[0] != '.' &&
		   ((str.find(".rdf")  == (str.length() - 4)) ||
            (str.find(".rdfs") == (str.length() - 5)) ||
		    (str.find(".n3")   == (str.length() - 3)) ||
		    (str.find(".ttl")  == (str.length() - 4)));
}

void
PluginManager::add_ladspa_presets()
{
	add_presets ("ladspa");
}

void
PluginManager::add_windows_vst_presets()
{
	add_presets ("windows-vst");
}

void
PluginManager::add_lxvst_presets()
{
	add_presets ("lxvst");
}

void
PluginManager::add_presets(string domain)
{

	PathScanner scanner;
	vector<string *> *presets;
	vector<string *>::iterator x;

	char* envvar;
	if ((envvar = getenv ("HOME")) == 0) {
		return;
	}

	string path = string_compose("%1/.%2/rdf", envvar, domain);
	presets = scanner (path, rdf_filter, 0, false, true);

	if (presets) {
		for (x = presets->begin(); x != presets->end (); ++x) {
			string file = "file:" + **x;
			if (lrdf_read_file(file.c_str())) {
				warning << string_compose(_("Could not parse rdf file: %1"), *x) << endmsg;
			}
		}
	}

	vector_delete (presets);
}

void
PluginManager::add_lrdf_data (const string &path)
{
	PathScanner scanner;
	vector<string *>* rdf_files;
	vector<string *>::iterator x;

	rdf_files = scanner (path, rdf_filter, 0, false, true);

	if (rdf_files) {
		for (x = rdf_files->begin(); x != rdf_files->end (); ++x) {
			const string uri(string("file://") + **x);

			if (lrdf_read_file(uri.c_str())) {
				warning << "Could not parse rdf file: " << uri << endmsg;
			}
		}
	}

	vector_delete (rdf_files);
}

int
PluginManager::ladspa_discover (string path)
{
	void *module;
	const LADSPA_Descriptor *descriptor;
	LADSPA_Descriptor_Function dfunc;
	const char *errstr;

	if ((module = dlopen (path.c_str(), RTLD_NOW)) == 0) {
		error << string_compose(_("LADSPA: cannot load module \"%1\" (%2)"), path, dlerror()) << endmsg;
		return -1;
	}

	dfunc = (LADSPA_Descriptor_Function) dlsym (module, "ladspa_descriptor");

	if ((errstr = dlerror()) != 0) {
		error << string_compose(_("LADSPA: module \"%1\" has no descriptor function."), path) << endmsg;
		error << errstr << endmsg;
		dlclose (module);
		return -1;
	}

	for (uint32_t i = 0; ; ++i) {
		if ((descriptor = dfunc (i)) == 0) {
			break;
		}

		if (!ladspa_plugin_whitelist.empty()) {
			if (find (ladspa_plugin_whitelist.begin(), ladspa_plugin_whitelist.end(), descriptor->UniqueID) == ladspa_plugin_whitelist.end()) {
				continue;
			}
		}

		PluginInfoPtr info(new LadspaPluginInfo);
		info->name = descriptor->Name;
		info->category = get_ladspa_category(descriptor->UniqueID);
		info->creator = descriptor->Maker;
		info->path = path;
		info->index = i;
		info->n_inputs = ChanCount();
		info->n_outputs = ChanCount();
		info->type = ARDOUR::LADSPA;

		char buf[32];
		snprintf (buf, sizeof (buf), "%lu", descriptor->UniqueID);
		info->unique_id = buf;

		for (uint32_t n=0; n < descriptor->PortCount; ++n) {
			if ( LADSPA_IS_PORT_AUDIO (descriptor->PortDescriptors[n]) ) {
				if ( LADSPA_IS_PORT_INPUT (descriptor->PortDescriptors[n]) ) {
					info->n_inputs.set_audio(info->n_inputs.n_audio() + 1);
				}
				else if ( LADSPA_IS_PORT_OUTPUT (descriptor->PortDescriptors[n]) ) {
					info->n_outputs.set_audio(info->n_outputs.n_audio() + 1);
				}
			}
		}

		if(_ladspa_plugin_info->empty()){
			_ladspa_plugin_info->push_back (info);
		}

		//Ensure that the plugin is not already in the plugin list.

		bool found = false;

		for (PluginInfoList::const_iterator i = _ladspa_plugin_info->begin(); i != _ladspa_plugin_info->end(); ++i) {
			if(0 == info->unique_id.compare((*i)->unique_id)){
			      found = true;
			}
		}

		if(!found){
		    _ladspa_plugin_info->push_back (info);
		}
	}

// GDB WILL NOT LIKE YOU IF YOU DO THIS
//	dlclose (module);

	return 0;
}

string
PluginManager::get_ladspa_category (uint32_t plugin_id)
{
	char buf[256];
	lrdf_statement pattern;

	snprintf(buf, sizeof(buf), "%s%" PRIu32, LADSPA_BASE, plugin_id);
	pattern.subject = buf;
	pattern.predicate = const_cast<char*>(RDF_TYPE);
	pattern.object = 0;
	pattern.object_type = lrdf_uri;

	lrdf_statement* matches1 = lrdf_matches (&pattern);

	if (!matches1) {
		return "Unknown";
	}

	pattern.subject = matches1->object;
	pattern.predicate = const_cast<char*>(LADSPA_BASE "hasLabel");
	pattern.object = 0;
	pattern.object_type = lrdf_literal;

	lrdf_statement* matches2 = lrdf_matches (&pattern);
	lrdf_free_statements(matches1);

	if (!matches2) {
		return ("Unknown");
	}

	string label = matches2->object;
	lrdf_free_statements(matches2);

	/* Kludge LADSPA class names to be singular and match LV2 class names.
	   This avoids duplicate plugin menus for every class, which is necessary
	   to make the plugin category menu at all usable, but is obviously a
	   filthy kludge.

	   In the short term, lrdf could be updated so the labels match and a new
	   release made. To support both specs, we should probably be mapping the
	   URIs to the same category in code and perhaps tweaking that hierarchy
	   dynamically to suit the user. Personally, I (drobilla) think that time
	   is better spent replacing the little-used LRDF.

	   In the longer term, we will abandon LRDF entirely in favour of LV2 and
	   use that class hierarchy. Aside from fixing this problem properly, that
	   will also allow for translated labels. SWH plugins have been LV2 for
	   ages; TAP needs porting. I don't know of anything else with LRDF data.
	*/
	if (label == "Utilities") {
		return "Utility";
	} else if (label == "Pitch shifters") {
		return "Pitch Shifter";
	} else if (label != "Dynamics" && label != "Chorus"
	           &&label[label.length() - 1] == 's'
	           && label[label.length() - 2] != 's') {
		return label.substr(0, label.length() - 1);
	} else {
		return label;
	}
}

#ifdef LV2_SUPPORT
void
PluginManager::lv2_refresh ()
{
	DEBUG_TRACE (DEBUG::PluginManager, "LV2: refresh\n");
	delete _lv2_plugin_info;
	_lv2_plugin_info = LV2PluginInfo::discover();
}
#endif

#ifdef AUDIOUNIT_SUPPORT
void
PluginManager::au_refresh ()
{
	DEBUG_TRACE (DEBUG::PluginManager, "AU: refresh\n");
	delete _au_plugin_info;
	_au_plugin_info = AUPluginInfo::discover();
}

#endif

#ifdef WINDOWS_VST_SUPPORT

void
PluginManager::windows_vst_refresh ()
{
	if (_windows_vst_plugin_info) {
		_windows_vst_plugin_info->clear ();
	} else {
		_windows_vst_plugin_info = new ARDOUR::PluginInfoList();
	}

	if (windows_vst_path.length() == 0) {
		windows_vst_path = "/usr/local/lib/vst:/usr/lib/vst";
	}

	windows_vst_discover_from_path (windows_vst_path);
}

int
PluginManager::add_windows_vst_directory (string path)
{
	if (windows_vst_discover_from_path (path) == 0) {
		windows_vst_path += ':';
		windows_vst_path += path;
		return 0;
	}
	return -1;
}

static bool windows_vst_filter (const string& str, void *arg)
{
	/* Not a dotfile, has a prefix before a period, suffix is "dll" */

	return str[0] != '.' && (str.length() > 4 && str.find (".dll") == (str.length() - 4));
}

int
PluginManager::windows_vst_discover_from_path (string path)
{
	PathScanner scanner;
	vector<string *> *plugin_objects;
	vector<string *>::iterator x;
	int ret = 0;

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("detecting Windows VST plugins along %1\n", path));

	plugin_objects = scanner (windows_vst_path, windows_vst_filter, 0, false, true);

	if (plugin_objects) {
		for (x = plugin_objects->begin(); x != plugin_objects->end (); ++x) {
			windows_vst_discover (**x);
		}
	}

	vector_delete (plugin_objects);
	return ret;
}

int
PluginManager::windows_vst_discover (string path)
{
	VSTInfo* finfo;
	char buf[32];

	if ((finfo = fst_get_info (const_cast<char *> (path.c_str()))) == 0) {
		warning << "Cannot get Windows VST information from " << path << endmsg;
		return -1;
	}

	if (!finfo->canProcessReplacing) {
		warning << string_compose (_("VST plugin %1 does not support processReplacing, and so cannot be used in ardour at this time"),
				    finfo->name)
			<< endl;
	}

	PluginInfoPtr info (new WindowsVSTPluginInfo);

	/* what a joke freeware VST is */

	if (!strcasecmp ("The Unnamed plugin", finfo->name)) {
		info->name = PBD::basename_nosuffix (path);
	} else {
		info->name = finfo->name;
	}


	snprintf (buf, sizeof (buf), "%d", finfo->UniqueID);
	info->unique_id = buf;
	info->category = "VST";
	info->path = path;
	info->creator = finfo->creator;
	info->index = 0;
	info->n_inputs.set_audio (finfo->numInputs);
	info->n_outputs.set_audio (finfo->numOutputs);
	info->n_inputs.set_midi (finfo->wantMidi ? 1 : 0);
	info->type = ARDOUR::Windows_VST;

	_windows_vst_plugin_info->push_back (info);
	fst_free_info (finfo);

	return 0;
}

#endif // WINDOWS_VST_SUPPORT

#ifdef LXVST_SUPPORT

void
PluginManager::lxvst_refresh ()
{
	if (_lxvst_plugin_info) {
		_lxvst_plugin_info->clear ();
	} else {
		_lxvst_plugin_info = new ARDOUR::PluginInfoList();
	}

	if (lxvst_path.length() == 0) {
		lxvst_path = "/usr/local/lib64/lxvst:/usr/local/lib/lxvst:/usr/lib64/lxvst:/usr/lib/lxvst";
	}

	lxvst_discover_from_path (lxvst_path);
}

int
PluginManager::add_lxvst_directory (string path)
{
	if (lxvst_discover_from_path (path) == 0) {
		lxvst_path += ':';
		lxvst_path += path;
		return 0;
	}
	return -1;
}

static bool lxvst_filter (const string& str, void *)
{
	/* Not a dotfile, has a prefix before a period, suffix is "so" */

	return str[0] != '.' && (str.length() > 3 && str.find (".so") == (str.length() - 3));
}

int
PluginManager::lxvst_discover_from_path (string path)
{
	PathScanner scanner;
	vector<string *> *plugin_objects;
	vector<string *>::iterator x;
	int ret = 0;

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Discovering linuxVST plugins along %1\n", path));

	plugin_objects = scanner (lxvst_path, lxvst_filter, 0, false, true);

	if (plugin_objects) {
		for (x = plugin_objects->begin(); x != plugin_objects->end (); ++x) {
			lxvst_discover (**x);
		}
	}

	vector_delete (plugin_objects);
	return ret;
}

int
PluginManager::lxvst_discover (string path)
{
	VSTInfo* finfo;
	char buf[32];

	if ((finfo = vstfx_get_info (const_cast<char *> (path.c_str()))) == 0) {
		return -1;
	}

	if (!finfo->canProcessReplacing) {
		warning << string_compose (_("linuxVST plugin %1 does not support processReplacing, and so cannot be used in ardour at this time"),
				    finfo->name)
			<< endl;
	}

	PluginInfoPtr info(new LXVSTPluginInfo);

	if (!strcasecmp ("The Unnamed plugin", finfo->name)) {
		info->name = PBD::basename_nosuffix (path);
	} else {
		info->name = finfo->name;
	}

	
	snprintf (buf, sizeof (buf), "%d", finfo->UniqueID);
	info->unique_id = buf;
	info->category = "linuxVSTs";
	info->path = path;
	info->creator = finfo->creator;
	info->index = 0;
	info->n_inputs.set_audio (finfo->numInputs);
	info->n_outputs.set_audio (finfo->numOutputs);
	info->n_inputs.set_midi (finfo->wantMidi ? 1 : 0);
	info->type = ARDOUR::LXVST;

        /* Make sure we don't find the same plugin in more than one place along
	   the LXVST_PATH We can't use a simple 'find' because the path is included
	   in the PluginInfo, and that is the one thing we can be sure MUST be
	   different if a duplicate instance is found.  So we just compare the type
	   and unique ID (which for some VSTs isn't actually unique...)
	*/
	
	if (!_lxvst_plugin_info->empty()) {
		for (PluginInfoList::iterator i =_lxvst_plugin_info->begin(); i != _lxvst_plugin_info->end(); ++i) {
			if ((info->type == (*i)->type)&&(info->unique_id == (*i)->unique_id)) {
				warning << "Ignoring duplicate Linux VST plugin " << info->name << "\n";
				vstfx_free_info(finfo);
				return 0;
			}
		}
	}
	
	_lxvst_plugin_info->push_back (info);
	vstfx_free_info (finfo);

	return 0;
}

#endif // LXVST_SUPPORT


PluginManager::PluginStatusType
PluginManager::get_status (const PluginInfoPtr& pi)
{
	PluginStatus ps (pi->type, pi->unique_id);
	PluginStatusList::const_iterator i =  find (statuses.begin(), statuses.end(), ps);
	if (i ==  statuses.end() ) {
		return Normal;
	} else {
		return i->status;
	}
}

void
PluginManager::save_statuses ()
{
	ofstream ofs;
	std::string path = Glib::build_filename (user_config_directory(), "plugin_statuses");

	ofs.open (path.c_str(), ios_base::openmode (ios::out|ios::trunc));

	if (!ofs) {
		return;
	}

	for (PluginStatusList::iterator i = statuses.begin(); i != statuses.end(); ++i) {
		switch ((*i).type) {
		case LADSPA:
			ofs << "LADSPA";
			break;
		case AudioUnit:
			ofs << "AudioUnit";
			break;
		case LV2:
			ofs << "LV2";
			break;
		case Windows_VST:
			ofs << "Windows-VST";
			break;
		case LXVST:
			ofs << "LXVST";
			break;
		}

		ofs << ' ';

		switch ((*i).status) {
		case Normal:
			ofs << "Normal";
			break;
		case Favorite:
			ofs << "Favorite";
			break;
		case Hidden:
			ofs << "Hidden";
			break;
		}

		ofs << ' ';
		ofs << (*i).unique_id;;
		ofs << endl;
	}

	ofs.close ();
}

void
PluginManager::load_statuses ()
{
	std::string path = Glib::build_filename (user_config_directory(), "plugin_statuses");
	ifstream ifs (path.c_str());

	if (!ifs) {
		return;
	}

	std::string stype;
	std::string sstatus;
	std::string id;
	PluginType type;
	PluginStatusType status;
	char buf[1024];

	while (ifs) {

		ifs >> stype;
		if (!ifs) {
			break;

		}

		ifs >> sstatus;
		if (!ifs) {
			break;

		}

		/* rest of the line is the plugin ID */

		ifs.getline (buf, sizeof (buf), '\n');
		if (!ifs) {
			break;
		}

		if (sstatus == "Normal") {
			status = Normal;
		} else if (sstatus == "Favorite") {
			status = Favorite;
		} else if (sstatus == "Hidden") {
			status = Hidden;
		} else {
			error << string_compose (_("unknown plugin status type \"%1\" - all entries ignored"), sstatus)
				  << endmsg;
			statuses.clear ();
			break;
		}

		if (stype == "LADSPA") {
			type = LADSPA;
		} else if (stype == "AudioUnit") {
			type = AudioUnit;
		} else if (stype == "LV2") {
			type = LV2;
		} else if (stype == "Windows-VST") {
			type = Windows_VST;
		} else if (stype == "LXVST") {
			type = LXVST;
		} else {
			error << string_compose (_("unknown plugin type \"%1\" - ignored"), stype)
			      << endmsg;
			continue;
		}

		id = buf;
		strip_whitespace_edges (id);
		set_status (type, id, status);
	}

	ifs.close ();
}

void
PluginManager::set_status (PluginType t, string id, PluginStatusType status)
{
	PluginStatus ps (t, id, status);
	statuses.erase (ps);

	if (status == Normal) {
		return;
	}

	statuses.insert (ps);
}

ARDOUR::PluginInfoList&
PluginManager::windows_vst_plugin_info ()
{
#ifdef WINDOWS_VST_SUPPORT
	if (!_windows_vst_plugin_info) {
		windows_vst_refresh ();
	}
	return *_windows_vst_plugin_info;
#else
	return _empty_plugin_info;
#endif
}

ARDOUR::PluginInfoList&
PluginManager::lxvst_plugin_info ()
{
#ifdef LXVST_SUPPORT
	if (!_lxvst_plugin_info)
		lxvst_refresh();
	return *_lxvst_plugin_info;
#else
	return _empty_plugin_info;
#endif
}

ARDOUR::PluginInfoList&
PluginManager::ladspa_plugin_info ()
{
	if (!_ladspa_plugin_info)
		ladspa_refresh();
	return *_ladspa_plugin_info;
}

ARDOUR::PluginInfoList&
PluginManager::lv2_plugin_info ()
{
#ifdef LV2_SUPPORT
	if (!_lv2_plugin_info)
		lv2_refresh();
	return *_lv2_plugin_info;
#else
	return _empty_plugin_info;
#endif
}

ARDOUR::PluginInfoList&
PluginManager::au_plugin_info ()
{
#ifdef AUDIOUNIT_SUPPORT
	if (!_au_plugin_info)
		au_refresh();
	return *_au_plugin_info;
#else
	return _empty_plugin_info;
#endif
}
