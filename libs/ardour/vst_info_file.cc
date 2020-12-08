/*
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2017 John Emmas <john@creativepost.co.uk>
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

/** @file libs/ardour/vst_info_file.cc
 *  @brief Code to manage info files containing cached information about a plugin.
 *  e.g. its name, creator etc.
 */

#include <cassert>

#include <sys/types.h>

#ifdef COMPILER_MSVC
#include <sys/utime.h>
#else
#include <unistd.h>
#include <utime.h>
#endif

#include <fcntl.h>
#include <errno.h>

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include "pbd/gstdio_compat.h"
#include <glibmm.h>

#include "pbd/error.h"
#include "pbd/compose.h"

#ifndef VST_SCANNER_APP
#include "ardour/plugin_manager.h" // scanner_bin_path
#include "ardour/rc_configuration.h"
#include "ardour/system_exec.h"
#endif

#include "ardour/filesystem_paths.h"
#include "ardour/linux_vst_support.h"
#include "ardour/mac_vst_support.h"
#include "ardour/plugin_types.h"
#include "ardour/vst_info_file.h"

#include "pbd/i18n.h"
#include "sha1.c"

#define MAX_STRING_LEN 256
#define PLUGIN_SCAN_TIMEOUT (Config->get_vst_scan_timeout()) // in deciseconds

using namespace std;
#ifndef VST_SCANNER_APP
namespace ARDOUR {
#endif

/* prototypes */
#ifdef WINDOWS_VST_SUPPORT
#include <fst.h>
static bool
vstfx_instantiate_and_get_info_fst (const char* dllpath, vector<VSTInfo*> *infos, int uniqueID);
#endif

#ifdef LXVST_SUPPORT
static bool vstfx_instantiate_and_get_info_lx (const char* dllpath, vector<VSTInfo*> *infos, int uniqueID);
#endif

#ifdef MACVST_SUPPORT
static bool vstfx_instantiate_and_get_info_mac (const char* dllpath, vector<VSTInfo*> *infos, int uniqueID);
#endif

/* ID for shell plugins */
static int vstfx_current_loading_id = 0;

/* *** CACHE FILE PATHS *** */

static string
get_vst_info_cache_dir () {
	string dir = Glib::build_filename (ARDOUR::user_cache_directory (), "vst");
	/* if the directory doesn't exist, try to create it */
	if (!Glib::file_test (dir, Glib::FILE_TEST_IS_DIR)) {
		if (g_mkdir (dir.c_str (), 0700)) {
			PBD::fatal << "Cannot create VST info folder '" << dir << "'" << endmsg;
		}
	}
	return dir;
}

static string
vstfx_infofile_path (const char* dllpath)
{
	char hash[41];
	Sha1Digest s;
	sha1_init (&s);
	sha1_write (&s, (const uint8_t *) dllpath, strlen (dllpath));
	sha1_result_hash (&s, hash);
	return Glib::build_filename (get_vst_info_cache_dir (), std::string (hash) + std::string (VST_EXT_INFOFILE));
}


/* *** VST Blacklist *** */

static void vstfx_read_blacklist (std::string &bl) {
	FILE * blacklist_fd = NULL;
	bl = "";

	string fn = Glib::build_filename (ARDOUR::user_cache_directory (), VST_BLACKLIST);

	if (!Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		return;
	}

	if (! (blacklist_fd = g_fopen (fn.c_str (), "rb"))) {
		return;
	}

	while (!feof (blacklist_fd)) {
		char buf[1024];
		size_t s = fread (buf, sizeof(char), 1024, blacklist_fd);
		if (ferror (blacklist_fd)) {
			PBD::error << string_compose (_("error reading VST Blacklist file %1 (%2)"), fn, strerror (errno)) << endmsg;
			bl = "";
			break;
		}
		if (s == 0) {
			break;
		}
		bl.append (buf, s);
	}
	::fclose (blacklist_fd);
}

/** mark plugin as blacklisted */
static void vstfx_blacklist (const char *id)
{
	string fn = Glib::build_filename (ARDOUR::user_cache_directory (), VST_BLACKLIST);
	FILE * blacklist_fd = NULL;
	if (! (blacklist_fd = g_fopen (fn.c_str (), "a"))) {
		PBD::error << string_compose (_("Cannot append to VST blacklist for '%1'"), id) << endmsg;
		return;
	}
	assert (NULL == strchr (id, '\n'));
	fprintf (blacklist_fd, "%s\n", id);
	::fclose (blacklist_fd);
}

/** mark plugin as not blacklisted */
static void vstfx_un_blacklist (const char *idcs)
{
	string id (idcs);
	string fn = Glib::build_filename (ARDOUR::user_cache_directory (), VST_BLACKLIST);
	if (!Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		PBD::warning << _("Expected VST Blacklist file does not exist.") << endmsg;
		return;
	}

	std::string bl;
	vstfx_read_blacklist (bl);

	::g_unlink (fn.c_str ());

	assert (!Glib::file_test (fn, Glib::FILE_TEST_EXISTS));
	assert (id.find ("\n") == string::npos);

	id += "\n"; // add separator
	const size_t rpl = bl.find (id);
	if (rpl != string::npos) {
		bl.replace (rpl, id.size (), "");
	}
	if (bl.empty ()) {
		return;
	}

	FILE * blacklist_fd = NULL;
	if (! (blacklist_fd = g_fopen (fn.c_str (), "w"))) {
		PBD::error << _("Cannot open VST blacklist.") << endmsg;;
		return;
	}
	fprintf (blacklist_fd, "%s", bl.c_str ());
	::fclose (blacklist_fd);
}

/* return true if plugin is blacklisted */
static bool vst_is_blacklisted (const char *idcs)
{
	// TODO ideally we'd also check if the VST has been updated since blacklisting
	string id (idcs);
	string fn = Glib::build_filename (ARDOUR::user_cache_directory (), VST_BLACKLIST);
	if (!Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		return false;
	}

	std::string bl;
	vstfx_read_blacklist (bl);

	assert (id.find ("\n") == string::npos);

	id += "\n"; // add separator
	const size_t rpl = bl.find (id);
	if (rpl != string::npos) {
		return true;
	}
	return false;
}



/* *** MEMORY MANAGEMENT *** */

/** cleanup single allocated VSTInfo */
static void
vstfx_free_info (VSTInfo *info)
{
	for (int i = 0; i < info->numParams; i++) {
		free (info->ParamNames[i]);
		free (info->ParamLabels[i]);
	}

	free (info->name);
	free (info->creator);
	free (info->Category);
	free (info->ParamNames);
	free (info->ParamLabels);
	free (info);
}

/** reset vector */
static void
vstfx_clear_info_list (vector<VSTInfo *> *infos)
{
	for (vector<VSTInfo *>::iterator i = infos->begin (); i != infos->end (); ++i) {
		vstfx_free_info (*i);
	}
	infos->clear ();
}


/* *** CACHE FILE I/O *** */

/** Helper function to read a line from the cache file
 * @return newly allocated string of NULL
 */
static char *
read_string (FILE *fp)
{
	char buf[MAX_STRING_LEN];

	if (!fgets (buf, MAX_STRING_LEN, fp)) {
		return 0;
	}

	if (strlen (buf)) {
		/* strip lash char here: '\n',
		 * since VST-params cannot be longer than 127 chars.
		 */
		buf[strlen (buf)-1] = 0;
		return strdup (buf);
	}
	return 0;
}

/** Read an integer value from a line in fp into n,
 *  @return true on failure, false on success.
 */
static bool
read_int (FILE* fp, int* n)
{
	char buf[MAX_STRING_LEN];

	char* p = fgets (buf, MAX_STRING_LEN, fp);
	if (p == 0) {
		return true;
	}

	return (sscanf (p, "%d", n) != 1);
}

/** parse a plugin-block from the cache info file */
static bool
vstfx_load_info_block (FILE* fp, VSTInfo *info)
{
	if ((info->name = read_string (fp)) == 0) return false;
	if ((info->creator = read_string (fp)) == 0) return false;
	if (read_int (fp, &info->UniqueID)) return false;
	if ((info->Category = read_string (fp)) == 0) return false;
	if (read_int (fp, &info->numInputs)) return false;
	if (read_int (fp, &info->numOutputs)) return false;
	if (read_int (fp, &info->numParams)) return false;
	if (read_int (fp, &info->wantMidi)) return false;
	if (read_int (fp, &info->hasEditor)) return false;
	if (read_int (fp, &info->canProcessReplacing)) return false;

	/* backwards compatibility with old .fsi files */
	if (info->wantMidi == -1) {
		info->wantMidi = 1;
	}

	info->isInstrument = (info->wantMidi & 4) ? 1 : 0;

	info->isInstrument |= info->numInputs == 0 && info->numOutputs > 0 && 1 == (info->wantMidi & 1);
	if (!strcmp (info->Category, "Instrument")) {
		info->isInstrument = 1;
	}

	if ((info->numParams) == 0) {
		info->ParamNames = NULL;
		info->ParamLabels = NULL;
		return true;
	}

	if ((info->ParamNames = (char **) malloc (sizeof (char*) * info->numParams)) == 0) {
		return false;
	}

	for (int i = 0; i < info->numParams; ++i) {
		if ((info->ParamNames[i] = read_string (fp)) == 0) return false;
	}

	if ((info->ParamLabels = (char **) malloc (sizeof (char*) * info->numParams)) == 0) {
		return false;
	}

	for (int i = 0; i < info->numParams; ++i) {
		if ((info->ParamLabels[i] = read_string (fp)) == 0) {
			return false;
		}
	}
	return true;
}

/** parse all blocks in a cache info file */
static bool
vstfx_load_info_file (FILE* fp, vector<VSTInfo*> *infos)
{
	VSTInfo *info;
	if ((info = (VSTInfo*) calloc (1, sizeof (VSTInfo))) == 0) {
		return false;
	}
	if (vstfx_load_info_block (fp, info)) {
		if (strncmp (info->Category, "Shell", 5)) {
			infos->push_back (info);
		} else {
			int plugin_cnt = 0;
			vstfx_free_info (info);
			if (!read_int (fp, &plugin_cnt)) {
				for (int i = 0; i < plugin_cnt; i++) {
					if ((info = (VSTInfo*) calloc (1, sizeof (VSTInfo))) == 0) {
						vstfx_clear_info_list (infos);
						return false;
					}
					if (vstfx_load_info_block (fp, info)) {
						infos->push_back (info);
					} else {
						vstfx_free_info (info);
						vstfx_clear_info_list (infos);
						return false;
					}
				}
			} else {
				return false; /* Bad file */
			}
		}
		return true;
	}
	vstfx_free_info (info);
	vstfx_clear_info_list (infos);
	return false;
}

static void
vstfx_write_info_block (FILE* fp, VSTInfo *info)
{
	assert (info);
	assert (fp);

	fprintf (fp, "%s\n", info->name);
	fprintf (fp, "%s\n", info->creator);
	fprintf (fp, "%d\n", info->UniqueID);
	fprintf (fp, "%s\n", info->Category);
	fprintf (fp, "%d\n", info->numInputs);
	fprintf (fp, "%d\n", info->numOutputs);
	fprintf (fp, "%d\n", info->numParams);
	fprintf (fp, "%d\n", info->wantMidi | (info->isInstrument ? 4 : 0));
	fprintf (fp, "%d\n", info->hasEditor);
	fprintf (fp, "%d\n", info->canProcessReplacing);

	for (int i = 0; i < info->numParams; i++) {
		fprintf (fp, "%s\n", info->ParamNames[i]);
	}

	for (int i = 0; i < info->numParams; i++) {
		fprintf (fp, "%s\n", info->ParamLabels[i]);
	}
}

static void
vstfx_write_info_file (FILE* fp, vector<VSTInfo *> *infos)
{
	assert (infos);
	assert (fp);

	if (infos->size () > 1) {
		vector<VSTInfo *>::iterator x = infos->begin ();
		/* write out the shell info first along with count of the number of
		 * plugins contained in this shell
		 */
		vstfx_write_info_block (fp, *x);
		fprintf (fp, "%d\n", (int)infos->size () - 1 );
		++x;
		/* Now write out the info for each plugin */
		for (; x != infos->end (); ++x) {
			vstfx_write_info_block (fp, *x);
		}
	} else if (infos->size () == 1) {
		vstfx_write_info_block (fp, infos->front ());
	} else {
		PBD::warning << _("VST object file contains no plugins.") << endmsg;
	}
}


/* *** CACHE MANAGEMENT *** */

/** remove info file from cache */
static void
vstfx_remove_infofile (const char *dllpath)
{
	::g_unlink (vstfx_infofile_path (dllpath).c_str ());
}

/** cache file for given plugin
 * @return FILE of the .fsi cache if found and up-to-date*/
static FILE *
vstfx_infofile_for_read (const char* dllpath)
{
	const size_t slen = strlen (dllpath);
	if (
			(slen <= 3 || g_ascii_strcasecmp (&dllpath[slen-3], ".so"))
			&&
			(slen <= 4 || g_ascii_strcasecmp (&dllpath[slen-4], ".vst"))
			&&
			(slen <= 4 || g_ascii_strcasecmp (&dllpath[slen-4], ".dll"))
	   ) {
		return 0;
	}

	string const path = vstfx_infofile_path (dllpath);

	if (Glib::file_test (path, Glib::FileTest (Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR))) {
		GStatBuf dllstat;
		GStatBuf fsistat;

		if (g_stat (dllpath, &dllstat) == 0) {
			if (g_stat (path.c_str (), &fsistat) == 0) {
				if (dllstat.st_mtime <= fsistat.st_mtime) {
					/* plugin is older than info file */
					return g_fopen (path.c_str (), "rb");
				}
			}
		}
		PBD::warning << string_compose (_("Ignored VST plugin which is newer than cache: '%1' (cache: '%2')"), dllpath, path) << endmsg;
		PBD::info << _("Re-Scan Plugins (Preferences > Plugins) to update the cache, also make sure your system-time is set correctly.") << endmsg;
	}
	return NULL;
}

/** newly created cache file for given plugin
 * @return FILE for the .fsi cache or NULL on error
 */
static FILE *
vstfx_infofile_for_write (const char* dllpath)
{
	const size_t slen = strlen (dllpath);
	if (
			(slen <= 3 || g_ascii_strcasecmp (&dllpath[slen-3], ".so"))
			&&
			(slen <= 4 || g_ascii_strcasecmp (&dllpath[slen-4], ".vst"))
			&&
			(slen <= 4 || g_ascii_strcasecmp (&dllpath[slen-4], ".dll"))
	   ) {
		return NULL;
	}

	string const path = vstfx_infofile_path (dllpath);
	return g_fopen (path.c_str (), "wb");
}

/** check if cache-file exists, is up-to-date and parse cache file
 * @param infos [return] loaded plugin info
 * @return true if .fsi cache was read successfully, false otherwise
 */
static bool
vstfx_get_info_from_file (const char* dllpath, vector<VSTInfo*> *infos)
{
	FILE* infofile;
	bool rv = false;
	if ((infofile = vstfx_infofile_for_read (dllpath)) != 0) {
		rv = vstfx_load_info_file (infofile, infos);
		fclose (infofile);
		if (!rv) {
			PBD::warning << string_compose (_("Cannot get VST information for '%1': failed to load cache file."), dllpath) << endmsg;
		}
	}
	return rv;
}



/* *** VST system-under-test methods *** */

static
bool vstfx_midi_input (VSTState* vstfx)
{
	AEffect* plugin = vstfx->plugin;

	/* should we send it VST events (i.e. MIDI) */

	if ((plugin->flags & effFlagsIsSynth)
			|| (plugin->dispatcher (plugin, effCanDo, 0, 0, const_cast<char*> ("receiveVstEvents"), 0.0f) > 0)
			|| (plugin->dispatcher (plugin, effCanDo, 0, 0, const_cast<char*> ("receiveVstMidiEvent"), 0.0f) > 0)
			|| (plugin->dispatcher (plugin, effCanDo, 0, 0, const_cast<char*> ("receiveVstMidiEvents"), 0.0f) > 0)
		 ) {
		return true;
	}

	return false;
}

static
bool vstfx_midi_output (VSTState* vstfx)
{
	AEffect* plugin = vstfx->plugin;

	int const vst_version = plugin->dispatcher (plugin, effGetVstVersion, 0, 0, 0, 0.0f);

	if (vst_version >= 2) {
		/* should we send it VST events (i.e. MIDI) */

		if (   (plugin->dispatcher (plugin, effCanDo, 0, 0, const_cast<char*> ("sendVstEvents"), 0.0f) > 0)
		    || (plugin->dispatcher (plugin, effCanDo, 0, 0, const_cast<char*> ("sendVstMidiEvent"), 0.0f) > 0)
		    || (plugin->dispatcher (plugin, effCanDo, 0, 0, const_cast<char*> ("sendVstMidiEvents"), 0.0f) > 0)
		   ) {
			return true;
		}
	}

	return false;
}

/** simple 'dummy' audiomaster callback to instantiate the plugin
 * and query information
 */
static intptr_t
simple_master_callback (AEffect *, int32_t opcode, int32_t, intptr_t, void *ptr, float)
{
	const char* vstfx_can_do_strings[] = {
		"supplyIdle",
		"sendVstTimeInfo",
		"sendVstEvents",
		"sendVstMidiEvent",
		"receiveVstEvents",
		"receiveVstMidiEvent",
		"supportShell",
		"shellCategory",
		"shellCategorycurID",
		"sizeWindow"
	};
	const int vstfx_can_do_string_count = 9;

	if (opcode == audioMasterVersion) {
		return 2400;
	}
	else if (opcode == audioMasterCanDo) {
		for (int i = 0; i < vstfx_can_do_string_count; i++) {
			if (! strcmp (vstfx_can_do_strings[i], (const char*)ptr)) {
				return 1;
			}
		}
		return 0;
	}
	else if (opcode == audioMasterCurrentId) {
		return vstfx_current_loading_id;
	}
	else {
		return 0;
	}
}


/** main plugin query and test function */
static VSTInfo*
vstfx_parse_vst_state (VSTState* vstfx)
{
	assert (vstfx);

	VSTInfo* info = (VSTInfo*) malloc (sizeof (VSTInfo));
	if (!info) {
		return 0;
	}

	/* We need to init the creator because some plugins
	 * fail to implement getVendorString, and so won't stuff the
	 * string with any name */

	char creator[65] = "Unknown";
	char name[65] = "";

	AEffect* plugin = vstfx->plugin;


	plugin->dispatcher (plugin, effGetEffectName, 0, 0, name, 0);

	if (strlen (name) == 0) {
		plugin->dispatcher (plugin, effGetProductString, 0, 0, name, 0);
	}

	if (strlen (name) == 0) {
		info->name = strdup (vstfx->handle->name);
	} else {
		info->name = strdup (name);
	}

	/*If the plugin doesn't bother to implement GetVendorString we will
	 * have pre-stuffed the string with 'Unknown' */

	plugin->dispatcher (plugin, effGetVendorString, 0, 0, creator, 0);

	/* Some plugins DO implement GetVendorString, but DON'T put a name in it
	 * so if its just a zero length string we replace it with 'Unknown' */

	if (strlen (creator) == 0) {
		info->creator = strdup ("Unknown");
	} else {
		info->creator = strdup (creator);
	}


	switch (plugin->dispatcher (plugin, effGetPlugCategory, 0, 0, 0, 0))
	{
		case kPlugCategEffect:         info->Category = strdup ("Effect"); break;
		case kPlugCategSynth:          info->Category = strdup ("Instrument"); break;
		case kPlugCategAnalysis:       info->Category = strdup ("Analyser"); break;
		case kPlugCategMastering:      info->Category = strdup ("Mastering"); break;
		case kPlugCategSpacializer:    info->Category = strdup ("Spatial"); break;
		case kPlugCategRoomFx:         info->Category = strdup ("RoomFx"); break;
		case kPlugSurroundFx:          info->Category = strdup ("SurroundFx"); break;
		case kPlugCategRestoration:    info->Category = strdup ("Restoration"); break;
		case kPlugCategOfflineProcess: info->Category = strdup ("Offline"); break;
		case kPlugCategShell:          info->Category = strdup ("Shell"); break;
		case kPlugCategGenerator:      info->Category = strdup ("Generator"); break;
		default:                       info->Category = strdup ("Unknown"); break;
	}

	info->UniqueID = plugin->uniqueID;

	info->numInputs = plugin->numInputs;
	info->numOutputs = plugin->numOutputs;
	info->numParams = plugin->numParams;
	info->wantMidi = (vstfx_midi_input (vstfx) ? 1 : 0) | (vstfx_midi_output (vstfx) ? 2 : 0);
	info->hasEditor = plugin->flags & effFlagsHasEditor ? true : false;
	info->isInstrument = (plugin->flags & effFlagsIsSynth) ? 1 : 0;
	info->canProcessReplacing = plugin->flags & effFlagsCanReplacing ? true : false;
	info->ParamNames = (char **) malloc (sizeof (char*)*info->numParams);
	info->ParamLabels = (char **) malloc (sizeof (char*)*info->numParams);

#ifdef __APPLE__
	if (info->hasEditor) {
		/* we only support Cocoa UIs (just like Reaper) */
		info->hasEditor = (plugin->dispatcher (plugin, effCanDo, 0, 0, const_cast<char*> ("hasCockosViewAsConfig"), 0.0f) & 0xffff0000) == 0xbeef0000;
	}
#endif

	for (int i = 0; i < info->numParams; ++i) {
		char name[VestigeMaxLabelLen];
		char label[VestigeMaxLabelLen];

		/* Not all plugins give parameters labels as well as names */

		strcpy (name, "No Name");
		strcpy (label, "No Label");

		plugin->dispatcher (plugin, effGetParamName, i, 0, name, 0);
		info->ParamNames[i] = strdup (name);

		//NOTE: 'effGetParamLabel' is no longer defined in vestige headers
		//plugin->dispatcher (plugin, effGetParamLabel, i, 0, label, 0);
		info->ParamLabels[i] = strdup (label);
	}
	return info;
}

/** wrapper around \ref vstfx_parse_vst_state,
 * iterate over plugins in shell, translate VST-info into ardour VSTState
 */
static void
vstfx_info_from_plugin (const char *dllpath, VSTState* vstfx, vector<VSTInfo *> *infos, enum ARDOUR::PluginType type)
{
	assert (vstfx);
	VSTInfo *info;

	if (!(info = vstfx_parse_vst_state (vstfx))) {
		return;
	}

	infos->push_back (info);
#if 1 // shell-plugin support
	/* If this plugin is a Shell and we are not already inside a shell plugin
	 * read the info for all of the plugins contained in this shell.
	 */
	if (!strncmp (info->Category, "Shell", 5)
			&& vstfx->handle->plugincnt == 1) {
		int id;
		vector< pair<int, string> > ids;
		AEffect *plugin = vstfx->plugin;

		do {
			char name[65] = "Unknown";
			id = plugin->dispatcher (plugin, effShellGetNextPlugin, 0, 0, name, 0);
			ids.push_back (std::make_pair (id, name));
		} while ( id != 0 );

		switch (type) {
#ifdef WINDOWS_VST_SUPPORT
			case ARDOUR::Windows_VST:
				fst_close (vstfx);
				break;
#endif
#ifdef LXVST_SUPPORT
			case ARDOUR::LXVST:
				vstfx_close (vstfx);
				break;
#endif
#ifdef MACVST_SUPPORT
			case ARDOUR::MacVST:
				mac_vst_close (vstfx);
				break;
#endif
			default:
				assert (0);
				break;
		}

		for (vector< pair<int, string> >::iterator x = ids.begin (); x != ids.end (); ++x) {
			id = (*x).first;
			if (id == 0) continue;
			/* recurse vstfx_get_info() */

			bool ok;
			switch (type) {
#ifdef WINDOWS_VST_SUPPORT
				case ARDOUR::Windows_VST:
					ok = vstfx_instantiate_and_get_info_fst (dllpath, infos, id);
					break;
#endif
#ifdef LXVST_SUPPORT
				case ARDOUR::LXVST:
					ok = vstfx_instantiate_and_get_info_lx (dllpath, infos, id);
					break;
#endif
#ifdef MACVST_SUPPORT
				case ARDOUR::MacVST:
					ok = vstfx_instantiate_and_get_info_mac (dllpath, infos, id);
					break;
#endif
				default:
					ok = false;
					break;
			}
			if (ok) {
				// One shell (some?, all?) does not report the actual plugin name
				// even after the shelled plugin has been instantiated.
				// Replace the name of the shell with the real name.
				info = infos->back ();
				free (info->name);

				if ((*x).second.length () == 0) {
					info->name = strdup ("Unknown");
				}
				else {
					info->name = strdup ((*x).second.c_str ());
				}
			}
		}
	} else {
		switch (type) {
#ifdef WINDOWS_VST_SUPPORT
			case ARDOUR::Windows_VST:
				fst_close (vstfx);
				break;
#endif
#ifdef LXVST_SUPPORT
			case ARDOUR::LXVST:
				vstfx_close (vstfx);
				break;
#endif
#ifdef MACVST_SUPPORT
			case ARDOUR::MacVST:
				mac_vst_close (vstfx);
				break;
#endif
			default:
				assert (0);
				break;
		}
	}
#endif
}



/* *** TOP-LEVEL PLUGIN INSTANTIATION FUNCTIONS *** */

#ifdef LXVST_SUPPORT
static bool
vstfx_instantiate_and_get_info_lx (
		const char* dllpath, vector<VSTInfo*> *infos, int uniqueID)
{
	VSTHandle* h;
	VSTState* vstfx;
	if (!(h = vstfx_load (dllpath))) {
		PBD::warning << string_compose (_("Cannot get LinuxVST information from '%1': load failed."), dllpath) << endmsg;
		return false;
	}

	vstfx_current_loading_id = uniqueID;

	if (!(vstfx = vstfx_instantiate (h, simple_master_callback, 0))) {
		vstfx_unload (h);
		PBD::warning << string_compose (_("Cannot get LinuxVST information from '%1': instantiation failed."), dllpath) << endmsg;
		return false;
	}

	vstfx_current_loading_id = 0;

	vstfx_info_from_plugin (dllpath, vstfx, infos, ARDOUR::LXVST);

	vstfx_unload (h);
	return true;
}
#endif

#ifdef WINDOWS_VST_SUPPORT
static bool
vstfx_instantiate_and_get_info_fst (
		const char* dllpath, vector<VSTInfo*> *infos, int uniqueID)
{
	VSTHandle* h;
	VSTState* vstfx;
	if (!(h = fst_load (dllpath))) {
		PBD::warning << string_compose (_("Cannot get Windows VST information from '%1': load failed."), dllpath) << endmsg;
		return false;
	}

	vstfx_current_loading_id = uniqueID;

	if (!(vstfx = fst_instantiate (h, simple_master_callback, 0))) {
		fst_unload (&h);
		vstfx_current_loading_id = 0;
		PBD::warning << string_compose (_("Cannot get Windows VST information from '%1': instantiation failed."), dllpath) << endmsg;
		return false;
	}
	vstfx_current_loading_id = 0;

	vstfx_info_from_plugin (dllpath, vstfx, infos, ARDOUR::Windows_VST);

	return true;
}
#endif

#ifdef MACVST_SUPPORT
static bool
vstfx_instantiate_and_get_info_mac (
		const char* dllpath, vector<VSTInfo*> *infos, int uniqueID)
{
	printf("vstfx_instantiate_and_get_info_mac %s\n", dllpath);
	VSTHandle* h;
	VSTState* vstfx;
	if (!(h = mac_vst_load (dllpath))) {
		PBD::warning << string_compose (_("Cannot get MacVST information from '%1': load failed."), dllpath) << endmsg;
		return false;
	}

	vstfx_current_loading_id = uniqueID;

	if (!(vstfx = mac_vst_instantiate (h, simple_master_callback, 0))) {
		mac_vst_unload (h);
		PBD::warning << string_compose (_("Cannot get MacVST information from '%1': instantiation failed."), dllpath) << endmsg;
		return false;
	}

	vstfx_current_loading_id = 0;

	vstfx_info_from_plugin (dllpath, vstfx, infos, ARDOUR::MacVST);

	mac_vst_unload (h);
	return true;
}
#endif

/* *** ERROR LOGGING *** */
#ifndef VST_SCANNER_APP

static FILE * _errorlog_fd = 0;
static char * _errorlog_dll = 0;

static void parse_scanner_output (std::string msg, size_t /*len*/)
{
	if (!_errorlog_fd && !_errorlog_dll) {
		PBD::error << "VST scanner: " << msg;
		return;
	}

#if 0 // TODO
	if (!_errorlog_fd) {
		if (!(_errorlog_fd = g_fopen (vstfx_errorfile_path (_errorlog_dll).c_str (), "w"))) {
			PBD::error << "Cannot create plugin error-log for plugin " << _errorlog_dll;
			free (_errorlog_dll);
			_errorlog_dll = NULL;
		}
	}
#endif

	if (_errorlog_fd) {
		fprintf (_errorlog_fd, "%s\n", msg.c_str ());
	} else if (_errorlog_dll) {
		PBD::error << "VST '" << _errorlog_dll << "': " << msg;
	} else {
		PBD::error << "VST scanner: " << msg;
	}
}

static void
set_error_log (const char* dllpath) {
	assert (!_errorlog_fd);
	assert (!_errorlog_dll);
	_errorlog_dll = strdup (dllpath);
}

static void
close_error_log () {
	if (_errorlog_fd) {
		fclose (_errorlog_fd);
		_errorlog_fd = 0;
	}
	free (_errorlog_dll);
	_errorlog_dll = 0;
}

#endif


/* *** the main function that uses all of the above *** */

static vector<VSTInfo *> *
vstfx_get_info (const char* dllpath, enum ARDOUR::PluginType type, enum VSTScanMode mode)
{
	FILE* infofile;
	vector<VSTInfo*> *infos = new vector<VSTInfo*>;

	if (vst_is_blacklisted (dllpath)) {
		return infos;
	}

	if (vstfx_get_info_from_file (dllpath, infos)) {
		return infos;
	}

#ifndef VST_SCANNER_APP
	std::string scanner_bin_path = ARDOUR::PluginManager::scanner_bin_path;

	if (mode == VST_SCAN_CACHE_ONLY) {
		/* never scan explicitly, use cache only */
		return infos;
	}
	else if (mode == VST_SCAN_USE_APP && scanner_bin_path != "") {
		/* use external scanner app */

		char **argp= (char**) calloc (3,sizeof (char*));
		argp[0] = strdup (scanner_bin_path.c_str ());
		argp[1] = strdup (dllpath);
		argp[2] = 0;

		set_error_log (dllpath);
		ARDOUR::SystemExec scanner (scanner_bin_path, argp);
		PBD::ScopedConnectionList cons;
		scanner.ReadStdout.connect_same_thread (cons, boost::bind (&parse_scanner_output, _1 ,_2));
		if (scanner.start (ARDOUR::SystemExec::MergeWithStdin)) {
			PBD::error << string_compose (_("Cannot launch VST scanner app '%1': %2"), scanner_bin_path, strerror (errno)) << endmsg;
			close_error_log ();
			return infos;
		} else {
			int timeout = PLUGIN_SCAN_TIMEOUT;
			bool no_timeout = (timeout <= 0);
			while (scanner.is_running () && (no_timeout || timeout > 0)) {
				if (!no_timeout && ARDOUR::PluginManager::instance().no_timeout()) {
					no_timeout = true;
					timeout = -1;
				}

				ARDOUR::PluginScanTimeout (timeout);
				--timeout;
				Glib::usleep (100000);

				if (ARDOUR::PluginManager::instance ().cancelled () /*|| (!no_timeout && timeout == 0*)*/) {
					// remove info file (might be incomplete)
					vstfx_remove_infofile (dllpath);
					// remove temporary blacklist file (scan incomplete)
					vstfx_un_blacklist (dllpath);
					scanner.terminate ();
					close_error_log ();
					return infos;
				}
			}
			scanner.terminate ();
		}
		close_error_log ();
		/* re-read index (generated by external scanner) */
		vstfx_clear_info_list (infos);
		if (!vst_is_blacklisted (dllpath)) {
			vstfx_get_info_from_file (dllpath, infos);
		}
		return infos;
	}
	/* else .. instantiate and check in in ardour process itself */
#else
	(void) mode; // unused parameter
#endif

	bool ok;
	/* blacklist in case instantiation fails */
	vstfx_blacklist (dllpath);

	switch (type) {
#ifdef WINDOWS_VST_SUPPORT
		case ARDOUR::Windows_VST:
			ok = vstfx_instantiate_and_get_info_fst (dllpath, infos, 0);
			break;
#endif
#ifdef LXVST_SUPPORT
		case ARDOUR::LXVST:
			ok = vstfx_instantiate_and_get_info_lx (dllpath, infos, 0);
			break;
#endif
#ifdef MACVST_SUPPORT
		case ARDOUR::MacVST:
			ok = vstfx_instantiate_and_get_info_mac (dllpath, infos, 0);
			break;
#endif
		default:
			ok = false;
			break;
	}

	if (!ok) {
		return infos;
	}

	/* remove from blacklist */
	vstfx_un_blacklist (dllpath);

	/* crate cache/whitelist */
	infofile = vstfx_infofile_for_write (dllpath);
	if (!infofile) {
		PBD::warning << string_compose (_("Cannot cache VST information for '%1': cannot create cache file."), dllpath) << endmsg;
		return infos;
	} else {
		vstfx_write_info_file (infofile, infos);
		fclose (infofile);

		/* In some cases the .dll may have a modification time in the future,
		 * (e.g. unzip a VST plugin: .zip files don't include timezones)
		 */
		string const fsipath = vstfx_infofile_path (dllpath);
		GStatBuf dllstat;
		GStatBuf fsistat;
		if (g_stat (dllpath, &dllstat) == 0 && g_stat (fsipath.c_str (), &fsistat) == 0) {
			struct utimbuf utb;
			utb.actime = fsistat.st_atime;
			utb.modtime = std::max (dllstat.st_mtime, fsistat.st_mtime);
			g_utime (fsipath.c_str (), &utb);
		}
	}
	return infos;
}


/* *** public API *** */

void
vstfx_free_info_list (vector<VSTInfo *> *infos)
{
	for (vector<VSTInfo *>::iterator i = infos->begin (); i != infos->end (); ++i) {
		vstfx_free_info (*i);
	}
	delete infos;
}

#ifdef LXVST_SUPPORT
vector<VSTInfo *> *
vstfx_get_info_lx (char* dllpath, enum VSTScanMode mode)
{
	return vstfx_get_info (dllpath, ARDOUR::LXVST, mode);
}
#endif

#ifdef MACVST_SUPPORT
vector<VSTInfo *> *
vstfx_get_info_mac (char* dllpath, enum VSTScanMode mode)
{
	return vstfx_get_info (dllpath, ARDOUR::MacVST, mode);
}
#endif

#ifdef WINDOWS_VST_SUPPORT
vector<VSTInfo *> *
vstfx_get_info_fst (char* dllpath, enum VSTScanMode mode)
{
	return vstfx_get_info (dllpath, ARDOUR::Windows_VST, mode);
}
#endif

#ifndef VST_SCANNER_APP
} // namespace
#endif
