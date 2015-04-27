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
#include <cstdlib>
#include <fstream>

#ifdef HAVE_LRDF
#include <lrdf.h>
#endif

#ifdef WINDOWS_VST_SUPPORT
#include "ardour/vst_info_file.h"
#include "fst.h"
#include "pbd/basename.h"
#include <cstring>
#endif // WINDOWS_VST_SUPPORT

#ifdef LXVST_SUPPORT
#include "ardour/vst_info_file.h"
#include "ardour/linux_vst_support.h"
#include "pbd/basename.h"
#include <cstring>
#endif //LXVST_SUPPORT

#include <glib/gstdio.h>
#include <glibmm/miscutils.h>
#include <glibmm/pattern.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/whitespace.h"
#include "pbd/file_utils.h"

#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/ladspa.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/plugin.h"
#include "ardour/plugin_manager.h"
#include "ardour/rc_configuration.h"

#include "ardour/search_paths.h"

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

#include "ardour/debug.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

PluginManager* PluginManager::_instance = 0;
std::string PluginManager::scanner_bin_path = "";

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
	, _cancel_scan(false)
	, _cancel_timeout(false)
{
	char* s;
	string lrdf_path;

#if defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT
	// source-tree (ardev, etc)
	PBD::Searchpath vstsp(Glib::build_filename(ARDOUR::ardour_dll_directory(), "fst"));

#ifdef PLATFORM_WINDOWS
	// on windows the .exe needs to be in the same folder with libardour.dll
	vstsp += Glib::build_filename(g_win32_get_package_installation_directory_of_module (0), "bin");
#else
	// on Unices additional internal-use binaries are deployed to $libdir
	vstsp += ARDOUR::ardour_dll_directory();
#endif

	if (!PBD::find_file (vstsp,
#ifdef PLATFORM_WINDOWS
    #ifdef DEBUGGABLE_SCANNER_APP
        #if defined(DEBUG) || defined(_DEBUG)
				"ardour-vst-scannerD.exe"
        #else
				"ardour-vst-scannerRDC.exe"
        #endif
    #else
				"ardour-vst-scanner.exe"
    #endif
#else
				"ardour-vst-scanner"
#endif
				, scanner_bin_path)) {
		PBD::warning << "VST scanner app (ardour-vst-scanner) not found in path " << vstsp.to_string() <<  endmsg;
	}
#endif

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

	if ((s = getenv ("VST_PATH"))) {
		windows_vst_path = s;
	} else if ((s = getenv ("VST_PLUGINS"))) {
		windows_vst_path = s;
	}

	if (windows_vst_path.length() == 0) {
		windows_vst_path = vst_search_path ();
	}

	if ((s = getenv ("LXVST_PATH"))) {
		lxvst_path = s;
	} else if ((s = getenv ("LXVST_PLUGINS"))) {
		lxvst_path = s;
	}

	if (lxvst_path.length() == 0) {
		lxvst_path = "/usr/local/lib64/lxvst:/usr/local/lib/lxvst:/usr/lib64/lxvst:/usr/lib/lxvst:"
			"/usr/local/lib64/linux_vst:/usr/local/lib/linux_vst:/usr/lib64/linux_vst:/usr/lib/linux_vst:"
			"/usr/lib/vst:/usr/local/lib/vst";
	}

	/* first time setup, use 'default' path */
	if (Config->get_plugin_path_lxvst() == X_("@default@")) {
		Config->set_plugin_path_lxvst(get_default_lxvst_path());
	}
	if (Config->get_plugin_path_vst() == X_("@default@")) {
		Config->set_plugin_path_vst(get_default_windows_vst_path());
	}

	if (_instance == 0) {
		_instance = this;
	}

	BootMessage (_("Discovering Plugins"));
}


PluginManager::~PluginManager()
{
	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {
		// don't bother, just exit quickly.
		delete _windows_vst_plugin_info;
		delete _lxvst_plugin_info;
		delete _ladspa_plugin_info;
		delete _lv2_plugin_info;
		delete _au_plugin_info;
	}
}

void
PluginManager::refresh (bool cache_only)
{
	DEBUG_TRACE (DEBUG::PluginManager, "PluginManager::refresh\n");
	_cancel_scan = false;

	BootMessage (_("Scanning LADSPA Plugins"));
	ladspa_refresh ();
#ifdef LV2_SUPPORT
	BootMessage (_("Scanning LV2 Plugins"));
	lv2_refresh ();
#endif
#ifdef WINDOWS_VST_SUPPORT
	if (Config->get_use_windows_vst()) {
		BootMessage (_("Scanning Windows VST Plugins"));
		windows_vst_refresh (cache_only);
	}
#endif // WINDOWS_VST_SUPPORT

#ifdef LXVST_SUPPORT
	if(Config->get_use_lxvst()) {
		BootMessage (_("Scanning Linux VST Plugins"));
		lxvst_refresh(cache_only);
	}
#endif //Native linuxVST SUPPORT

#ifdef AUDIOUNIT_SUPPORT
	BootMessage (_("Scanning AU Plugins"));
	au_refresh (cache_only);
#endif

	BootMessage (_("Plugin Scan Complete..."));
	PluginListChanged (); /* EMIT SIGNAL */
	PluginScanMessage(X_("closeme"), "", false);
	_cancel_scan = false;
}

void
PluginManager::cancel_plugin_scan ()
{
	_cancel_scan = true;
}

void
PluginManager::cancel_plugin_timeout ()
{
	_cancel_timeout = true;
}

void
PluginManager::clear_vst_cache ()
{
	// see also libs/ardour/vst_info_file.cc - vstfx_infofile_path()
#ifdef WINDOWS_VST_SUPPORT
	{
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, Config->get_plugin_path_vst(), "\\.fsi$", true);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
#endif

#ifdef LXVST_SUPPORT
	{
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, Config->get_plugin_path_lxvst(), "\\.fsi$", true);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
#endif

#if (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT)
	{
		string personal = get_personal_vst_info_cache_dir();
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, personal, "\\.fsi$", /* user cache is flat, no recursion */ false);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
#endif
}

void
PluginManager::clear_vst_blacklist ()
{
#ifdef WINDOWS_VST_SUPPORT
	{
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, Config->get_plugin_path_vst(), "\\.fsb$", true);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
#endif

#ifdef LXVST_SUPPORT
	{
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, Config->get_plugin_path_lxvst(), "\\.fsb$", true);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
#endif

#if (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT)
	{
		string personal = get_personal_vst_blacklist_dir();

		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, personal, "\\.fsb$", /* flat user cache */ false);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
#endif
}

void
PluginManager::clear_au_cache ()
{
#ifdef AUDIOUNIT_SUPPORT
	// AUPluginInfo::au_cache_path ()
	string fn = Glib::build_filename (ARDOUR::user_config_directory(), "au_cache");
	if (Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		::g_unlink(fn.c_str());
	}
#endif
}

void
PluginManager::clear_au_blacklist ()
{
#ifdef AUDIOUNIT_SUPPORT
	string fn = Glib::build_filename (ARDOUR::user_cache_directory(), "au_blacklist.txt");
	if (Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		::g_unlink(fn.c_str());
	}
#endif
}

void
PluginManager::ladspa_refresh ()
{
	if (_ladspa_plugin_info) {
		_ladspa_plugin_info->clear ();
	} else {
		_ladspa_plugin_info = new ARDOUR::PluginInfoList ();
	}

	/* allow LADSPA_PATH to augment, not override standard locations */

	/* Only add standard locations to ladspa_path if it doesn't
	 * already contain them. Check for trailing G_DIR_SEPARATOR too.
	 */

	vector<string> ladspa_modules;

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("LADSPA: search along: [%1]\n", ladspa_search_path().to_string()));

	find_files_matching_pattern (ladspa_modules, ladspa_search_path (), "*.so");
	find_files_matching_pattern (ladspa_modules, ladspa_search_path (), "*.dylib");
	find_files_matching_pattern (ladspa_modules, ladspa_search_path (), "*.dll");

	for (vector<std::string>::iterator i = ladspa_modules.begin(); i != ladspa_modules.end(); ++i) {
		ARDOUR::PluginScanMessage(_("LADSPA"), *i, false);
		ladspa_discover (*i);
	}
}

#ifdef HAVE_LRDF
static bool rdf_filter (const string &str, void* /*arg*/)
{
	return str[0] != '.' &&
		   ((str.find(".rdf")  == (str.length() - 4)) ||
            (str.find(".rdfs") == (str.length() - 5)) ||
		    (str.find(".n3")   == (str.length() - 3)) ||
		    (str.find(".ttl")  == (str.length() - 4)));
}
#endif

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
#ifdef HAVE_LRDF
	vector<string> presets;
	vector<string>::iterator x;

	char* envvar;
	if ((envvar = getenv ("HOME")) == 0) {
		return;
	}

	string path = string_compose("%1/.%2/rdf", envvar, domain);
	find_files_matching_filter (presets, path, rdf_filter, 0, false, true);

	for (x = presets.begin(); x != presets.end (); ++x) {
		string file = "file:" + *x;
		if (lrdf_read_file(file.c_str())) {
			warning << string_compose(_("Could not parse rdf file: %1"), *x) << endmsg;
		}
	}

#endif
}

void
PluginManager::add_lrdf_data (const string &path)
{
#ifdef HAVE_LRDF
	vector<string> rdf_files;
	vector<string>::iterator x;

	find_files_matching_filter (rdf_files, path, rdf_filter, 0, false, true);

	for (x = rdf_files.begin(); x != rdf_files.end (); ++x) {
		const string uri(string("file://") + *x);

		if (lrdf_read_file(uri.c_str())) {
			warning << "Could not parse rdf file: " << uri << endmsg;
		}
	}
#endif
}

int
PluginManager::ladspa_discover (string path)
{
	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Checking for LADSPA plugin at %1\n", path));

	Glib::Module module(path);
	const LADSPA_Descriptor *descriptor;
	LADSPA_Descriptor_Function dfunc;
	void* func = 0;

	if (!module) {
		error << string_compose(_("LADSPA: cannot load module \"%1\" (%2)"),
			path, Glib::Module::get_last_error()) << endmsg;
		return -1;
	}


	if (!module.get_symbol("ladspa_descriptor", func)) {
		error << string_compose(_("LADSPA: module \"%1\" has no descriptor function."), path) << endmsg;
		error << Glib::Module::get_last_error() << endmsg;
		return -1;
	}

	dfunc = (LADSPA_Descriptor_Function)func;

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("LADSPA plugin found at %1\n", path));

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

		DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Found LADSPA plugin, name: %1, Inputs: %2, Outputs: %3\n", info->name, info->n_inputs, info->n_outputs));
	}

// GDB WILL NOT LIKE YOU IF YOU DO THIS
//	dlclose (module);

	return 0;
}

string
PluginManager::get_ladspa_category (uint32_t plugin_id)
{
#ifdef HAVE_LRDF
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
#else
		return ("Unknown");
#endif
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
PluginManager::au_refresh (bool cache_only)
{
	DEBUG_TRACE (DEBUG::PluginManager, "AU: refresh\n");
	if (cache_only && !Config->get_discover_audio_units ()) {
		return;
	}
	delete _au_plugin_info;
	_au_plugin_info = AUPluginInfo::discover();

	// disable automatic scan in case we crash
	Config->set_discover_audio_units (false);
	Config->save_state();

	/* note: AU require a CAComponentDescription pointer provided by the OS.
	 * Ardour only caches port and i/o config. It can't just 'scan' without
	 * 'discovering' (like we do for VST).
	 *
	 * So in case discovery fails, we assume the worst: the Description
	 * is broken (malicious plugins) and even a simple 'scan' would always
	 * crash ardour on startup. Hence we disable Auto-Scan on start.
	 *
	 * If the crash happens at any later time (description is available),
	 * Ardour will blacklist the plugin in question -- unless
	 * the crash happens during realtime-run.
	 */

	// successful scan re-enabled automatic discovery
	Config->set_discover_audio_units (true);
	Config->save_state();
}

#endif

#ifdef WINDOWS_VST_SUPPORT

void
PluginManager::windows_vst_refresh (bool cache_only)
{
	if (_windows_vst_plugin_info) {
		_windows_vst_plugin_info->clear ();
	} else {
		_windows_vst_plugin_info = new ARDOUR::PluginInfoList();
	}

	windows_vst_discover_from_path (Config->get_plugin_path_vst(), cache_only);
}

static bool windows_vst_filter (const string& str, void * /*arg*/)
{
	/* Not a dotfile, has a prefix before a period, suffix is "dll" */
	return str[0] != '.' && str.length() > 4 && strings_equal_ignore_case (".dll", str.substr(str.length() - 4));
}

int
PluginManager::windows_vst_discover_from_path (string path, bool cache_only)
{
	vector<string> plugin_objects;
	vector<string>::iterator x;
	int ret = 0;

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("detecting Windows VST plugins along %1\n", path));

	find_files_matching_filter (plugin_objects, Config->get_plugin_path_vst(), windows_vst_filter, 0, false, true, true);

	for (x = plugin_objects.begin(); x != plugin_objects.end (); ++x) {
		ARDOUR::PluginScanMessage(_("VST"), *x, !cache_only && !cancelled());
		windows_vst_discover (*x, cache_only || cancelled());
	}

	return ret;
}

int
PluginManager::windows_vst_discover (string path, bool cache_only)
{
	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("windows_vst_discover '%1'\n", path));

	_cancel_timeout = false;
	vector<VSTInfo*> * finfos = vstfx_get_info_fst (const_cast<char *> (path.c_str()),
			cache_only ? VST_SCAN_CACHE_ONLY : VST_SCAN_USE_APP);

	if (finfos->empty()) {
		DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Cannot get Windows VST information from '%1'\n", path));
		return -1;
	}

	uint32_t discovered = 0;
	for (vector<VSTInfo *>::iterator x = finfos->begin(); x != finfos->end(); ++x) {
		VSTInfo* finfo = *x;
		char buf[32];

		if (!finfo->canProcessReplacing) {
			warning << string_compose (_("VST plugin %1 does not support processReplacing, and so cannot be used in %2 at this time"),
							 finfo->name, PROGRAM_NAME)
				<< endl;
			continue;
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
		info->n_inputs.set_midi ((finfo->wantMidi&1) ? 1 : 0);
		info->n_outputs.set_midi ((finfo->wantMidi&2) ? 1 : 0);
		info->type = ARDOUR::Windows_VST;

		// TODO: check dup-IDs (lxvst AND windows vst)
		bool duplicate = false;

		if (!_windows_vst_plugin_info->empty()) {
			for (PluginInfoList::iterator i =_windows_vst_plugin_info->begin(); i != _windows_vst_plugin_info->end(); ++i) {
				if ((info->type == (*i)->type)&&(info->unique_id == (*i)->unique_id)) {
					warning << "Ignoring duplicate Windows VST plugin " << info->name << "\n";
					duplicate = true;
					break;
				}
			}
		}

		if (!duplicate) {
			DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Windows VST plugin ID '%1'\n", info->unique_id));
			_windows_vst_plugin_info->push_back (info);
			discovered++;
		}
	}

	vstfx_free_info_list (finfos);
	return discovered > 0 ? 0 : -1;
}

#endif // WINDOWS_VST_SUPPORT

#ifdef LXVST_SUPPORT

void
PluginManager::lxvst_refresh (bool cache_only)
{
	if (_lxvst_plugin_info) {
		_lxvst_plugin_info->clear ();
	} else {
		_lxvst_plugin_info = new ARDOUR::PluginInfoList();
	}

	lxvst_discover_from_path (Config->get_plugin_path_lxvst(), cache_only);
}

static bool lxvst_filter (const string& str, void *)
{
	/* Not a dotfile, has a prefix before a period, suffix is "so" */

	return str[0] != '.' && (str.length() > 3 && str.find (".so") == (str.length() - 3));
}

int
PluginManager::lxvst_discover_from_path (string path, bool cache_only)
{
	vector<string> plugin_objects;
	vector<string>::iterator x;
	int ret = 0;

#ifndef NDEBUG
	(void) path;
#endif

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Discovering linuxVST plugins along %1\n", path));

	find_files_matching_filter (plugin_objects, Config->get_plugin_path_lxvst(), lxvst_filter, 0, false, true, true);

	for (x = plugin_objects.begin(); x != plugin_objects.end (); ++x) {
		ARDOUR::PluginScanMessage(_("LXVST"), *x, !cache_only && !cancelled());
		lxvst_discover (*x, cache_only || cancelled());
	}

	return ret;
}

int
PluginManager::lxvst_discover (string path, bool cache_only)
{
	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("checking apparent LXVST plugin at %1\n", path));

	_cancel_timeout = false;
	vector<VSTInfo*> * finfos = vstfx_get_info_lx (const_cast<char *> (path.c_str()),
			cache_only ? VST_SCAN_CACHE_ONLY : VST_SCAN_USE_APP);

	if (finfos->empty()) {
		DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Cannot get Linux VST information from '%1'\n", path));
		return -1;
	}

	uint32_t discovered = 0;
	for (vector<VSTInfo *>::iterator x = finfos->begin(); x != finfos->end(); ++x) {
		VSTInfo* finfo = *x;
		char buf[32];

		if (!finfo->canProcessReplacing) {
			warning << string_compose (_("linuxVST plugin %1 does not support processReplacing, and so cannot be used in %2 at this time"),
							 finfo->name, PROGRAM_NAME)
				<< endl;
			continue;
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
		info->n_inputs.set_midi ((finfo->wantMidi&1) ? 1 : 0);
		info->n_outputs.set_midi ((finfo->wantMidi&2) ? 1 : 0);
		info->type = ARDOUR::LXVST;

					/* Make sure we don't find the same plugin in more than one place along
			 the LXVST_PATH We can't use a simple 'find' because the path is included
			 in the PluginInfo, and that is the one thing we can be sure MUST be
			 different if a duplicate instance is found.  So we just compare the type
			 and unique ID (which for some VSTs isn't actually unique...)
		*/

		// TODO: check dup-IDs with windowsVST, too
		bool duplicate = false;
		if (!_lxvst_plugin_info->empty()) {
			for (PluginInfoList::iterator i =_lxvst_plugin_info->begin(); i != _lxvst_plugin_info->end(); ++i) {
				if ((info->type == (*i)->type)&&(info->unique_id == (*i)->unique_id)) {
					warning << "Ignoring duplicate Linux VST plugin " << info->name << "\n";
					duplicate = true;
					break;
				}
			}
		}

		if (!duplicate) {
			_lxvst_plugin_info->push_back (info);
			discovered++;
		}
	}

	vstfx_free_info_list (finfos);
	return discovered > 0 ? 0 : -1;
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
	assert(_lxvst_plugin_info);
	return *_lxvst_plugin_info;
#else
	return _empty_plugin_info;
#endif
}

ARDOUR::PluginInfoList&
PluginManager::ladspa_plugin_info ()
{
	assert(_ladspa_plugin_info);
	return *_ladspa_plugin_info;
}

ARDOUR::PluginInfoList&
PluginManager::lv2_plugin_info ()
{
#ifdef LV2_SUPPORT
	assert(_lv2_plugin_info);
	return *_lv2_plugin_info;
#else
	return _empty_plugin_info;
#endif
}

ARDOUR::PluginInfoList&
PluginManager::au_plugin_info ()
{
#ifdef AUDIOUNIT_SUPPORT
	if (_au_plugin_info) {
		return *_au_plugin_info;
	}
#endif
	return _empty_plugin_info;
}
