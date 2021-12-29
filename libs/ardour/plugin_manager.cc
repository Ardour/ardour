/*
 * Copyright (C) 2000-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2018 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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

#include <stdint.h>

#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include <glib.h>
#include "pbd/gstdio_compat.h"

#ifdef HAVE_LRDF
#include <lrdf.h>
#endif

#if defined PLATFORM_WINDOWS && defined VST3_SUPPORT
#include <windows.h>
#include <shlobj.h> // CSIDL_*
#include "pbd/windows_special_dirs.h"
#endif

#ifdef WINDOWS_VST_SUPPORT
#include "fst.h"
#include "pbd/basename.h"
#include <cstring>
#endif // WINDOWS_VST_SUPPORT

#ifdef LXVST_SUPPORT
#include "pbd/basename.h"
#include <cstring>
#endif //LXVST_SUPPORT

#ifdef MACVST_SUPPORT
#include "pbd/basename.h"
#include "pbd/pathexpand.h"
#include <cstring>
#endif //MACVST_SUPPORT

#include <glibmm/miscutils.h>
#include <glibmm/pattern.h>
#include <glibmm/fileutils.h>

#include "pbd/convert.h"
#include "pbd/file_utils.h"
#include "pbd/tokenizer.h"
#include "pbd/whitespace.h"

#include "ardour/directory_names.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/ladspa.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/luascripting.h"
#include "ardour/luaproc.h"
#include "ardour/lv2_plugin.h"
#include "ardour/plugin.h"
#include "ardour/plugin_manager.h"
#include "ardour/rc_configuration.h"
#include "ardour/search_paths.h"

#if (defined WINDOWS_VST_SUPPORT || defined MACVST_SUPPORT || defined LXVST_SUPPORT)
#include "ardour/system_exec.h"
#include "ardour/vst2_scan.h"
#endif

#ifdef WINDOWS_VST_SUPPORT
#include "ardour/windows_vst_plugin.h"
#endif

#ifdef LXVST_SUPPORT
#include "ardour/lxvst_plugin.h"
#include "ardour/linux_vst_support.h"
#endif

#ifdef MACVST_SUPPORT
#include "ardour/mac_vst_support.h"
#include "ardour/mac_vst_plugin.h"
#endif

#ifdef AUDIOUNIT_SUPPORT
#include "CAAudioUnit.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioUnitUtilities.h>

#include "ardour/audio_unit.h"
#include "ardour/auv2_scan.h"
#include "ardour/system_exec.h"
#include <Carbon/Carbon.h>
#endif

#ifdef VST3_SUPPORT
#include "ardour/system_exec.h"
#include "ardour/vst3_module.h"
#include "ardour/vst3_plugin.h"
#include "ardour/vst3_scan.h"
#endif

#include "pbd/error.h"
#include "pbd/stl_delete.h"

#include "pbd/i18n.h"

#include "ardour/debug.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

PluginManager* PluginManager::_instance = 0;
std::string PluginManager::auv2_scanner_bin_path = "";
std::string PluginManager::vst2_scanner_bin_path = "";
std::string PluginManager::vst3_scanner_bin_path = "";


#ifdef VST3_SUPPORT
# if defined __x86_64__ || defined _M_X64
#  define VST3_BLACKLIST  "vst3_x64_blacklist.txt"
# elif defined __i386__  || defined _M_IX86
#  define VST3_BLACKLIST  "vst3_x86_blacklist.txt"
# elif defined __aarch64__
#  define VST3_BLACKLIST  "vst3_a64_blacklist.txt"
# elif defined __arm__
#  define VST3_BLACKLIST  "vst3_a32_blacklist.txt"
# else
#  define VST3_BLACKLIST  "vst3_blacklist.txt"
# endif
#endif

#if (defined WINDOWS_VST_SUPPORT || defined MACVST_SUPPORT || defined LXVST_SUPPORT)
# if ( defined(__x86_64__) || defined(_M_X64) )
#  define VST2_BLACKLIST  "vst2_x64_blacklist.txt"
# else
#  define VST2_BLACKLIST  "vst2_x86_blacklist.txt"
# endif
#endif

#define AUV2_BLACKLIST  "auv2_blacklist.txt"

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
	, _mac_vst_plugin_info(0)
	, _vst3_plugin_info(0)
	, _ladspa_plugin_info(0)
	, _lv2_plugin_info(0)
	, _au_plugin_info(0)
	, _lua_plugin_info(0)
	, _cancel_scan_one (false)
	, _cancel_scan_all (false)
	, _cancel_scan_timeout_one (false)
	, _cancel_scan_timeout_all (false)
	, _enable_scan_timeout (false)
{
	char* s;
	string lrdf_path;

#if defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT || defined VST3_SUPPORT
	// source-tree (ardev, etc)
	PBD::Searchpath vstsp(Glib::build_filename(ARDOUR::ardour_dll_directory(), "fst"));

#ifdef PLATFORM_WINDOWS
	// on windows the .exe needs to be in the same folder with libardour.dll
	vstsp += Glib::build_filename(windows_package_directory_path(), "bin");
#else
	// on Unices additional internal-use binaries are deployed to $libdir
	vstsp += ARDOUR::ardour_dll_directory();
#endif

#if defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT
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
				, vst2_scanner_bin_path)) {
		PBD::warning << "VST scanner app (ardour-vst-scanner) not found in path " << vstsp.to_string() << endmsg;
	}
#endif // VST2

#ifdef VST3_SUPPORT
	if (!PBD::find_file (vstsp,
#ifdef PLATFORM_WINDOWS
    #ifdef DEBUGGABLE_SCANNER_APP
        #if defined(DEBUG) || defined(_DEBUG)
				"ardour-vst3-scannerD.exe"
        #else
				"ardour-vst3-scannerRDC.exe"
        #endif
    #else
				"ardour-vst3-scanner.exe"
    #endif
#else
				"ardour-vst3-scanner"
#endif
				, vst3_scanner_bin_path)) {
		PBD::warning << "VST3 scanner app (ardour-vst3-scanner) not found in path " << vstsp.to_string() << endmsg;
	}
#endif // VST3_SUPPORT
#endif // any VST

#ifdef AUDIOUNIT_SUPPORT
	PBD::Searchpath ausp (Glib::build_filename(ARDOUR::ardour_dll_directory(), "auscan"));
	ausp += ARDOUR::ardour_dll_directory();
	if (!PBD::find_file (ausp, "ardour-au-scanner" , auv2_scanner_bin_path)) {
		PBD::warning << "AUv2 scanner app (ardour-au-scanner) not found in path " << ausp.to_string() << endmsg;
	}
#endif

	load_statuses ();
	load_tags ();
	load_stats ();

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

#ifdef MACVST_SUPPORT
	if (Config->get_use_macvst ()) {
		add_mac_vst_presets ();
	}
#endif

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
	if (Config->get_plugin_path_vst3() == X_("@default@")) {
		/* This path is currently only added to the existing path */
		if ((s = getenv ("VST3_PATH"))) {
			Config->set_plugin_path_vst3 (s);
		} else {
			Config->set_plugin_path_vst3 ("");
		}
	}

	if (_instance == 0) {
		_instance = this;
	}

	BootMessage (_("Discovering Plugins"));

	LuaScripting::instance().scripts_changed.connect_same_thread (lua_refresh_connection, boost::bind (&PluginManager::lua_refresh_cb, this));
}


PluginManager::~PluginManager()
{
	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {
		// don't bother, just exit quickly.
		delete _windows_vst_plugin_info;
		delete _lxvst_plugin_info;
		delete _mac_vst_plugin_info;
		delete _ladspa_plugin_info;
		delete _lv2_plugin_info;
		delete _au_plugin_info;
		delete _lua_plugin_info;
	}

	/* do drop VST3 Info in order to release any loaded modules */
	delete _vst3_plugin_info;
}

bool
PluginManager::cache_valid () const
{
	return Config->get_plugin_cache_version () >= cache_version ();
}

uint32_t
PluginManager::cache_version ()
{
	return 1000 * atoi (X_(PROGRAM_VERSION)) + 2;
}

struct PluginInfoPtrNameSorter {
	bool operator () (PluginInfoPtr const& a, PluginInfoPtr const& b) const {
		return PBD::downcase (a->name) < PBD::downcase (b->name);
	}
};

void
PluginManager::detect_name_ambiguities (PluginInfoList* pil)
{
	if (!pil) {
		return;
	}
	pil->sort (PluginInfoPtrNameSorter ());

	for (PluginInfoList::iterator i = pil->begin(); i != pil->end();) {
		 PluginInfoPtr& p = *i;
		 ++i;
		 if (i != pil->end() && PBD::downcase ((*i)->name) == PBD::downcase (p->name)) {
			 /* mark name as ambiguous IFF ambiguity can be resolved
				* by listing number of audio outputs.
				* This is used in the instrument selector.
				*/
			 bool r = p->max_configurable_ouputs () != (*i)->max_configurable_ouputs ();
			 p->multichannel_name_ambiguity = r;
			 (*i)->multichannel_name_ambiguity = r;
		 }
	}
}

void
PluginManager::detect_type_ambiguities (PluginInfoList& pil)
{
	PluginInfoList dup;
	pil.sort (PluginInfoPtrNameSorter ());
	for (PluginInfoList::iterator i = pil.begin(); i != pil.end(); ++i) {
		switch (dup.size ()) {
			case 0:
				break;
			case 1:
				if (PBD::downcase (dup.back()->name) != PBD::downcase ((*i)->name)) {
					dup.clear ();
				}
				break;
			default:
				if (PBD::downcase (dup.back()->name) != PBD::downcase ((*i)->name)) {
					/* found multiple plugins with same name */
					bool typediff = false;
					bool chandiff = false;
					for (PluginInfoList::iterator j = dup.begin(); j != dup.end(); ++j) {
						if (dup.front()->type != (*j)->type) {
							typediff = true;
						}
						chandiff |= (*j)->multichannel_name_ambiguity;
					}
					if (typediff) {
						for (PluginInfoList::iterator j = dup.begin(); j != dup.end(); ++j) {
							(*j)->plugintype_name_ambiguity = true;
							/* show multi-channel information for consistency, when other types display it.
							 * eg. "Foo 8 outs, VST", "Foo 12 outs, VST", "Foo <=12 outs, AU"
							 */
							if (chandiff) {
								(*j)->multichannel_name_ambiguity = true;
							}
						}
					}
					dup.clear ();
				}
				break;
		}
		dup.push_back (*i);
	}
}

void
PluginManager::conceal_duplicates (ARDOUR::PluginInfoList* old, ARDOUR::PluginInfoList* nu)
{
	if (!old || !nu) {
		return;
	}
	for (PluginInfoList::const_iterator i = old->begin(); i != old->end(); ++i) {
		for (PluginInfoList::const_iterator j = nu->begin(); j != nu->end(); ++j) {
			if ((*i)->creator == (*j)->creator && (*i)->name == (*j)->name) {
				PluginStatus ps ((*i)->type, (*i)->unique_id, Concealed);
				if (find (statuses.begin(), statuses.end(), ps) == statuses.end()) {
					statuses.erase (ps);
					statuses.insert (ps);
				}
			}
		}
	}
}

void
PluginManager::refresh (bool cache_only)
{
	Glib::Threads::Mutex::Lock lm (_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		return;
	}

	load_scanlog ();

	DEBUG_TRACE (DEBUG::PluginManager, "PluginManager::refresh\n");
	reset_scan_cancel_state ();

	BootMessage (_("Scanning LADSPA Plugins"));
	ladspa_refresh ();
	BootMessage (_("Scanning Lua DSP Processors"));
	lua_refresh ();

	BootMessage (_("Scanning LV2 Plugins"));
	lv2_refresh ();

	bool conceal_lv1 = Config->get_conceal_lv1_if_lv2_exists();

	if (conceal_lv1) {
		conceal_duplicates (_ladspa_plugin_info, _lv2_plugin_info);
	}

#ifdef WINDOWS_VST_SUPPORT
	if (Config->get_use_windows_vst()) {
		if (cache_only) {
			BootMessage (_("Scanning Windows VST Plugins"));
		} else {
			BootMessage (_("Discovering Windows VST Plugins"));
		}
		windows_vst_refresh (cache_only);
	}
#endif // WINDOWS_VST_SUPPORT

#ifdef LXVST_SUPPORT
	if(Config->get_use_lxvst()) {
		if (cache_only) {
			BootMessage (_("Scanning Linux VST Plugins"));
		} else {
			BootMessage (_("Discovering Linux VST Plugins"));
		}
		lxvst_refresh(cache_only);
	}
#endif //Native linuxVST SUPPORT

#ifdef MACVST_SUPPORT
	if(Config->get_use_macvst ()) {
		if (cache_only) {
			BootMessage (_("Scanning Mac VST Plugins"));
		} else {
			BootMessage (_("Discovering Mac VST Plugins"));
		}
		mac_vst_refresh (cache_only);
	} else if (_mac_vst_plugin_info) {
		_mac_vst_plugin_info->clear ();
	} else {
		_mac_vst_plugin_info = new ARDOUR::PluginInfoList();
	}
#endif //Native Mac VST SUPPORT

#if !defined NDEBUG & (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT)
	if (!cache_only) {
		string fn = Glib::build_filename (ARDOUR::user_cache_directory(), VST2_BLACKLIST);
		if (Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
			DEBUG_TRACE (DEBUG::PluginManager, string_compose ("VST 2 Blacklist: %1\n", fn));
		}
	}
#endif

#ifdef VST3_SUPPORT
	if(Config->get_use_vst3 ()) {
		if (cache_only) {
			BootMessage (_("Scanning VST3 Plugins"));
		} else {
			BootMessage (_("Discovering VST3 Plugins"));
		}
		vst3_refresh (cache_only);
	}

#ifndef NDEBUG
	if (!cache_only) {
		string fn = Glib::build_filename (ARDOUR::user_cache_directory (), VST3_BLACKLIST);
		if (Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
			DEBUG_TRACE (DEBUG::PluginManager, string_compose ("VST3 Blacklist: %1\n", fn));
		}
	}
#endif

	bool conceal_vst2 = Config->get_conceal_vst2_if_vst3_exists();
	if (conceal_vst2) {
		conceal_duplicates (_windows_vst_plugin_info, _vst3_plugin_info);
		conceal_duplicates (_lxvst_plugin_info, _vst3_plugin_info);
		conceal_duplicates (_mac_vst_plugin_info, _vst3_plugin_info);
	}
#else
	bool conceal_vst2 = false;
#endif

#ifdef AUDIOUNIT_SUPPORT

	if (Config->get_use_audio_units ()) {
		if (cache_only) {
			BootMessage (_("Scanning AU Plugins"));
		} else {
			BootMessage (_("Discovering AU Plugins"));
		}
		au_refresh (cache_only);
	}

#ifndef NDEBUG
	if (!cache_only) {
		string fn = Glib::build_filename (ARDOUR::user_cache_directory(), AUV2_BLACKLIST);
		if (Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
			DEBUG_TRACE (DEBUG::PluginManager, string_compose ("AUv2 Blacklist: %1\n", fn));
		}
	}
#endif
#endif

	/* unset concealed plugins */
	if (!conceal_lv1 || !conceal_vst2) {
		for (PluginStatusList::iterator i = statuses.begin(); i != statuses.end();) {
			PluginStatusList::iterator j = i++;
			if ((*j).status != Concealed) {
				continue;
			}
			if (!conceal_lv1 && (*j).type == LADSPA) {
				statuses.erase (j);
			}
			if (!conceal_vst2 && ((*j).type == Windows_VST || (*j).type == LXVST || (*j).type == MacVST)) {
				statuses.erase (j);
			}
		}
	}

	if (!cache_only && !cache_valid () && !cancelled ()) {
		Config->set_plugin_cache_version (cache_version ());
		Config->save_state();
	}

	BootMessage (_("Plugin Scan Complete..."));

	reset_scan_cancel_state ();
	PluginScanMessage(X_("closeme"), "", false);

	BootMessage (_("Indexing Plugins..."));
	detect_ambiguities ();
}

void
PluginManager::detect_ambiguities ()
{
	detect_name_ambiguities (_windows_vst_plugin_info);
	detect_name_ambiguities (_lxvst_plugin_info);
	detect_name_ambiguities (_mac_vst_plugin_info);
	detect_name_ambiguities (_au_plugin_info);
	detect_name_ambiguities (_ladspa_plugin_info);
	detect_name_ambiguities (_lv2_plugin_info);
	detect_name_ambiguities (_lua_plugin_info);
	detect_name_ambiguities (_vst3_plugin_info);

	PluginInfoList all_plugs;
	if (_windows_vst_plugin_info) {
		all_plugs.insert(all_plugs.end(), _windows_vst_plugin_info->begin(), _windows_vst_plugin_info->end());
	}
	if (_lxvst_plugin_info) {
		all_plugs.insert(all_plugs.end(), _lxvst_plugin_info->begin(), _lxvst_plugin_info->end());
	}
	if (_mac_vst_plugin_info) {
		all_plugs.insert(all_plugs.end(), _mac_vst_plugin_info->begin(), _mac_vst_plugin_info->end());
	}
	if (_vst3_plugin_info) {
		all_plugs.insert(all_plugs.end(), _vst3_plugin_info->begin(), _vst3_plugin_info->end());
	}
	if (_au_plugin_info) {
		all_plugs.insert(all_plugs.end(), _au_plugin_info->begin(), _au_plugin_info->end());
	}
	if (_ladspa_plugin_info) {
		all_plugs.insert(all_plugs.end(), _ladspa_plugin_info->begin(), _ladspa_plugin_info->end());
	}
	if (_lv2_plugin_info) {
		all_plugs.insert(all_plugs.end(), _lv2_plugin_info->begin(), _lv2_plugin_info->end());
	}
	if (_lua_plugin_info) {
		all_plugs.insert(all_plugs.end(), _lua_plugin_info->begin(), _lua_plugin_info->end());
	}
	detect_type_ambiguities (all_plugs);

	save_scanlog ();
	PluginListChanged (); /* EMIT SIGNAL */
}

void
PluginManager::enable_scan_timeout ()
{
	_enable_scan_timeout = true;
}

void
PluginManager::cancel_scan_all ()
{
	_cancel_scan_all = true;
}

void
PluginManager::cancel_scan_one ()
{
	_cancel_scan_one = true;
}

void
PluginManager::cancel_scan_timeout_one ()
{
	_cancel_scan_timeout_one = true;
}

void
PluginManager::cancel_scan_timeout_all ()
{
	_cancel_scan_timeout_all = true;
}

void
PluginManager::reset_scan_cancel_state (bool single)
{
	_cancel_scan_one         = false;
	_cancel_scan_timeout_one = false;
	if (single) {
		return;
	}
	_cancel_scan_all         = false;
	_cancel_scan_timeout_all = false;
	_enable_scan_timeout     = false;
}

void
PluginManager::clear_vst_cache ()
{
#if (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT)
	{
		string dn = Glib::build_filename (ARDOUR::user_cache_directory(), "vst");
		vector<string> v2i_files;
		find_files_matching_regex (v2i_files, dn, "\\.v2i$", false);
		for (vector<string>::iterator i = v2i_files.begin(); i != v2i_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
	Config->set_plugin_cache_version (0);
	Config->save_state();
#endif
}

void
PluginManager::clear_vst_blacklist ()
{
#if (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT)
	{
		string fn = Glib::build_filename (ARDOUR::user_cache_directory(), VST2_BLACKLIST);
		if (Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
			::g_unlink (fn.c_str());
		}
	}
#endif
}

void
PluginManager::clear_au_cache ()
{
#ifdef AUDIOUNIT_SUPPORT
	string dn = Glib::build_filename (ARDOUR::user_cache_directory(), "auv2");
	vector<string> a2i_files;
	find_files_matching_regex (a2i_files, dn, "\\.a2i$", false);
	for (vector<string>::iterator i = a2i_files.begin(); i != a2i_files.end (); ++i) {
		::g_unlink(i->c_str());
	}
	Config->set_plugin_cache_version (0);
	Config->save_state();
#endif
}

void
PluginManager::clear_au_blacklist ()
{
#ifdef AUDIOUNIT_SUPPORT
	string fn = Glib::build_filename (ARDOUR::user_cache_directory(), AUV2_BLACKLIST);
	if (Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		::g_unlink(fn.c_str());
	}
#endif
}

void
PluginManager::lua_refresh ()
{
	if (_lua_plugin_info) {
		_lua_plugin_info->clear ();
	} else {
		_lua_plugin_info = new ARDOUR::PluginInfoList ();
	}
	ARDOUR::LuaScriptList & _scripts (LuaScripting::instance ().scripts (LuaScriptInfo::DSP));
	for (LuaScriptList::const_iterator s = _scripts.begin(); s != _scripts.end(); ++s) {
		LuaPluginInfoPtr lpi (new LuaPluginInfo(*s));
		_lua_plugin_info->push_back (lpi);
		set_tags (lpi->type, lpi->unique_id, lpi->category, lpi->name, FromPlug);
	}
}

void
PluginManager::lua_refresh_cb ()
{
	Glib::Threads::Mutex::Lock lm (_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		return;
	}
	lua_refresh ();
	PluginListChanged (); /* EMIT SIGNAL */
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

	size_t n = 1;
	size_t all_modules = ladspa_modules.size ();
	for (vector<std::string>::iterator i = ladspa_modules.begin(); i != ladspa_modules.end(); ++i, ++n) {
		ARDOUR::PluginScanMessage (string_compose (_("LADSPA (%1 / %2)"), n, all_modules), *i, false);
		ladspa_discover (*i);
#ifdef MIXBUS
		if (i->find ("harrison_channelstrip") != std::string::npos) {
			PluginScanLog::iterator j = _plugin_scan_log.find (PSLEPtr (new PluginScanLogEntry (LADSPA, *i)));
			if (j != _plugin_scan_log.end ()) {
				_plugin_scan_log.erase (j);
			}
		}
#endif
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
PluginManager::add_mac_vst_presets()
{
	add_presets ("mac-vst");
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

	PSLEPtr psle (scan_log_entry (LADSPA, path));
	psle->reset ();

#ifdef MIXBUS
	if (path.find ("harrison_channelstrip") != std::string::npos) {
		// always scan, even when cancelled
	} else
#endif
	if (cancelled ()) {
		psle->msg (PluginScanLogEntry::New, "Scan was cancelled.");
		return -1;
	}

	Glib::Module module (path);
	const LADSPA_Descriptor *descriptor;
	LADSPA_Descriptor_Function dfunc;
	void* func = 0;

	if (!module) {
		psle->msg (PluginScanLogEntry::Error, string_compose(_("Cannot load module \"%1\" (%2)"), path, Glib::Module::get_last_error()));
		return -1;
	}


	if (!module.get_symbol("ladspa_descriptor", func)) {
		psle->msg (PluginScanLogEntry::Error, string_compose(_("LADSPA module \"%1\" has no descriptor function (%2)."), path, Glib::Module::get_last_error()));
		return -1;
	}

	dfunc = (LADSPA_Descriptor_Function)func;

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("LADSPA plugin found at %1\n", path));
	psle->msg (PluginScanLogEntry::OK, string_compose ("LADSPA plugin found at %1\n", path));

	uint32_t i = 0;
	for (i = 0; ; ++i) {
		/* if a ladspa plugin allocates memory here
		 * it is never free()ed (or plugin-dependent only when unloading).
		 * For some plugins memory allocated is incremental, we should
		 * avoid re-scanning plugins and file bug reports.
		 */
		if ((descriptor = dfunc (i)) == 0) {
			break;
		}

		if (!ladspa_plugin_whitelist.empty()) {
			if (find (ladspa_plugin_whitelist.begin(), ladspa_plugin_whitelist.end(), descriptor->UniqueID) == ladspa_plugin_whitelist.end()) {
				psle->msg (PluginScanLogEntry::OK, string_compose(_("LADSPA ignored blacklisted unique-id %1."), descriptor->UniqueID));
				continue;
			}
		}

		PluginInfoPtr info(new LadspaPluginInfo);
		info->name = descriptor->Name;
		info->category = get_ladspa_category(descriptor->UniqueID);
		info->path = path;
		info->index = i;
		info->n_inputs = ChanCount();
		info->n_outputs = ChanCount();
		info->type = ARDOUR::LADSPA;

		string::size_type pos = 0;
		string creator = descriptor->Maker;
		/* stupid LADSPA creator strings */
#ifdef PLATFORM_WINDOWS
		while (pos < creator.length() && creator[pos] > -2 && creator[pos] < 256 && (isalnum (creator[pos]) || isspace (creator[pos]) || creator[pos] == '.')) ++pos;
#else
		while (pos < creator.length() && (isalnum (creator[pos]) || isspace (creator[pos]) || creator[pos] == '.')) ++pos;
#endif

		/* If there were too few characters to create a
		 * meaningful name, mark this creator as 'Unknown'
		 */
		if (creator.length() < 2 || pos < 3) {
			info->creator = "Unknown";
		} else{
			info->creator = creator.substr (0, pos);
			strip_whitespace_edges (info->creator);
		}

		char buf[32];
		snprintf (buf, sizeof (buf), "%lu", descriptor->UniqueID);
		info->unique_id = buf;

		for (uint32_t n=0; n < descriptor->PortCount; ++n) {
			if (LADSPA_IS_PORT_AUDIO (descriptor->PortDescriptors[n])) {
				if (LADSPA_IS_PORT_INPUT (descriptor->PortDescriptors[n])) {
					info->n_inputs.set_audio(info->n_inputs.n_audio() + 1);
				}
				else if (LADSPA_IS_PORT_OUTPUT (descriptor->PortDescriptors[n])) {
					info->n_outputs.set_audio(info->n_outputs.n_audio() + 1);
				}
			}
		}

		/* Ensure that the plugin is not already in the plugin list. */
		bool found = false;
		for (PluginInfoList::const_iterator i = _ladspa_plugin_info->begin(); i != _ladspa_plugin_info->end(); ++i) {
			if(0 == info->unique_id.compare((*i)->unique_id)){
				found = true;
			}
		}

		if (!found) {
			_ladspa_plugin_info->push_back (info);
			psle->add (info);
			set_tags (info->type, info->unique_id, info->category, info->name, FromPlug);
			psle->msg (PluginScanLogEntry::OK, string_compose(_("Found LADSPA plugin, id: %1 name: %2, Inputs: %3, Outputs: %4"), info->unique_id, info->name, info->n_inputs, info->n_outputs));
		} else {
			psle->msg (PluginScanLogEntry::OK, string_compose(_("LADSPA ignored plugin with duplicate id %1."), descriptor->UniqueID));
		}

		DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Found LADSPA plugin, id: %1 name: %2, Inputs: %3, Outputs: %4\n",
					info->unique_id, info->name, info->n_inputs, info->n_outputs));
	}

	if (i == 0) {
		psle->msg (PluginScanLogEntry::Error, _("LADSPA no plugins found in module."));
	}

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

void
PluginManager::lv2_plugin (std::string const& uri, PluginScanLogEntry::PluginScanResult sr, std::string const& msg, bool reset)
{
	PSLEPtr psle (scan_log_entry (LV2, uri));
	if (reset) {
		psle->reset ();
	}
	psle->msg (sr, msg);
}

void
PluginManager::lv2_refresh ()
{
	DEBUG_TRACE (DEBUG::PluginManager, "LV2: refresh\n");
	delete _lv2_plugin_info;
	_lv2_plugin_info = LV2PluginInfo::discover (sigc::mem_fun (*this, &PluginManager::lv2_plugin));

	for (PluginInfoList::iterator i = _lv2_plugin_info->begin(); i != _lv2_plugin_info->end(); ++i) {
		PSLEPtr psle (scan_log_entry (LV2, (*i)->unique_id));
		psle->add (*i);
		set_tags ((*i)->type, (*i)->unique_id, (*i)->category, (*i)->name, FromPlug);
	}
}

#ifdef AUDIOUNIT_SUPPORT

static void
auv2_blacklist (std::string const& id)
{
	string fn = Glib::build_filename (ARDOUR::user_cache_directory(), AUV2_BLACKLIST);
	FILE* f = NULL;
	if (! (f = g_fopen (fn.c_str (), "a"))) {
		error << "Cannot append to AU blacklist for '" << id <<"'\n";
		return;
	}
	assert (id.find ("\n") == string::npos);
	fprintf (f, "%s\n", id.c_str ());
	::fclose (f);
}

static void
auv2_whitelist (std::string id)
{
	string fn = Glib::build_filename (ARDOUR::user_cache_directory(), AUV2_BLACKLIST);
	if (!Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		return;
	}

	std::string bl;
	try {
		bl = Glib::file_get_contents (fn);
	} catch (Glib::FileError const& err) {
		return;
	}
	::g_unlink (fn.c_str ());

	assert (id.find("\n") == string::npos);

	id += "\n"; // add separator
	const size_t rpl = bl.find (id);
	if (rpl != string::npos) {
		bl.replace (rpl, id.size (), "");
	}

	if (bl.empty ()) {
		return;
	}
	Glib::file_set_contents (fn, bl);
}

static bool
auv2_is_blacklisted (std::string const& id)
{
	string fn = Glib::build_filename (ARDOUR::user_cache_directory(), AUV2_BLACKLIST);
	if (!Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		return false;
	}

	std::string bl;
	try {
		bl = Glib::file_get_contents (fn);
	} catch (Glib::FileError const& err) {
		return false;
	}
	return bl.find (id + "\n") != string::npos;
}

/* ****************************************************************************/

static void auv2_scanner_log (std::string msg, std::stringstream* ss)
{
	*ss << msg;
}

bool
PluginManager::run_auv2_scanner_app (CAComponentDescription const& desc, AUv2DescStr const& d, PSLEPtr psle) const
{
	char **argp= (char**) calloc (8, sizeof (char*));
	argp[0] = strdup (auv2_scanner_bin_path.c_str ());
	argp[1] = strdup ("-f");
	if (Config->get_verbose_plugin_scan()) {
		argp[2] = strdup ("-v");
	} else {
		argp[2] = strdup ("-f");
	}
	argp[3] = strdup ("--");
	argp[4] = strdup (d.type.c_str());
	argp[5] = strdup (d.subt.c_str());
	argp[6] = strdup (d.manu.c_str());
	argp[7] = 0;

	stringstream scan_log;
	ARDOUR::SystemExec scanner (auv2_scanner_bin_path, argp);
	PBD::ScopedConnection c;
	scanner.ReadStdout.connect_same_thread (c, boost::bind (&auv2_scanner_log, _1, &scan_log));

	if (scanner.start (ARDOUR::SystemExec::MergeWithStdin)) {
		psle->msg (PluginScanLogEntry::Error, string_compose (_("Cannot launch AU scanner app '%1': %2"), auv2_scanner_bin_path, strerror (errno)));
		return false;
	}

	int timeout = _enable_scan_timeout ? 1 + Config->get_plugin_scan_timeout() : 0; /* deciseconds */
	bool notime = (timeout <= 0);

	while (scanner.is_running () && (notime || timeout > 0)) {
		if (!notime && no_timeout ()) {
			notime = true;
			timeout = -1;
		} else if (notime && !no_timeout() && _enable_scan_timeout) {
			notime = false;
			timeout = 1 + Config->get_plugin_scan_timeout ();
		}

		if (timeout > -864000) {
			--timeout;
		}
		ARDOUR::PluginScanTimeout (timeout);
		Glib::usleep (100000);

		if (cancelled () || (!notime && timeout == 0)) {
			scanner.terminate ();
			psle->msg (PluginScanLogEntry::OK, scan_log.str());
			if (cancelled ()) {
				psle->msg (PluginScanLogEntry::New, "Scan was cancelled.");
			} else {
				psle->msg (PluginScanLogEntry::TimeOut, "Scan Timed Out.");
			}
			/* may be partially written */
			g_unlink (auv2_cache_file (desc).c_str ());
			auv2_whitelist (d.to_s ());
			return false;
		}
	}
	psle->msg (PluginScanLogEntry::OK, scan_log.str());
	return true;
}

void
PluginManager::auv2_plugin (CAComponentDescription const& desc, AUv2Info const& nfo)
{
	PSLEPtr psle (scan_log_entry (AudioUnit, auv2_stringify_descriptor (desc)));

	AUPluginInfoPtr info (new AUPluginInfo (boost::shared_ptr<CAComponentDescription> (new CAComponentDescription (desc))));
	psle->msg (PluginScanLogEntry::OK);

	info->unique_id   = nfo.id;
	info->name        = nfo.name;
	info->creator     = nfo.creator;
	info->category    = nfo.category;
	info->version     = nfo.version;
	info->max_outputs = nfo.max_outputs;
	info->io_configs  = nfo.io_configs;

	_au_plugin_info->push_back (info);

	psle->add (info);
}

int
PluginManager::auv2_discover (AUv2DescStr const& d, bool cache_only)
{
	if (!d.valid ()) {
		return -1;
	}

	std::string dstr = d.to_s ();
	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("checking AU plugin at %1\n", dstr));

	PSLEPtr psle (scan_log_entry (AudioUnit, dstr));

	if (auv2_is_blacklisted (dstr)) {
		psle->msg (PluginScanLogEntry::Blacklisted);
		return -1;
	}

	CAComponentDescription desc (d.desc ());

	bool run_scan = false;
	bool is_new   = false;

	string cache_file = auv2_valid_cache_file (desc, false, &is_new);

	if (!cache_only && auv2_scanner_bin_path.empty () && cache_file.empty ()) {
		/* scan in host context */
		psle->reset ();
		auv2_blacklist (dstr);
		psle->msg (PluginScanLogEntry::OK, "(internal scan)");
		if (!auv2_scan_and_cache (desc, sigc::mem_fun (*this, &PluginManager::auv2_plugin), false)) {
			psle->msg (PluginScanLogEntry::Error, "Cannot load AUv2");
			psle->msg (PluginScanLogEntry::Blacklisted);
			return -1;
		}
		psle->msg (PluginScanLogEntry::OK, string_compose (_("Saved AUV2 plugin cache to %1"), auv2_cache_file (desc)));
		auv2_whitelist (dstr);
		return 0;
	}

	XMLTree tree;
	if (cache_file.empty ()) {
		run_scan = true;
	} else if (tree.read (cache_file)) {
		/* valid cache file was found, now check version */
		int cf_version = 0;
		if (!tree.root()->get_property ("version", cf_version) || cf_version < 2) {
			run_scan = true;
		}
	} else {
		/* failed to parse XML */
		run_scan = true;
	}

	if (!cache_only && run_scan) {
		/* re/generate cache file */
		psle->reset ();
		auv2_blacklist (dstr);

		if (!run_auv2_scanner_app (desc, d, psle)) {
			return -1;
		}

		cache_file = auv2_cache_file (desc);

		if (cache_file.empty ()) {
			psle->msg (PluginScanLogEntry::Error, _("Scan Failed."));
			psle->msg (PluginScanLogEntry::Blacklisted);
			return -1;
		}
		/* re-read cache file */
		if (!tree.read (cache_file)) {
			psle->msg (PluginScanLogEntry::Error, string_compose (_("Cannot parse AUv2 cache file '%1' for plugin '%2'"), cache_file, dstr));
			psle->msg (PluginScanLogEntry::Blacklisted);
			return -1;
		}
		run_scan = false; // mark as scanned
	}

	if (cache_file.empty () || run_scan) {
		/* cache file does not exist and cache_only == true,
		 * or cache file is invalid (scan needed)
		 */
		psle->msg (is_new ? PluginScanLogEntry::New : PluginScanLogEntry::Updated);
		return -1;
	}

	auv2_whitelist (dstr);
	psle->set_result (PluginScanLogEntry::OK);

	uint32_t discovered = 0;
	for (XMLNodeConstIterator i = tree.root()->children().begin(); i != tree.root()->children().end(); ++i) {
		try {
			AUv2Info nfo (**i);

			if (nfo.id != dstr) {
				psle->msg (PluginScanLogEntry::Error, string_compose (_("Cache file %1 ID mismatch '%2' vs '%3'"), cache_file, nfo.id, dstr));
				continue;
			}

			auv2_plugin (desc, nfo);
			++discovered;
		} catch (...) {
			psle->msg (PluginScanLogEntry::Error, string_compose (_("Corrupt AUv2 cache file '%1'"), cache_file));
			DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Cannot load AUv2 '%1'\n", dstr));
		}
	}

	return discovered;
}

void
PluginManager::au_refresh (bool cache_only)
{
	assert (Config->get_use_audio_units ());

	DEBUG_TRACE (DEBUG::PluginManager, "AU: refresh\n");
	delete _au_plugin_info;
	_au_plugin_info = new ARDOUR::PluginInfoList();

	ARDOUR::PluginScanMessage(_("AUv2"), _("Indexing"), false);
	/* disable AU in case indexing crashes */
	Config->set_use_audio_units (false);
	Config->save_state();

	string aucrsh = Glib::build_filename (ARDOUR::user_cache_directory(), "au_crash");
	g_file_set_contents (aucrsh.c_str(), "", -1, NULL);

	std::vector<AUv2DescStr> audesc;
	auv2_list_plugins (audesc);

	/* successful, re-enabled AU support */
	Config->set_use_audio_units (true);
	Config->save_state();

	::g_unlink (aucrsh.c_str());

	size_t n = 1;
	size_t all_modules = audesc.size ();
	for (std::vector<AUv2DescStr>::const_iterator i = audesc.begin (); i != audesc.end (); ++i, ++n) {
		reset_scan_cancel_state (true);
		ARDOUR::PluginScanMessage (string_compose (_("AUv2 (%1 / %2)"), n, all_modules), i->to_s(), !cache_only && !cancelled());
		auv2_discover (*i, cache_only || cancelled ());
	}

	for (PluginInfoList::iterator i = _au_plugin_info->begin(); i != _au_plugin_info->end(); ++i) {
		set_tags ((*i)->type, (*i)->unique_id, (*i)->category, (*i)->name, FromPlug);
	}
}

#endif

#if (defined WINDOWS_VST_SUPPORT || defined MACVST_SUPPORT || defined LXVST_SUPPORT)

static bool vst2_is_blacklisted (string const& module_path)
{
	string fn = Glib::build_filename (ARDOUR::user_cache_directory (), VST2_BLACKLIST);
	if (!Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		return false;
	}

	std::string bl;
	try {
		bl = Glib::file_get_contents (fn);
	} catch (Glib::FileError const& err) {
		return false;
	}
	return bl.find (module_path + "\n") != string::npos;
}

static void vst2_blacklist (string const& module_path)
{
	if (module_path.empty () || vst2_is_blacklisted (module_path)) {
		return;
	}
	string fn = Glib::build_filename (ARDOUR::user_cache_directory (), VST2_BLACKLIST);
	FILE* f = NULL;
	if (! (f = g_fopen (fn.c_str (), "a"))) {
		PBD::error << string_compose (_("Cannot write to VST2 blacklist file '%1'"), fn) << endmsg;
		return;
	}
	fprintf (f, "%s\n", module_path.c_str ());
	::fclose (f);
}

static void vst2_whitelist (string module_path)
{
	string fn = Glib::build_filename (ARDOUR::user_cache_directory (), VST2_BLACKLIST);
	if (!Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		return;
	}

	std::string bl;
	try {
		bl = Glib::file_get_contents (fn);
	} catch (Glib::FileError const& err) {
		return;
	}
	::g_unlink (fn.c_str ());

	module_path += "\n"; // add separator
	const size_t rpl = bl.find (module_path);
	if (rpl != string::npos) {
		bl.replace (rpl, module_path.size (), "");
	}
	if (bl.empty ()) {
		return;
	}
	Glib::file_set_contents (fn, bl);
}

static void vst2_scanner_log (std::string msg, std::stringstream* ss)
{
	*ss << msg;
}

bool
PluginManager::run_vst2_scanner_app (std::string path, PSLEPtr psle) const
{
	char **argp= (char**) calloc (5, sizeof (char*));
	argp[0] = strdup (vst2_scanner_bin_path.c_str ());
	argp[1] = strdup ("-f");
	if (Config->get_verbose_plugin_scan()) {
		argp[2] = strdup ("-v");
	} else {
		argp[2] = strdup ("-f");
	}
	argp[3] = strdup (path.c_str ());
	argp[4] = 0;

	stringstream scan_log;
	ARDOUR::SystemExec scanner (vst2_scanner_bin_path, argp);
	PBD::ScopedConnection c;
	scanner.ReadStdout.connect_same_thread (c, boost::bind (&vst2_scanner_log, _1, &scan_log));

	if (scanner.start (ARDOUR::SystemExec::MergeWithStdin)) {
		psle->msg (PluginScanLogEntry::Error, string_compose (_("Cannot launch VST scanner app '%1': %2"), vst2_scanner_bin_path, strerror (errno)));
		return false;
	}

	int timeout = _enable_scan_timeout ? 1 + Config->get_plugin_scan_timeout() : 0; /* deciseconds */
	bool notime = (timeout <= 0);

	while (scanner.is_running () && (notime || timeout > 0)) {
		if (!notime && no_timeout ()) {
			notime = true;
			timeout = -1;
		} else if (notime && !no_timeout() && _enable_scan_timeout) {
			notime = false;
			timeout = 1 + Config->get_plugin_scan_timeout ();
		}

		if (timeout > -864000) {
			--timeout;
		}
		ARDOUR::PluginScanTimeout (timeout);
		Glib::usleep (100000);

		if (cancelled () || (!notime && timeout == 0)) {
			scanner.terminate ();
			psle->msg (PluginScanLogEntry::OK, scan_log.str());
			if (cancelled ()) {
				psle->msg (PluginScanLogEntry::New, "Scan was cancelled.");
			} else {
				psle->msg (PluginScanLogEntry::TimeOut, "Scan Timed Out.");
			}
			/* may be partially written */
			g_unlink (vst2_cache_file (path).c_str ());
			vst2_whitelist (path);
			return false;
		}
	}
	psle->msg (PluginScanLogEntry::OK, scan_log.str());
	return true;
}

bool
PluginManager::vst2_plugin (string const& path, PluginType type, VST2Info const& nfo)
{
	PSLEPtr psle (scan_log_entry (type, path));

	if (!nfo.can_process_replace) {
		psle->msg (PluginScanLogEntry::Error, string_compose (_("plugin '%1' does not support processReplacing, and so cannot be used in %2 at this time"), nfo.name, PROGRAM_NAME));
		return false;
	}

	PluginInfoPtr info;
	ARDOUR::PluginInfoList* plist;

	switch (type) {
#ifdef WINDOWS_VST_SUPPORT
		case ARDOUR::Windows_VST:
			info = PluginInfoPtr(new WindowsVSTPluginInfo (nfo));
			plist = _windows_vst_plugin_info;
			break;
#endif
#ifdef LXVST_SUPPORT
		case ARDOUR::LXVST:
			info = PluginInfoPtr(new LXVSTPluginInfo (nfo));
			plist = _lxvst_plugin_info;
			break;
#endif
#ifdef MACVST_SUPPORT
		case ARDOUR::MacVST:
			info = PluginInfoPtr(new MacVSTPluginInfo (nfo));
			plist = _mac_vst_plugin_info;
			break;
#endif
		default:
			assert (0);
			return false;
	}

	info->path = path;

	/* what a joke freeware VST is */
	if (!strcasecmp ("The Unnamed plugin", info->name.c_str())) {
		info->name = PBD::basename_nosuffix (path);
	}

	/* Make sure we don't find the same plugin in more than one place along
	 * the LXVST_PATH We can't use a simple 'find' because the path is included
	 * in the PluginInfo, and that is the one thing we can be sure MUST be
	 * different if a duplicate instance is found. So we just compare the type
	 * and unique ID (which for some VSTs isn't actually unique...)
	 */

	bool duplicate = false;
	if (!plist->empty()) {
		for (PluginInfoList::iterator i =plist->begin(); i != plist->end(); ++i) {
			if ((info->type == (*i)->type) && (info->unique_id == (*i)->unique_id)) {
				psle->msg (PluginScanLogEntry::Error, string_compose (_("Ignoring plugin '%1'. VST-ID conflicts with other plugin '%2' files: '%3' vs '%4'"), info->name, (*i)->name, info->path, (*i)->path));
				duplicate = true;
				continue;
			}
		}
	}

	if (duplicate) {
		return false;
	}

	plist->push_back (info);
	psle->add (info);

	if (!info->category.empty ()) {
		set_tags (info->type, info->unique_id, info->category, info->name, FromPlug);
	}
	return true;
}

int
PluginManager::vst2_discover (string path, ARDOUR::PluginType type, bool cache_only)
{
	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("checking apparent VST plugin at %1\n", path));

	PSLEPtr psle (scan_log_entry (type, path));

	if (vst2_is_blacklisted (path)) {
		psle->msg (PluginScanLogEntry::Blacklisted);
		return -1;
	}

	bool run_scan = false;
	bool is_new   = false;

	string cache_file = vst2_valid_cache_file (path, false, &is_new);

	if (!cache_only && vst2_scanner_bin_path.empty () && cache_file.empty ()) {
		/* scan in host context */
		psle->reset ();
		vst2_blacklist (path);
		psle->msg (PluginScanLogEntry::OK, string_compose ("VST2 plugin: '%1' (internal scan)", path));

		if (!vst2_scan_and_cache (path, type, sigc::mem_fun (*this, &PluginManager::vst2_plugin))) {
			psle->msg (PluginScanLogEntry::Error, "Cannot load VST2");
			psle->msg (PluginScanLogEntry::Blacklisted);
			DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Cannot load VST2 at '%1'\n", path));
			return -1;
		}
		psle->msg (PluginScanLogEntry::OK, string_compose (_("Saved VST2 plugin cache to '%1'"), vst2_cache_file (path)));
		vst2_whitelist (path);
		return 0;
	}


	XMLTree tree;
	if (cache_file.empty ()) {
		run_scan = true;
	} else if (tree.read (cache_file)) {
		/* valid cache file was found, now check version */
		int cf_version = 0;
		if (!tree.root()->get_property ("version", cf_version) || cf_version < 1) {
			run_scan = true;
		}
	} else {
		/* failed to parse XML */
		run_scan = true;
	}

	if (!cache_only && run_scan) {
		/* re/generate cache file */
		psle->reset ();
		vst2_blacklist (path);

		if (!run_vst2_scanner_app (path, psle)) {
			return -1;
		}

		cache_file = vst2_valid_cache_file (path);

		if (cache_file.empty ()) {
			psle->msg (PluginScanLogEntry::Error, _("Scan Failed."));
			psle->msg (PluginScanLogEntry::Blacklisted);
			return -1;
		}
		/* re-read cache file */
		if (!tree.read (cache_file)) {
			psle->msg (PluginScanLogEntry::Error, string_compose (_("Cannot parse VST2 cache file '%1' for plugin '%2'"), cache_file, path));
			psle->msg (PluginScanLogEntry::Blacklisted);
			return -1;
		}
		run_scan = false; // mark as scanned
	}

	if (cache_file.empty () || run_scan) {
		/* cache file does not exist and cache_only == true,
		 * or cache file is invalid (scan needed)
		 */
		psle->msg (is_new ? PluginScanLogEntry::New : PluginScanLogEntry::Updated);
		return -1;
	}

	std::string binary;
	if (!tree.root()->get_property ("binary", binary) || binary != path) {
		psle->msg (PluginScanLogEntry::Incompatible, string_compose (_("Invalid VST2 cache file '%1'"), cache_file)); // XXX log as error msg
		psle->msg (PluginScanLogEntry::Blacklisted);
		vst2_blacklist (path);
		return -1;
	}

	std::string arch;
	if (!tree.root()->get_property ("arch", arch) || arch != vst2_arch ()) {
		vst2_blacklist (path);
		psle->msg (PluginScanLogEntry::Blacklisted);
		psle->msg (PluginScanLogEntry::Incompatible, string_compose (_("VST2 architecture mismatches '%1'"), arch));
		return -1;
	}

	vst2_whitelist (path);
	psle->set_result (PluginScanLogEntry::OK);

	uint32_t discovered = 0;
	for (XMLNodeConstIterator i = tree.root()->children().begin(); i != tree.root()->children().end(); ++i) {
		try {
			VST2Info nfo (**i);
			if (vst2_plugin (path, type, nfo)) {
				++discovered;
			} else {
				psle->msg (PluginScanLogEntry::Blacklisted);
				vst2_blacklist (path);
			}
		} catch (...) {
			psle->msg (PluginScanLogEntry::Error, string_compose (_("Corrupt VST2 cache file '%1'"), cache_file));
			DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Cannot load VST2 at '%1'\n", path));
			continue;
		}
	}

	return discovered;
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
	if (!cache_only) {
		/* ensure that VST path is flushed to disk */
		Config->save_state();
	}
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

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Discovering Windows VST plugins along %1\n", path));

	if (Session::get_disable_all_loaded_plugins ()) {
		info << _("Disabled WindowsVST scan (safe mode)") << endmsg;
		return -1;
	}

	find_files_matching_filter (plugin_objects, path, windows_vst_filter, 0, false, true, true);

	sort (plugin_objects.begin (), plugin_objects.end ());
	plugin_objects.erase (unique (plugin_objects.begin (), plugin_objects.end ()), plugin_objects.end ());

	size_t n = 1;
	size_t all_modules = plugin_objects.size ();
	for (x = plugin_objects.begin(); x != plugin_objects.end (); ++x, ++n) {
		reset_scan_cancel_state (true);
		ARDOUR::PluginScanMessage (string_compose (_("VST2 (%1 / %2)"), n, all_modules), *x, !cache_only && !cancelled());
		vst2_discover (*x, Windows_VST, cache_only || cancelled());
	}

	return ret;
}
#endif // WINDOWS_VST_SUPPORT

#ifdef MACVST_SUPPORT
void
PluginManager::mac_vst_refresh (bool cache_only)
{
	if (_mac_vst_plugin_info) {
		_mac_vst_plugin_info->clear ();
	} else {
		_mac_vst_plugin_info = new ARDOUR::PluginInfoList();
	}

	mac_vst_discover_from_path ("~/Library/Audio/Plug-Ins/VST:/Library/Audio/Plug-Ins/VST", cache_only);
	if (!cache_only) {
		/* ensure that VST path is flushed to disk */
		Config->save_state();
	}
}

static bool mac_vst_filter (const string& str)
{
	string plist = Glib::build_filename (str, "Contents", "Info.plist");
	if (!Glib::file_test (plist, Glib::FILE_TEST_IS_REGULAR)) {
		return false;
	}
	return str[0] != '.' && str.length() > 4 && strings_equal_ignore_case (".vst", str.substr(str.length() - 4));
}

int
PluginManager::mac_vst_discover_from_path (string path, bool cache_only)
{
	vector<string> plugin_objects;
	vector<string>::iterator x;

	if (Session::get_disable_all_loaded_plugins ()) {
		info << _("Disabled MacVST scan (safe mode)") << endmsg;
		return -1;
	}

	Searchpath paths (path);
	/* customized version of run_functor_for_paths() */
	for (vector<string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
		string expanded_path = path_expand (*i);
		if (!Glib::file_test (expanded_path, Glib::FILE_TEST_IS_DIR)) continue;
		try {
			Glib::Dir dir(expanded_path);
			for (Glib::DirIterator di = dir.begin(); di != dir.end(); di++) {
				string fullpath = Glib::build_filename (expanded_path, *di);

				/* we're only interested in bundles */
				if (!Glib::file_test (fullpath, Glib::FILE_TEST_IS_DIR)) {
					continue;
				}

				if (mac_vst_filter (fullpath)) {
					plugin_objects.push_back (fullpath);
					continue;
				}

				/* don't descend into AU bundles in the VST dir */
				if (fullpath[0] == '.' || (fullpath.length() > 10 && strings_equal_ignore_case (".component", fullpath.substr(fullpath.length() - 10)))) {
					continue;
				}

				/* recurse */
				mac_vst_discover_from_path (fullpath, cache_only);
			}
		} catch (Glib::FileError& err) { }
	}

	sort (plugin_objects.begin (), plugin_objects.end ());
	plugin_objects.erase (unique (plugin_objects.begin (), plugin_objects.end ()), plugin_objects.end ());

	size_t n = 1;
	size_t all_modules = plugin_objects.size ();
	for (x = plugin_objects.begin(); x != plugin_objects.end (); ++x, ++n) {
		reset_scan_cancel_state (true);
		ARDOUR::PluginScanMessage (string_compose (_("VST2 (%1 / %2)"), n, all_modules), *x, !cache_only && !cancelled());
		vst2_discover (*x, MacVST, cache_only || cancelled());
	}

	return 0;
}

#endif // MAC_VST_SUPPORT

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
	if (!cache_only) {
		/* ensure that VST path is flushed to disk */
		Config->save_state();
	}
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

	if (Session::get_disable_all_loaded_plugins ()) {
		info << _("Disabled LinuxVST scan (safe mode)") << endmsg;
		return -1;
	}

#ifndef NDEBUG
	(void) path;
#endif

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Discovering linuxVST plugins along %1\n", path));

	find_files_matching_filter (plugin_objects, Config->get_plugin_path_lxvst(), lxvst_filter, 0, false, true, true);

	sort (plugin_objects.begin (), plugin_objects.end ());
	plugin_objects.erase (unique (plugin_objects.begin (), plugin_objects.end ()), plugin_objects.end ());

	size_t n = 1;
	size_t all_modules = plugin_objects.size ();
	for (x = plugin_objects.begin(); x != plugin_objects.end (); ++x, ++n) {
		reset_scan_cancel_state (true);
		ARDOUR::PluginScanMessage (string_compose (_("VST2 (%1 / %2)"), n, all_modules), *x, !cache_only && !cancelled());
		vst2_discover (*x, LXVST, cache_only || cancelled());
	}

	return 0;
}

#endif // LXVST_SUPPORT

void
PluginManager::clear_vst3_cache ()
{
#ifdef VST3_SUPPORT
	string dn = Glib::build_filename (ARDOUR::user_cache_directory(), "vst");
	vector<string> v3i_files;
	find_files_matching_regex (v3i_files, dn, "\\.v3i$", false);
	for (vector<string>::iterator i = v3i_files.begin(); i != v3i_files.end (); ++i) {
		::g_unlink(i->c_str());
	}
	Config->set_plugin_cache_version (0);
	Config->save_state();
#endif
}

void
PluginManager::clear_vst3_blacklist ()
{
#ifdef VST3_SUPPORT
	string fn = Glib::build_filename (ARDOUR::user_cache_directory (), VST3_BLACKLIST);
	if (Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		::g_unlink(fn.c_str());
	}
#endif
}

#ifdef VST3_SUPPORT

static bool vst3_is_blacklisted (string const& module_path)
{
	string fn = Glib::build_filename (ARDOUR::user_cache_directory (), VST3_BLACKLIST);
	if (!Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		return false;
	}

	std::string bl;
	try {
		bl = Glib::file_get_contents (fn);
	} catch (Glib::FileError const& err) {
		return false;
	}
	return bl.find (module_path + "\n") != string::npos;
}

static void vst3_blacklist (string const& module_path)
{
	if (module_path.empty () || vst3_is_blacklisted (module_path)) {
		return;
	}
	string fn = Glib::build_filename (ARDOUR::user_cache_directory (), VST3_BLACKLIST);
	FILE* f = NULL;
	if (! (f = g_fopen (fn.c_str (), "a"))) {
		PBD::error << string_compose (_("Cannot write to VST3 blacklist file '%1'"), fn) << endmsg;
		return;
	}
	fprintf (f, "%s\n", module_path.c_str ());
	::fclose (f);
}

static void vst3_whitelist (string module_path)
{
	if (module_path.empty ()) {
		return;
	}

	string fn = Glib::build_filename (ARDOUR::user_cache_directory (), VST3_BLACKLIST);
	if (!Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		return;
	}

	std::string bl;
	try {
		bl = Glib::file_get_contents (fn);
	} catch (Glib::FileError const& err) {
		return;
	}
	::g_unlink (fn.c_str ());

	module_path += "\n"; // add separator
	const size_t rpl = bl.find (module_path);
	if (rpl != string::npos) {
		bl.replace (rpl, module_path.size (), "");
	}
	if (bl.empty ()) {
		return;
	}
	Glib::file_set_contents (fn, bl);
}

static bool vst3_filter (const string& str, void*)
{
	return str[0] != '.' && (str.length() > 4 && str.find (".vst3") == (str.length() - 5));
}

void
PluginManager::vst3_refresh (bool cache_only)
{
	if (_vst3_plugin_info) {
		_vst3_plugin_info->clear ();
	} else {
		_vst3_plugin_info = new ARDOUR::PluginInfoList();
	}

#ifdef __APPLE__
	vst3_discover_from_path ("~/Library/Audio/Plug-Ins/VST3:/Library/Audio/Plug-Ins/VST3", cache_only);
#elif defined PLATFORM_WINDOWS
	std::string prog = PBD::get_win_special_folder_path (CSIDL_PROGRAM_FILES);
	vst3_discover_from_path (Glib::build_filename (prog, "Common Files", "VST3"), cache_only);
#else
	vst3_discover_from_path ("~/.vst3:/usr/local/lib/vst3:/usr/lib/vst3", cache_only);
#endif
}

int
PluginManager::vst3_discover_from_path (string const& path, bool cache_only)
{
	if (Session::get_disable_all_loaded_plugins ()) {
		info << _("Disabled VST3 scan (safe mode)") << endmsg;
		return -1;
	}

	Searchpath paths (path);
	if (!Config->get_plugin_path_vst3().empty ()) {
		Searchpath custom (Config->get_plugin_path_vst3 ());
		paths += custom;
	}

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("VST3: search along: [%1]\n", paths.to_string ()));

	vector<string> plugin_objects;

	find_paths_matching_filter (plugin_objects, paths, vst3_filter, 0, false, true, true);

	size_t n = 1;
	size_t all_modules = plugin_objects.size ();
	for (vector<string>::iterator i = plugin_objects.begin(); i != plugin_objects.end (); ++i, ++n) {
		reset_scan_cancel_state (true);
		ARDOUR::PluginScanMessage (string_compose (_("VST3 (%1 / %2)"), n, all_modules), *i, !cache_only && !cancelled());
		vst3_discover (*i, cache_only || cancelled ());
	}

	return cancelled() ? -1 : 0;
}

void
PluginManager::vst3_plugin (string const& module_path, string const& bundle_path, VST3Info const& i)
{
	PluginInfoPtr info (new VST3PluginInfo ());

	info->path      = module_path;
	info->index     = i.index;
	info->unique_id = i.uid;
	info->name      = i.name;
	info->category  = i.category; // TODO process split at "|" -> tags
	info->creator   = i.vendor;
	info->n_inputs  = ChanCount();
	info->n_outputs = ChanCount();

	info->n_inputs.set_audio (i.n_inputs + i.n_aux_inputs);
	info->n_inputs.set_midi (i.n_midi_inputs);

	info->n_outputs.set_audio (i.n_outputs + i.n_aux_outputs);
	info->n_outputs.set_midi (i.n_midi_outputs);

	_vst3_plugin_info->push_back (info);

	scan_log_entry (VST3, bundle_path)->add (info);

	if (!info->category.empty ()) {
		set_tags (info->type, info->unique_id, info->category, info->name, FromPlug);
	}
}

int
PluginManager::vst3_discover (string const& path, bool cache_only)
{
	string module_path = module_path_vst3 (path);
	if (module_path.empty ()) {
		PSLEPtr psl = PSLEPtr (new PluginScanLogEntry (VST3, path));
		psl->msg (PluginScanLogEntry::Error, "Invalid Module Path");
		_plugin_scan_log.erase (psl);
		_plugin_scan_log.insert (psl);
		return -1;
	}

	PSLEPtr psle (scan_log_entry (VST3, path));

	if (vst3_is_blacklisted (module_path)) {
		psle->msg (PluginScanLogEntry::Blacklisted);
		return -1;
	}

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("VST3: discover %1 (%2)\n", path, module_path));

	bool run_scan = false;
	bool is_new   = false;

	string cache_file = vst3_valid_cache_file (module_path, false, &is_new);

	if (!cache_only && vst3_scanner_bin_path.empty () && cache_file.empty ()) {
		/* scan in host context */
		psle->reset ();
		vst3_blacklist (module_path);
		psle->msg (PluginScanLogEntry::OK, string_compose ("VST3 module-path: '%1' (internal scan)", module_path));
		if (! vst3_scan_and_cache (module_path, path, sigc::mem_fun (*this, &PluginManager::vst3_plugin))) {
			psle->msg (PluginScanLogEntry::Error, "Cannot load VST3");
			psle->msg (PluginScanLogEntry::Blacklisted);
			DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Cannot load VST3 at '%1'\n", path));
			return -1;
		}
		psle->msg (PluginScanLogEntry::OK, string_compose (_("Saved VST3 plugin cache to '%1'"), vst3_cache_file (module_path)));
		vst3_whitelist (module_path);
		return 0;
	}

	XMLTree tree;
	if (cache_file.empty ()) {
		run_scan = true;
	} else if (tree.read (cache_file)) {
		/* valid cache file was found, now check version
		 * see ARDOUR::vst3_scan_and_cache VST3Cache version
		 */
		int cf_version = 0;
		if (!tree.root()->get_property ("version", cf_version) || cf_version < 1) {
			run_scan = true;
		}
	} else {
		/* failed to parse XML */
		run_scan = true;
	}

	if (!cache_only && run_scan) {
		/* re/generate cache file */
		psle->reset ();
		vst3_blacklist (module_path);
		psle->msg (PluginScanLogEntry::OK, string_compose ("VST3 module-path '%1'", module_path));
		if (!run_vst3_scanner_app (path, psle)) {
			return -1;
		}

		cache_file = vst3_valid_cache_file (module_path);

		if (cache_file.empty ()) {
			psle->msg (PluginScanLogEntry::Blacklisted);
			psle->msg (PluginScanLogEntry::Error, _("Scan Failed."));
			return -1;
		}
		/* re-read cache file */
		if (!tree.read (cache_file)) {
			psle->msg (PluginScanLogEntry::Blacklisted);
			psle->msg (PluginScanLogEntry::Error, string_compose (_("Cannot parse VST3 cache file '%1' for plugin '%2'"), cache_file, module_path));
			return -1;
		}
		run_scan = false; // mark as scanned
	}

	if (cache_file.empty () || run_scan) {
		/* cache file does not exist and cache_only == true,
		 * or cache file is invalid (scan needed)
		 */
		psle->msg (is_new ? PluginScanLogEntry::New : PluginScanLogEntry::Updated);
		return -1;
	}

	std::string module;
	if (!tree.root()->get_property ("module", module) || module != module_path) {
		psle->msg (PluginScanLogEntry::Error, string_compose (_("Invalid VST3 cache file '%1'"), cache_file));
		psle->msg (PluginScanLogEntry::Blacklisted);
		if (!vst3_is_blacklisted (path)) {
			vst3_blacklist (module_path);
		}
		return -1;
	}

	vst3_whitelist (module_path);
	psle->set_result (PluginScanLogEntry::OK);

	for (XMLNodeConstIterator i = tree.root()->children().begin(); i != tree.root()->children().end(); ++i) {
		try {
			VST3Info nfo (**i);
			vst3_plugin (module_path, path, nfo);
		} catch (...) {
			psle->msg (PluginScanLogEntry::Error, string_compose (_("Corrupt VST3 cache file '%1'"), cache_file));
			DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Cannot load VST3 at '%1'\n", module_path));
			continue;
		}
	}
	return 0;
}

static void vst3_scanner_log (std::string msg, std::stringstream* ss)
{
	*ss << msg;
}

bool
PluginManager::run_vst3_scanner_app (std::string bundle_path, PSLEPtr psle) const
{
	char **argp= (char**) calloc (5, sizeof (char*));
	argp[0] = strdup (vst3_scanner_bin_path.c_str ());
	argp[1] = strdup ("-f");
	if (Config->get_verbose_plugin_scan()) {
		argp[2] = strdup ("-v");
	} else {
		argp[2] = strdup ("-f");
	}
	argp[3] = strdup (bundle_path.c_str ());
	argp[4] = 0;

	stringstream scan_log;
	ARDOUR::SystemExec scanner (vst3_scanner_bin_path, argp);
	PBD::ScopedConnection c;
	scanner.ReadStdout.connect_same_thread (c, boost::bind (&vst3_scanner_log, _1, &scan_log));

	if (scanner.start (ARDOUR::SystemExec::MergeWithStdin)) {
		psle->msg (PluginScanLogEntry::Error, string_compose (_("Cannot launch VST scanner app '%1': %2"), vst3_scanner_bin_path, strerror (errno)));
		return false;
	}

	int timeout = _enable_scan_timeout ? 1 + Config->get_plugin_scan_timeout() : 0; /* deciseconds */
	bool notime = (timeout <= 0);

	while (scanner.is_running () && (notime || timeout > 0)) {
		if (!notime && no_timeout ()) {
			notime = true;
			timeout = -1;
		} else if (notime && !no_timeout() && _enable_scan_timeout) {
			notime = false;
			timeout = 1 + Config->get_plugin_scan_timeout ();
		}

		if (timeout > -864000) {
			--timeout;
		}
		ARDOUR::PluginScanTimeout (timeout);
		Glib::usleep (100000);

		if (cancelled () || (!notime && timeout == 0)) {
			scanner.terminate ();
			psle->msg (PluginScanLogEntry::OK, scan_log.str());
			if (cancelled ()) {
				psle->msg (PluginScanLogEntry::New, "Scan was cancelled.");
			} else {
				psle->msg (PluginScanLogEntry::TimeOut, "Scan Timed Out.");
			}
			/* may be partially written */
			std::string module_path = module_path_vst3 (bundle_path);
			if (!module_path.empty ()) {
				g_unlink (vst3_cache_file (module_path).c_str ());
			}
			vst3_whitelist (module_path);
			return false;
		}
	}
	psle->msg (PluginScanLogEntry::OK, scan_log.str());
	return true;
}

#endif // VST3_SUPPORT

PluginManager::PluginStatusType
PluginManager::get_status (const PluginInfoPtr& pi) const
{
	PluginStatus ps (pi->type, pi->unique_id);
	PluginStatusList::const_iterator i = find (statuses.begin(), statuses.end(), ps);
	if (i == statuses.end()) {
		return Normal;
	} else {
		return i->status;
	}
}

void
PluginManager::save_statuses ()
{
	std::string path = Glib::build_filename (user_plugin_metadata_dir(), "plugin_statuses");
	stringstream ofs;

	for (PluginStatusList::iterator i = statuses.begin(); i != statuses.end(); ++i) {
		if ((*i).status == Concealed) {
			continue;
		}
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
		case MacVST:
			ofs << "MacVST";
			break;
		case VST3:
			ofs << "VST3";
			break;
		case Lua:
			ofs << "Lua";
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
		case Concealed:
			ofs << "Hidden";
			assert (0);
			break;
		}

		ofs << ' ';

		ofs << (*i).unique_id;;
		ofs << endl;
	}
	g_file_set_contents (path.c_str(), ofs.str().c_str(), -1, NULL);
}

void
PluginManager::load_statuses ()
{
	std::string path;
	find_file (plugin_metadata_search_path(), "plugin_statuses", path); // note: if no user folder is found, this will find the resources path
	gchar *fbuf = NULL;
	if (!g_file_get_contents (path.c_str(), &fbuf, NULL, NULL)) {
		return;
	}
	stringstream ifs (fbuf);
	g_free (fbuf);

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
			error << string_compose (_("unknown plugin status type \"%1\" - all entries ignored"), sstatus) << endmsg;
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
		} else if (stype == "MacVST") {
			type = MacVST;
		} else if (stype == "Lua") {
			type = Lua;
		} else if (stype == "VST3") {
			type = VST3;
		} else {
			error << string_compose (_("unknown plugin type \"%1\" - ignored"), stype)
			      << endmsg;
			continue;
		}

		id = buf;
		strip_whitespace_edges (id);
		set_status (type, id, status);
	}
}

void
PluginManager::set_status (PluginType t, string const& id, PluginStatusType status)
{
	PluginStatus ps (t, id, status);
	statuses.erase (ps);

	if (status != Normal && status != Concealed) {
		statuses.insert (ps);
	}

	PluginStatusChanged (t, id, status); /* EMIT SIGNAL */
}

void
PluginManager::save_stats ()
{
	// TODO: consider thinning out the list
	// (LRU > 1 year? or LRU > 2 weeks && use_count < 10% of avg use-count)
	std::string path = Glib::build_filename (user_plugin_metadata_dir(), "plugin_stats");
	XMLNode* root = new XMLNode (X_("PluginStats"));

	for (PluginStatsList::iterator i = statistics.begin(); i != statistics.end(); ++i) {
		XMLNode* node = root->add_child (X_("Plugin"));
		node->set_property (X_("type"), (*i).type);
		node->set_property (X_("id"), (*i).unique_id);
		node->set_property (X_("lru"), (*i).lru);
		node->set_property (X_("use-count"), (*i).use_count);
	}

	XMLTree tree;
	tree.set_root (root);
	if (!tree.write (path)) {
		error << string_compose (_("Could not save Plugin Statistics to %1"), path) << endmsg;
	}
}

void
PluginManager::load_stats ()
{
	std::string path = Glib::build_filename (user_plugin_metadata_dir(), "plugin_stats");
	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		return;
	}
	info << string_compose (_("Loading plugin statistics file %1"), path) << endmsg;

	XMLTree tree;
	if (!tree.read (path)) {
		error << string_compose (_("Cannot parse plugin statistics from %1"), path) << endmsg;
		return;
	}

	PluginStatsList stats;

	float avg_age = 0;
	float avg_cnt = 0;

	for (XMLNodeConstIterator i = tree.root()->children().begin(); i != tree.root()->children().end(); ++i)
	{
		PluginType type;
		string     id;
		int64_t    lru;
		uint64_t   use_count;

		if (!(*i)->get_property (X_("type"), type) ||
				!(*i)->get_property (X_("id"), id) ||
				!(*i)->get_property (X_("lru"), lru) ||
				!(*i)->get_property (X_("use-count"), use_count)) {
			continue;
		}
		avg_age += lru;
		avg_cnt += use_count;
		PluginStats ps (type, id, lru, use_count);
		stats.insert (ps);
	}

	if (stats.size () > 0) {
		avg_age /= stats.size ();
		avg_cnt /= stats.size ();
	}

	statistics.clear ();
	for (PluginStatsList::iterator i = stats.begin(); i != stats.end(); ++i) {
		/* we use average age in case ardour has not been used for a while,
		 * ignoring old plugins changes the average age, so we only flush
		 * the least use plugins.
		 */
		if (i->lru + 2592000 /*30 days*/ < avg_age && i->use_count < avg_cnt / 2) {
#ifndef NDEBUG
			std::cout << "- Zero stats of plugin '" << i->unique_id << "' use-count: " << i->use_count << " LRU: " << (time(NULL) - i->lru) / 3600 << std::endl;
#endif
			continue;
		}
		if (i->lru + 604800 /*7 days*/ < avg_age && i->use_count < 2) {
#ifndef NDEBUG
			std::cout << "- Zero stats of plugin '" << i->unique_id << "' use-count: " << i->use_count << " LRU: " << (time(NULL) - i->lru) / 3600 << std::endl;
#endif
			continue;
		}
		statistics.insert (*i);
	}
}

void
PluginManager::reset_stats ()
{
	statistics.clear ();
	PluginStatsChanged (); /* EMIT SIGNAL */
	save_stats ();
}

void
PluginManager::stats_use_plugin (PluginInfoPtr const& pip)
{
	PluginStats ps (pip->type, pip->unique_id, time (NULL));
	PluginStatsList::const_iterator i = find (statistics.begin(), statistics.end(), ps);
	if (i == statistics.end()) {
		ps.use_count = 1;
		statistics.insert (ps);
	} else {
		ps.use_count = i->use_count + 1;
		statistics.erase (ps);
		statistics.insert (ps);
	}
	PluginStatsChanged (); /* EMIT SIGNAL */
	save_stats (); // XXX
}

bool
PluginManager::stats (PluginInfoPtr const& pip, int64_t& lru, uint64_t& use_count) const
{
	PluginStats ps (pip->type, pip->unique_id, time (NULL));
	PluginStatsList::const_iterator i = find (statistics.begin(), statistics.end(), ps);
	if (i == statistics.end()) {
		return false;
	}
	lru = i->lru;
	use_count = i->use_count;
	return true;
}

PluginType
PluginManager::to_generic_vst (const PluginType t)
{
	switch (t) {
		case Windows_VST:
		case LXVST:
		case MacVST:
			return LXVST;
		default:
			break;
	}
	return t;
}

std::string
PluginManager::plugin_type_name (const PluginType t, bool short_name)
{
#if defined WINDOWS_VST_SUPPORT && defined LXVST_SUPPORT
	switch (t) {
		case Windows_VST:
			return short_name ? "VST" : "Windows-VST";
		case LXVST:
			return short_name ? "LXVST" : "Linux-VST";
		default:
			break;
	}
#endif

	switch (t) {
		case Windows_VST:
		case LXVST:
		case MacVST:
			return short_name ? "VST" : "VST2";
		case AudioUnit:
			return short_name ? "AU" : enum_2_string (t);
		case LADSPA:
			return short_name ? "LV1" : enum_2_string (t);
		default:
			return enum_2_string (t);
	}
}

struct SortByTag {
	bool operator() (std::string a, std::string b) {
		return a.compare (b) < 0;
	}
};

vector<std::string>
PluginManager::get_tags (const PluginInfoPtr& pi) const
{
	vector<std::string> tags;

	PluginTag ps (to_generic_vst(pi->type), pi->unique_id, "", "", FromPlug);
	PluginTagList::const_iterator i = find (ptags.begin(), ptags.end(), ps);
	if (i != ptags.end ()) {
		PBD::tokenize (i->tags, string(" "), std::back_inserter (tags), true);
		SortByTag sorter;
		sort (tags.begin(), tags.end(), sorter);
	}
	return tags;
}

std::string
PluginManager::get_tags_as_string (PluginInfoPtr const& pi) const
{
	std::string ret;

	vector<std::string> tags = get_tags(pi);
	for (vector<string>::iterator t = tags.begin(); t != tags.end(); ++t) {
		if (t != tags.begin ()) {
			ret.append(" ");
		}
		ret.append(*t);
	}

	return ret;
}

std::string
PluginManager::user_plugin_metadata_dir () const
{
	std::string dir = Glib::build_filename (user_config_directory(), plugin_metadata_dir_name);
	g_mkdir_with_parents (dir.c_str(), 0744);
	return dir;
}

bool
PluginManager::load_plugin_order_file (XMLNode &n) const
{
	std::string path = Glib::build_filename (user_plugin_metadata_dir(), "plugin_order");

	info << string_compose (_("Loading plugin order file %1"), path) << endmsg;
	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		return false;
	}

	XMLTree tree;
	if (tree.read (path)) {
		n = *(tree.root());
		return true;
	} else {
		error << string_compose (_("Cannot parse Plugin Order info from %1"), path) << endmsg;
		return false;
	}
}


void
PluginManager::save_plugin_order_file (XMLNode &elem) const
{
	std::string path = Glib::build_filename (user_plugin_metadata_dir(), "plugin_order");

	XMLTree tree;
	tree.set_root (&elem);
	if (!tree.write (path)) {
		error << string_compose (_("Could not save Plugin Order info to %1"), path) << endmsg;
	}
	tree.set_root (0); // note: must disconnect the elem from XMLTree, or it will try to delete memory it didn't allocate
}


std::string
PluginManager::dump_untagged_plugins ()
{
	std::string path = Glib::build_filename (user_plugin_metadata_dir(), "untagged_plugins");
	XMLNode* root = new XMLNode (X_("PluginTags"));

	for (PluginTagList::iterator i = ptags.begin(); i != ptags.end(); ++i) {
#ifdef MIXBUS
		if ((*i).type == LADSPA) {
			uint32_t id = atoi ((*i).unique_id);
			if (id >= 9300 && id <= 9399) {
				continue; /* do not write mixbus channelstrip ladspa's in the tagfile */
			}
		}
#endif
		if ((*i).tagtype == FromPlug) {
			XMLNode* node = new XMLNode (X_("Plugin"));
			node->set_property (X_("type"), to_generic_vst ((*i).type));
			node->set_property (X_("id"), (*i).unique_id);
			node->set_property (X_("tags"), (*i).tags);
			node->set_property (X_("name"), (*i).name);
			root->add_child_nocopy (*node);
		}
	}

	XMLTree tree;
	tree.set_root (root);
	if (tree.write (path)) {
		return path;
	} else {
		return "";
	}
}

void
PluginManager::save_tags ()
{
	std::string path = Glib::build_filename (user_plugin_metadata_dir(), "plugin_tags");
	XMLNode* root = new XMLNode (X_("PluginTags"));

	for (PluginTagList::iterator i = ptags.begin(); i != ptags.end(); ++i) {
#ifdef MIXBUS
		if ((*i).type == LADSPA) {
			uint32_t id = atoi ((*i).unique_id);
			if (id >= 9300 && id <= 9399) {
				continue; /* do not write mixbus channelstrip ladspa's in the tagfile */
			}
		}
#endif
		if ((*i).tagtype <= FromFactoryFile) {
			/* user file should contain only plugins that are user-tagged */
			continue;
		}
		XMLNode* node = new XMLNode (X_("Plugin"));
		node->set_property (X_("type"), to_generic_vst ((*i).type));
		node->set_property (X_("id"), (*i).unique_id);
		node->set_property (X_("tags"), (*i).tags);
		node->set_property (X_("name"), (*i).name);
		node->set_property (X_("user-set"), "1");
		root->add_child_nocopy (*node);
	}

	XMLTree tree;
	tree.set_root (root);
	if (!tree.write (path)) {
		error << string_compose (_("Could not save Plugin Tags info to %1"), path) << endmsg;
	}
}

void
PluginManager::load_tags ()
{
	vector<std::string> tmp;
	find_files_matching_pattern (tmp, plugin_metadata_search_path (), "plugin_tags");

	for (vector<std::string>::const_reverse_iterator p = tmp.rbegin ();
			p != (vector<std::string>::const_reverse_iterator)tmp.rend(); ++p) {
		std::string path = *p;
		info << string_compose (_("Loading plugin meta data file %1"), path) << endmsg;
		if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
			return;
		}

		XMLTree tree;
		if (!tree.read (path)) {
			error << string_compose (_("Cannot parse plugin tag info from %1"), path) << endmsg;
			return;
		}

		for (XMLNodeConstIterator i = tree.root()->children().begin(); i != tree.root()->children().end(); ++i) {
			PluginType type;
			string id;
			string tags;
			string name;
			bool user_set;
			if (!(*i)->get_property (X_("type"), type) ||
			    !(*i)->get_property (X_("id"), id) ||
			    !(*i)->get_property (X_("tags"), tags) ||
			    !(*i)->get_property (X_("name"), name)) {
				continue;
			}
			if (!(*i)->get_property (X_("user-set"), user_set)) {
				user_set = false;
			}
			strip_whitespace_edges (tags);
			set_tags (type, id, tags, name, user_set ? FromUserFile : FromFactoryFile);
		}
	}
}

void
PluginManager::set_tags (PluginType t, string id, string tag, std::string name, TagType ttype)
{
	string sanitized = sanitize_tag (tag);

	PluginTag ps (to_generic_vst (t), id, sanitized, name, ttype);
	PluginTagList::const_iterator i = find (ptags.begin(), ptags.end(), ps);
	if (i == ptags.end()) {
		ptags.insert (ps);
	} else if ((uint32_t) ttype >= (uint32_t) (*i).tagtype) { // only overwrite if we are more important than the existing. Gui > UserFile > FactoryFile > Plugin
		ptags.erase (ps);
		ptags.insert (ps);
	}
	if (ttype == FromFactoryFile) {
		if (find (ftags.begin(), ftags.end(), ps) != ftags.end()) {
			ftags.erase (ps);
		}
		ftags.insert (ps);
	}
	if (ttype == FromGui) {
		PluginTagChanged (t, id, sanitized); /* EMIT SIGNAL */
	}
}

void
PluginManager::reset_tags (PluginInfoPtr const& pi)
{
	PluginTag ps (pi->type, pi->unique_id, pi->category, pi->name, FromPlug);

	PluginTagList::const_iterator j = find (ftags.begin(), ftags.end(), ps);
	if (j != ftags.end()) {
		ps.tags = j->tags;
		ps.tagtype = j->tagtype;
	}

	PluginTagList::const_iterator i = find (ptags.begin(), ptags.end(), ps);
	if (i != ptags.end()) {
		ptags.erase (ps);
		ptags.insert (ps);
		PluginTagChanged (ps.type, ps.unique_id, ps.tags); /* EMIT SIGNAL */
	}
}

std::string
PluginManager::sanitize_tag (const std::string to_sanitize) const
{
	if (to_sanitize.empty ()) {
		return "";
	}
	string sanitized = to_sanitize;
	vector<string> tags;
	if (!PBD::tokenize (sanitized, string(" ,\n"), std::back_inserter (tags), true)) {
#ifndef NDEBUG
		cerr << _("PluginManager::sanitize_tag could not tokenize string: ") << sanitized << endmsg;
#endif
		return "";
	}

	/* convert tokens to lower-case, space-separated list */
	sanitized = "";
	for (vector<string>::iterator t = tags.begin(); t != tags.end(); ++t) {
		if (t != tags.begin ()) {
			sanitized.append(" ");
		}
		sanitized.append (downcase (*t));
	}

	return sanitized;
}

std::vector<std::string>
PluginManager::get_all_tags (TagFilter tag_filter) const
{
	std::vector<std::string> ret;

	PluginTagList::const_iterator pt;
	for (pt = ptags.begin(); pt != ptags.end(); ++pt) {
		if ((*pt).tags.empty ()) {
			continue;
		}

		/* if favorites_only then we need to check the info ptr and maybe skip */
		if (tag_filter == OnlyFavorites) {
			PluginStatus stat ((*pt).type, (*pt).unique_id);
			PluginStatusList::const_iterator i = find (statuses.begin(), statuses.end(), stat);
			if ((i != statuses.end()) && (i->status == Favorite)) {
				/* it's a favorite! */
			} else {
				continue;
			}
		}
		if (tag_filter == NoHidden) {
			PluginStatus stat ((*pt).type, (*pt).unique_id);
			PluginStatusList::const_iterator i = find (statuses.begin(), statuses.end(), stat);
			if ((i != statuses.end()) && ((i->status == Hidden) || (i->status == Concealed))) {
				continue;
			}
		}

		/* parse each plugin's tag string into separate tags */
		vector<string> tags;
		if (!PBD::tokenize ((*pt).tags, string(" "), std::back_inserter (tags), true)) {
#ifndef NDEBUG
			cerr << _("PluginManager: Could not tokenize string: ") << (*pt).tags << endmsg;
#endif
			continue;
		}

		/* maybe add the tags we've found */
		for (vector<string>::iterator t = tags.begin(); t != tags.end(); ++t) {
			/* if this tag isn't already in the list, add it */
			vector<string>::iterator i = find (ret.begin(), ret.end(), *t);
			if (i == ret.end()) {
				ret.push_back (*t);
			}
		}
	}

	/* sort in alphabetical order */
	SortByTag sorter;
	sort (ret.begin(), ret.end(), sorter);

	return ret;
}

const ARDOUR::PluginInfoList&
PluginManager::windows_vst_plugin_info ()
{
#ifdef WINDOWS_VST_SUPPORT
	if (_windows_vst_plugin_info) {
		return *_windows_vst_plugin_info;
	}
#endif
	return _empty_plugin_info;
}

const ARDOUR::PluginInfoList&
PluginManager::mac_vst_plugin_info ()
{
#ifdef MACVST_SUPPORT
	if (_mac_vst_plugin_info) {
		return *_mac_vst_plugin_info;
	}
#endif
	return _empty_plugin_info;
}

const ARDOUR::PluginInfoList&
PluginManager::lxvst_plugin_info ()
{
#ifdef LXVST_SUPPORT
	if(_lxvst_plugin_info) {
		return *_lxvst_plugin_info;
	}
#endif
	return _empty_plugin_info;
}

const ARDOUR::PluginInfoList&
PluginManager::ladspa_plugin_info ()
{
	assert(_ladspa_plugin_info);
	return *_ladspa_plugin_info;
}

const ARDOUR::PluginInfoList&
PluginManager::lv2_plugin_info ()
{
	assert(_lv2_plugin_info);
	return *_lv2_plugin_info;
}

const ARDOUR::PluginInfoList&
PluginManager::au_plugin_info ()
{
#ifdef AUDIOUNIT_SUPPORT
	if (_au_plugin_info) {
		return *_au_plugin_info;
	}
#endif
	return _empty_plugin_info;
}

const ARDOUR::PluginInfoList&
PluginManager::vst3_plugin_info ()
{
#ifdef VST3_SUPPORT
	if (_vst3_plugin_info) {
		return *_vst3_plugin_info;
	}
#endif
	return _empty_plugin_info;
}

const ARDOUR::PluginInfoList&
PluginManager::lua_plugin_info ()
{
	assert(_lua_plugin_info);
	return *_lua_plugin_info;
}

/* ****************************************************************************/

void
PluginManager::blacklist (ARDOUR::PluginType type, std::string const& path_uid)
{
	PluginInfoList* pil = 0;

	switch (type) {
		case AudioUnit:
#ifdef AUDIOUNIT_SUPPORT
			auv2_blacklist (path_uid);
			pil = _au_plugin_info;
#endif
			break;
		case Windows_VST:
#ifdef WINDOWS_VST_SUPPORT
			vst2_blacklist (path_uid);
			pil = _windows_vst_plugin_info;
#endif
			break;
		case LXVST:
#ifdef LXVST_SUPPORT
			vst2_blacklist (path_uid);
			pil = _lxvst_plugin_info;
#endif
			break;
		case MacVST:
#ifdef MACVST_SUPPORT
			vst2_blacklist (path_uid);
			pil = _mac_vst_plugin_info;
#endif
			break;
		case VST3:
#ifdef VST3_SUPPORT
			vst3_blacklist (module_path_vst3 (path_uid));
			pil = _vst3_plugin_info;
#endif
			break;
		default:
			return;
	}

	PSLEPtr psle (scan_log_entry (type, path_uid));
	psle->msg (PluginScanLogEntry::Blacklisted);
	save_scanlog ();

	if (!pil) {
		return;
	}

	/* remove existing info */
	PluginScanLog::iterator i = _plugin_scan_log.find (PSLEPtr (new PluginScanLogEntry (type, path_uid)));
	if (i != _plugin_scan_log.end ()) {
		PluginInfoList const& plugs ((*i)->nfo ());
		for (PluginInfoList::const_iterator j = plugs.begin(); j != plugs.end(); ++j) {
			PluginInfoList::iterator k = std::find (pil->begin(), pil->end(), *j);
			if (k != pil->end()) {
				pil->erase (k);
			}
		}
	}
	PluginListChanged (); /* EMIT SIGNAL */
}

bool
PluginManager::whitelist (ARDOUR::PluginType type, std::string const& path_uid, bool force)
{
	if (!force) {
		PluginScanLog::iterator i = _plugin_scan_log.find (PSLEPtr (new PluginScanLogEntry (type, path_uid)));
		if (i == _plugin_scan_log.end ()) {
			/* plugin was not found */
			return false;
		}
		/* only allow manually blacklisted (no error) */
		if ((*i)->result () != PluginScanLogEntry::Blacklisted) {
			return false;
		}
	}

	switch (type) {
		case AudioUnit:
#ifdef AUDIOUNIT_SUPPORT
			auv2_whitelist (path_uid);
			return true;
#endif
			break;
		case Windows_VST:
		case LXVST:
		case MacVST:
#if (defined WINDOWS_VST_SUPPORT || defined MACVST_SUPPORT || defined LXVST_SUPPORT)
			vst2_whitelist (path_uid);
			return true;
#endif
			break;
		case VST3:
#ifdef VST3_SUPPORT
			vst3_whitelist (module_path_vst3 (path_uid));
			return true;
#endif
			break;
		default:
			break;
	}
	return false;
}

std::string
PluginManager::cache_file (ARDOUR::PluginType type, std::string const& path_uid)
{
	std::string fn;

	switch (type) {
		case AudioUnit:
#ifdef AUDIOUNIT_SUPPORT
			{
				AUv2DescStr aud (path_uid);
				fn = auv2_cache_file (aud.desc ());
			}
#endif
			break;
		case Windows_VST:
		case LXVST:
		case MacVST:
#if (defined WINDOWS_VST_SUPPORT || defined MACVST_SUPPORT || defined LXVST_SUPPORT)
			fn = ARDOUR::vst2_cache_file (path_uid);
#endif
			break;
		case VST3:
#ifdef VST3_SUPPORT
			fn = ARDOUR::vst3_cache_file (module_path_vst3 (path_uid));
#endif
			break;
		default:
			break;
	}
	if (!fn.empty () && !Glib::file_test (fn, Glib::FileTest (Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR))) {
		return "";
	}
	return fn;
}

bool
PluginManager::rescan_plugin (ARDOUR::PluginType type, std::string const& path_uid, size_t num, size_t den)
{
	PluginInfoList* pil = 0;

	switch (type) {
		case AudioUnit:
			pil = _au_plugin_info;
			break;
		case Windows_VST:
			pil = _windows_vst_plugin_info;
			break;
		case LXVST:
			pil = _lxvst_plugin_info;
			break;
		case MacVST:
			pil = _mac_vst_plugin_info;
			break;
		case VST3:
			pil = _vst3_plugin_info;
			// XXX resolve VST module to bundle
			// string module_path = module_path_vst3 (uid);
			break;
		case LADSPA:
			pil = _ladspa_plugin_info;
			break;
		default:
			DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Invalid plugin type for rescan: %1\n", type));
			return false;
	}

	if (!pil) {
		DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Rescan of '%1' without initial scan\n", type));
		return false;
	}

	bool erased = false;
	/* remove existing info */
	PluginScanLog::iterator i = _plugin_scan_log.find (PSLEPtr (new PluginScanLogEntry (type, path_uid)));
	if (i != _plugin_scan_log.end ()) {
		PluginInfoList const& plugs ((*i)->nfo ());
		for (PluginInfoList::const_iterator j = plugs.begin(); j != plugs.end(); ++j) {
			PluginInfoList::iterator k = std::find (pil->begin(), pil->end(), *j);
			if (k != pil->end()) {
				pil->erase (k);
			}
			erased = true;
		}
		_plugin_scan_log.erase (i);
	}

	reset_scan_cancel_state (den < 2 ? false : true);

	whitelist (type, path_uid, true);

	/* force re-scan, remove cache file */
	std::string fn = cache_file (type, path_uid);
	if (!fn.empty ()) {
		::g_unlink (fn.c_str ());
	}

	int rv = -1;
	switch (type) {
		case AudioUnit:
#ifdef AUDIOUNIT_SUPPORT
			{
				AUv2DescStr aud (path_uid);
				if (den > 1) {
					ARDOUR::PluginScanMessage (string_compose (_("AUv2 (%1 / %2)"), num, den), aud.to_s(), !cancelled());
				} else {
					ARDOUR::PluginScanMessage (_("AUv2"), aud.to_s(), !cancelled());
				}
				rv = auv2_discover (aud, false);
			}
#endif
			break;
		case Windows_VST:
		case LXVST:
		case MacVST:
#if (defined WINDOWS_VST_SUPPORT || defined MACVST_SUPPORT || defined LXVST_SUPPORT)
			if (den > 1) {
				ARDOUR::PluginScanMessage (string_compose (_("VST2 (%1 / %2)"), num, den), path_uid, !cancelled());
			} else {
				ARDOUR::PluginScanMessage (_("VST2"), path_uid, !cancelled());
			}
			rv = vst2_discover (path_uid, type, false);
#endif
			break;
		case VST3:
#ifdef VST3_SUPPORT
			if (den > 1) {
				ARDOUR::PluginScanMessage (string_compose (_("VST3 (%1 / %2)"), num, den), path_uid, !cancelled());
			} else {
				ARDOUR::PluginScanMessage (_("VST3"), path_uid, !cancelled());
			}
			rv = vst3_discover (path_uid, false);
#endif
			break;
		case LADSPA:
			if (den > 1) {
				ARDOUR::PluginScanMessage (string_compose (_("LADSPA (%1 / %2)"), num, den), path_uid, !cancelled());
			} else {
				ARDOUR::PluginScanMessage (_("LADSPA"), path_uid, !cancelled());
			}
			rv = ladspa_discover (path_uid);
			break;
		default:
			return false;
	}

	if (den > 1) {
		return (rv >= 0 || erased);
	}

	reset_scan_cancel_state ();

	if (rv < 0) {
		save_scanlog ();
		if (erased) {
			PluginListChanged (); /* EMIT SIGNAL */
		} else {
			PluginScanLogChanged (); /* EMIT SIGNAL */
		}
		PluginScanMessage(X_("closeme"), "", false);
		return false;
	}

	PluginScanMessage(X_("closeme"), "", false);
	detect_ambiguities ();
	return true;
}

void
PluginManager::rescan_faulty ()
{
	bool changed = false;
	/* rescan can change _plugin_scan_log, so we need to make a copy first */
	PluginScanLog psl;
	for (PluginScanLog::const_iterator i = _plugin_scan_log.begin(); i != _plugin_scan_log.end(); ++i) {
		if (!(*i)->recent() || ((*i)->result() & PluginScanLogEntry::Faulty) != PluginScanLogEntry::OK) {
			psl.insert (*i);
		}
	}

	reset_scan_cancel_state ();

	size_t n = 1;
	size_t all_modules = psl.size ();
	for (PluginScanLog::const_iterator i = psl.begin(); i != psl.end(); ++i, ++n) {
		changed |= rescan_plugin ((*i)->type (), (*i)->path (), n, all_modules);
		if (_cancel_scan_all) {
			break;
		}
	}

	reset_scan_cancel_state ();
	PluginScanMessage(X_("closeme"), "", false);

	if (changed) {
		detect_ambiguities ();
	} else {
		save_scanlog ();
		PluginScanLogChanged ();/* EMIT SIGNAL */
	}
}

/* ****************************************************************************/
void
PluginManager::scan_log (std::vector<boost::shared_ptr<PluginScanLogEntry> >& l) const
{
	for (PluginScanLog::const_iterator i = _plugin_scan_log.begin(); i != _plugin_scan_log.end(); ++i) {
		l.push_back (*i);
	}
}

void
PluginManager::clear_stale_log ()
{
	bool erased = false;
	for (PluginScanLog::const_iterator i = _plugin_scan_log.begin(); i != _plugin_scan_log.end();) {
		if (!(*i)->recent()) {
			whitelist ((*i)->type (), (*i)->path (), true);
			std::string fn = cache_file ((*i)->type (), (*i)->path ());
			if (!fn.empty ()) {
				DEBUG_TRACE (DEBUG::PluginManager, string_compose ("unlink stale cache: %1\n", fn));
				::g_unlink (fn.c_str ());
			}
			i = _plugin_scan_log.erase (i);
			erased = true;
		} else {
			++i;
		}
	}
	if (erased) {
		save_scanlog ();
		PluginScanLogChanged ();/* EMIT SIGNAL */
	}
}

void
PluginManager::load_scanlog ()
{
	_plugin_scan_log.clear ();
	std::string path = Glib::build_filename (user_plugin_metadata_dir(), "scan_log");
	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		return;
	}

	XMLTree tree;
	if (!tree.read (path)) {
		error << string_compose (_("Cannot load Plugin Scan Log from '%1'."), path) << endmsg;
		return;
	}

	for (XMLNodeConstIterator i = tree.root()->children().begin(); i != tree.root()->children().end(); ++i) {
		try {
			_plugin_scan_log.insert (PSLEPtr (new PluginScanLogEntry (**i)));
		} catch (...) {
			error << string_compose (_("Plugin Scan Log '%1' contains invalid information."), path) << endmsg;
		}
	}
}

void
PluginManager::save_scanlog ()
{
	std::string path = Glib::build_filename (user_plugin_metadata_dir(), "scan_log");
	XMLNode* root = new XMLNode (X_("PluginScanLog"));
	root->set_property ("version", 1);

	for (PluginScanLog::const_iterator i = _plugin_scan_log.begin(); i != _plugin_scan_log.end(); ++i) {
		XMLNode& node = (*i)->state ();
		root->add_child_nocopy (node);
	}

	XMLTree tree;
	tree.set_root (root);
	if (!tree.write (path)) {
		error << string_compose (_("Could not save Plugin Scan Log to %1"), path) << endmsg;
	}
}
