/*
    Copyright (C) 2012-2014 Paul Davis

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

/** @file libs/ardour/vst_info_file.cc
 *  @brief Code to manage info files containing cached information about a plugin.
 *  e.g. its name, creator etc.
 */

#include <iostream>
#include <cassert>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glibmm.h>

#include "pbd/error.h"

#ifndef VST_SCANNER_APP
#include "pbd/system_exec.h"
#include "pbd/file_utils.h"
#endif

#include "ardour/filesystem_paths.h"
#include "ardour/linux_vst_support.h"
#include "ardour/vst_info_file.h"

#define MAX_STRING_LEN 256

using namespace std;

// TODO: namespace public API into ARDOUR, ::Session or ::PluginManager
//       consolidate vstfx_get_info_lx() and vstfx_get_info_fst()

/* prototypes */
#ifdef WINDOWS_VST_SUPPORT
#include <fst.h>
static bool
vstfx_instantiate_and_get_info_fst (const char* dllpath, vector<VSTInfo*> *infos, int uniqueID);
#endif

#ifdef LXVST_SUPPORT
static bool vstfx_instantiate_and_get_info_lx (const char* dllpath, vector<VSTInfo*> *infos, int uniqueID);
#endif

static int vstfx_current_loading_id = 0;

static char *
read_string (FILE *fp)
{
	char buf[MAX_STRING_LEN];

	if (!fgets (buf, MAX_STRING_LEN, fp)) {
		return 0;
	}

	if (strlen(buf) < MAX_STRING_LEN) {
		if (strlen (buf)) {
			buf[strlen(buf)-1] = 0;
		}
		return strdup (buf);
	} else {
		return 0;
	}
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

static void
vstfx_clear_info_list (vector<VSTInfo *> *infos)
{
	for (vector<VSTInfo *>::iterator i = infos->begin(); i != infos->end(); ++i) {
		vstfx_free_info(*i);
	}
	infos->clear();
}

static bool
vstfx_load_info_block(FILE* fp, VSTInfo *info)
{
	if ((info->name = read_string(fp)) == 0) return false;
	if ((info->creator = read_string(fp)) == 0) return false;
	if (read_int (fp, &info->UniqueID)) return false;
	if ((info->Category = read_string(fp)) == 0) return false;
	if (read_int (fp, &info->numInputs)) return false;
	if (read_int (fp, &info->numOutputs)) return false;
	if (read_int (fp, &info->numParams)) return false;
	if (read_int (fp, &info->wantMidi)) return false;
	if (read_int (fp, &info->hasEditor)) return false;
	if (read_int (fp, &info->canProcessReplacing)) return false;

	if ((info->ParamNames = (char **) malloc(sizeof(char*)*info->numParams)) == 0) {
		return false;
	}

	for (int i = 0; i < info->numParams; ++i) {
		if ((info->ParamNames[i] = read_string(fp)) == 0) return false;
	}

	if ((info->ParamLabels = (char **) malloc(sizeof(char*)*info->numParams)) == 0) {
		return false;
	}

	for (int i = 0; i < info->numParams; ++i) {
		if ((info->ParamLabels[i] = read_string(fp)) == 0) {
			return false;
		}
	}
	return true;
}

static bool
vstfx_load_info_file (FILE* fp, vector<VSTInfo*> *infos)
{
	VSTInfo *info;
	if ((info = (VSTInfo*) calloc (1, sizeof (VSTInfo))) == 0) {
		return false;
	}
	if (vstfx_load_info_block(fp, info)) {
		if (strncmp (info->Category, "Shell", 5)) {
			infos->push_back(info);
		} else {
			int plugin_cnt = 0;
			vstfx_free_info(info);
			if (read_int (fp, &plugin_cnt)) {
				for (int i = 0; i < plugin_cnt; i++) {
					if ((info = (VSTInfo*) calloc (1, sizeof (VSTInfo))) == 0) {
						vstfx_clear_info_list(infos);
						return false;
					}
					if (vstfx_load_info_block(fp, info)) {
						infos->push_back(info);
					} else {
						vstfx_free_info(info);
						vstfx_clear_info_list(infos);
						return false;
					}
				}
			} else {
				return false; /* Bad file */
			}
		}
		return true;
	}
	vstfx_free_info(info);
	vstfx_clear_info_list(infos);
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
	fprintf (fp, "%d\n", info->wantMidi);
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
	assert(infos);
	assert(fp);

	if (infos->size() > 1) {
		vector<VSTInfo *>::iterator x = infos->begin();
		/* write out the shell info first along with count of the number of
		 * plugins contained in this shell
		 */
		vstfx_write_info_block(fp, *x);
		fprintf( fp, "%d\n", (int)infos->size() - 1 );
		++x;
		/* Now write out the info for each plugin */
		for (; x != infos->end(); ++x) {
			vstfx_write_info_block(fp, *x);
		}
	} else if (infos->size() == 1) {
		vstfx_write_info_block(fp, infos->front());
	} else {
		PBD::error << "Zero plugins in VST." << endmsg; // XXX here? rather make this impossible before if it ain't already.
	}
}

static string
vstfx_blacklist_path (const char* dllpath, int personal)
{
	string dir;
	if (personal) {
		dir = get_personal_vst_blacklist_dir();
	} else {
		dir = Glib::path_get_dirname (std::string(dllpath));
	}

	stringstream s;
	s << "." << Glib::path_get_basename (dllpath) << ".fsb";
	return Glib::build_filename (dir, s.str ());
}

/* return true if plugin is blacklisted or has an invalid file extension */
static bool
vstfx_blacklist_stat (const char *dllpath, int personal)
{
	if (strstr (dllpath, ".so" ) == 0 && strstr(dllpath, ".dll") == 0) {
		return true;
	}
	string const path = vstfx_blacklist_path (dllpath, personal);

	if (Glib::file_test (path, Glib::FileTest (Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR))) {
		struct stat dllstat;
		struct stat fsbstat;

		if (stat (dllpath, &dllstat) == 0 && stat (path.c_str(), &fsbstat) == 0) {
			if (dllstat.st_mtime > fsbstat.st_mtime) {
				/* plugin is newer than blacklist file */
				return true;
			}
		}
		/* stat failed or plugin is older than blacklist file */
		return true;
	}
	/* blacklist file does not exist */
	return false;
}

static bool
vstfx_check_blacklist (const char *dllpath)
{
	if (vstfx_blacklist_stat(dllpath, 0)) return true;
	if (vstfx_blacklist_stat(dllpath, 1)) return true;
	return false;
}

static FILE *
vstfx_blacklist_file (const char *dllpath)
{
	FILE *f;
	if ((f = fopen (vstfx_blacklist_path (dllpath, 0).c_str(), "w"))) {
		return f;
	}
	return fopen (vstfx_blacklist_path (dllpath, 1).c_str(), "w");
}

static bool
vstfx_blacklist (const char *dllpath)
{
	FILE *f = vstfx_blacklist_file(dllpath);
	if (f) {
		fclose(f);
		return true;
	}
	return false;
}

static void
vstfx_un_blacklist (const char *dllpath)
{
	::g_unlink(vstfx_blacklist_path (dllpath, 0).c_str());
	::g_unlink(vstfx_blacklist_path (dllpath, 1).c_str());
}

static string
vstfx_infofile_path (const char* dllpath, int personal)
{
	string dir;
	if (personal) {
		dir = get_personal_vst_info_cache_dir();
	} else {
		dir = Glib::path_get_dirname (std::string(dllpath));
	}

	stringstream s;
	s << "." << Glib::path_get_basename (dllpath) << ".fsi";
	return Glib::build_filename (dir, s.str ());
}

static char *
vstfx_infofile_stat (const char *dllpath, struct stat* statbuf, int personal)
{
	if (strstr (dllpath, ".so" ) == 0 && strstr(dllpath, ".dll") == 0) {
		return 0;
	}

	string const path = vstfx_infofile_path (dllpath, personal);

	if (Glib::file_test (path, Glib::FileTest (Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR))) {

		struct stat dllstat;

		if (stat (dllpath, &dllstat) == 0) {
			if (stat (path.c_str(), statbuf) == 0) {
				if (dllstat.st_mtime <= statbuf->st_mtime) {
					/* plugin is older than info file */
					return strdup (path.c_str ());
				}
			}
		}
	}

	return 0;
}


static FILE *
vstfx_infofile_for_read (const char* dllpath)
{
	struct stat own_statbuf;
	struct stat sys_statbuf;
	FILE *rv = NULL;

	char* own_info = vstfx_infofile_stat (dllpath, &own_statbuf, 1);
	char* sys_info = vstfx_infofile_stat (dllpath, &sys_statbuf, 0);

	if (own_info) {
		if (sys_info) {
			if (own_statbuf.st_mtime <= sys_statbuf.st_mtime) {
				/* system info file is newer, use it */
				rv = g_fopen (sys_info, "rb");
			}
		} else {
			rv = g_fopen (own_info, "rb");
		}
	} else if (sys_info) {
		rv = g_fopen (sys_info, "rb");
	}
	free(own_info);
	free(sys_info);

	return rv;
}

static FILE *
vstfx_infofile_create (const char* dllpath, int personal)
{
	if (strstr (dllpath, ".so" ) == 0 && strstr(dllpath, ".dll") == 0) {
		return 0;
	}

	string const path = vstfx_infofile_path (dllpath, personal);
	return fopen (path.c_str(), "w");
}

static FILE *
vstfx_infofile_for_write (const char* dllpath)
{
	FILE* f;

	if ((f = vstfx_infofile_create (dllpath, 0)) == 0) {
		f = vstfx_infofile_create (dllpath, 1);
	}

	return f;
}

static
int vstfx_can_midi (VSTState* vstfx)
{
	AEffect* plugin = vstfx->plugin;

	int const vst_version = plugin->dispatcher (plugin, effGetVstVersion, 0, 0, 0, 0.0f);

	if (vst_version >= 2) {
		/* should we send it VST events (i.e. MIDI) */

		if ((plugin->flags & effFlagsIsSynth) || (plugin->dispatcher (plugin, effCanDo, 0, 0,(void*) "receiveVstEvents", 0.0f) > 0)) {
			return -1;
		}
	}

	return false;
}

static VSTInfo*
vstfx_parse_vst_state (VSTState* vstfx)
{
	assert (vstfx);

	VSTInfo* info = (VSTInfo*) malloc (sizeof (VSTInfo));
	if (!info) {
		return 0;
	}

	/*We need to init the creator because some plugins
	  fail to implement getVendorString, and so won't stuff the
	  string with any name*/

	char creator[65] = "Unknown\0";

	AEffect* plugin = vstfx->plugin;

	info->name = strdup (vstfx->handle->name);

	/*If the plugin doesn't bother to implement GetVendorString we will
	  have pre-stuffed the string with 'Unkown' */

	plugin->dispatcher (plugin, effGetVendorString, 0, 0, creator, 0);

	/*Some plugins DO implement GetVendorString, but DON'T put a name in it
	  so if its just a zero length string we replace it with 'Unknown' */

	if (strlen(creator) == 0) {
		info->creator = strdup ("Unknown");
	} else {
		info->creator = strdup (creator);
	}


	switch (plugin->dispatcher (plugin, effGetPlugCategory, 0, 0, 0, 0))
	{
		case kPlugCategEffect:         info->Category = strdup ("Effect"); break;
		case kPlugCategSynth:          info->Category = strdup ("Synth"); break;
		case kPlugCategAnalysis:       info->Category = strdup ("Anaylsis"); break;
		case kPlugCategMastering:      info->Category = strdup ("Mastering"); break;
		case kPlugCategSpacializer:    info->Category = strdup ("Spacializer"); break;
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
	info->wantMidi = vstfx_can_midi(vstfx);
	info->hasEditor = plugin->flags & effFlagsHasEditor ? true : false;
	info->canProcessReplacing = plugin->flags & effFlagsCanReplacing ? true : false;
	info->ParamNames = (char **) malloc(sizeof(char*)*info->numParams);
	info->ParamLabels = (char **) malloc(sizeof(char*)*info->numParams);

	for (int i = 0; i < info->numParams; ++i) {
		char name[64];
		char label[64];

		/* Not all plugins give parameters labels as well as names */

		strcpy (name, "No Name");
		strcpy (label, "No Label");

		plugin->dispatcher (plugin, effGetParamName, i, 0, name, 0);
		info->ParamNames[i] = strdup(name);

		//NOTE: 'effGetParamLabel' is no longer defined in vestige headers
		//plugin->dispatcher (plugin, effGetParamLabel, i, 0, label, 0);
		info->ParamLabels[i] = strdup(label);
	}
	return info;
}

static void
vstfx_info_from_plugin (const char *dllpath, VSTState* vstfx, vector<VSTInfo *> *infos, int type)
{
	assert(vstfx);
	VSTInfo *info;

	if ((info = vstfx_parse_vst_state(vstfx))) {
		infos->push_back(info);
#if 1
		/* If this plugin is a Shell and we are not already inside a shell plugin
		 * read the info for all of the plugins contained in this shell.
		 */
		if (!strncmp (info->Category, "Shell", 5)
		    && vstfx->handle->plugincnt == 1) {
			int id;
			vector< pair<int, string> > ids;
			AEffect *plugin = vstfx->plugin;
			string path = vstfx->handle->path;

			do {
				char name[65] = "Unknown\0";
				id = plugin->dispatcher (plugin, effShellGetNextPlugin, 0, 0, name, 0);
				ids.push_back(std::make_pair(id, name));
			} while ( id != 0 );

			switch(type) {
#ifdef WINDOWS_VST_SUPPORT
				case 1: fst_close(vstfx); break;
#endif
#ifdef LXVST_SUPPORT
				case 2: vstfx_close (vstfx); break;
#endif
				default: assert(0); break;
			}

			for (vector< pair<int, string> >::iterator x = ids.begin(); x != ids.end(); ++x) {
				id = (*x).first;
				if (id == 0) continue;
				/* recurse vstfx_get_info() */

				bool ok;
				switch (type) { // TODO use lib ardour's type
#ifdef WINDOWS_VST_SUPPORT
					case 1:  ok = vstfx_instantiate_and_get_info_fst(dllpath, infos, id); break;
#endif
#ifdef LXVST_SUPPORT
					case 2:  ok = vstfx_instantiate_and_get_info_lx(dllpath, infos, id); break;
#endif
					default: ok = false;
				}
				if (ok) {
					// One shell (some?, all?) does not report the actual plugin name
					// even after the shelled plugin has been instantiated.
					// Replace the name of the shell with the real name.
					info = infos->back();
					free (info->name);

					if ((*x).second.length() == 0) {
						info->name = strdup("Unknown");
					}
					else {
						info->name = strdup ((*x).second.c_str());
					}
				}
			}
		}
#endif
	}
}

/* A simple 'dummy' audiomaster callback which should be ok,
   we will only be instantiating the plugin in order to get its info
*/

static intptr_t
simple_master_callback (AEffect *, int32_t opcode, int32_t, intptr_t, void *ptr, float)
{
	const char* vstfx_can_do_strings[] = {
		"supportShell",
		"shellCategory"
	};
	const int vstfx_can_do_string_count = 2;

	if (opcode == audioMasterVersion) {
		return 2400;
	}
	else if (opcode == audioMasterCanDo) {
		for (int i = 0; i < vstfx_can_do_string_count; i++) {
			if (! strcmp(vstfx_can_do_strings[i], (const char*)ptr)) {
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

static bool
vstfx_get_info_from_file(const char* dllpath, vector<VSTInfo*> *infos)
{
	FILE* infofile;
	bool rv = false;
	if ((infofile = vstfx_infofile_for_read (dllpath)) != 0) {
		rv = vstfx_load_info_file(infofile, infos);
		fclose (infofile);
		if (!rv) {
			PBD::warning << "Cannot get LinuxVST information form " << dllpath << ": info file load failed." << endmsg;
		}
	}
	return rv;
}

#ifdef LXVST_SUPPORT
static bool
vstfx_instantiate_and_get_info_lx (
		const char* dllpath, vector<VSTInfo*> *infos, int uniqueID)
{
	VSTHandle* h;
	VSTState* vstfx;
	if (!(h = vstfx_load(dllpath))) {
		PBD::warning << "Cannot get LinuxVST information from " << dllpath << ": load failed." << endmsg;
		return false;
	}

	vstfx_current_loading_id = uniqueID;

	if (!(vstfx = vstfx_instantiate(h, simple_master_callback, 0))) {
		vstfx_unload(h);
		PBD::warning << "Cannot get LinuxVST information from " << dllpath << ": instantiation failed." << endmsg;
		return false;
	}

	vstfx_current_loading_id = 0;

	vstfx_info_from_plugin(dllpath, vstfx, infos, 2);

	vstfx_close (vstfx);
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
	if(!(h = fst_load(dllpath))) {
		PBD::warning << "Cannot get Windows VST information from " << dllpath << ": load failed." << endmsg;
		return false;
	}

	vstfx_current_loading_id = uniqueID;

	if(!(vstfx = fst_instantiate(h, simple_master_callback, 0))) {
		fst_unload(&h);
		vstfx_current_loading_id = 0;
		PBD::warning << "Cannot get Windows VST information from " << dllpath << ": instantiation failed." << endmsg;
		return false;
	}
	vstfx_current_loading_id = 0;

	vstfx_info_from_plugin(dllpath, vstfx, infos, 1);

	fst_close(vstfx);
	//fst_unload(&h); // XXX -> fst_close()
	return true;
}
#endif

#ifndef VST_SCANNER_APP
static void parse_scanner_output (std::string msg, size_t /*len*/)
{
	// TODO write to blacklist or error file..?
	PBD::error << "VST scanner: " << msg;
}
#endif

static vector<VSTInfo *> *
vstfx_get_info (const char* dllpath, int type, enum VSTScanMode mode)
{
	FILE* infofile;
	vector<VSTInfo*> *infos = new vector<VSTInfo*>;

	if (vstfx_check_blacklist(dllpath)) {
		return infos;
	}

	if (vstfx_get_info_from_file(dllpath, infos)) {
		return infos;
	}

#ifndef VST_SCANNER_APP
	if (mode == VST_SCAN_CACHE_ONLY) {
		/* never scan explicitly, use cache only */
		return infos;
	}
	else if (mode == VST_SCAN_USE_APP) {
		/* use external scanner app -- TODO resolve path only once use static
		 * ARDOUR::PluginManager::scanner_bin_path
		 */
		std::string scanner_bin_path; //= "/home/rgareus/src/git/ardourCairoCanvas/build/libs/fst/ardour-vst-scanner"; // XXX
		if (!PBD::find_file_in_search_path (
					PBD::Searchpath(Glib::build_filename(ARDOUR::ardour_dll_directory(), "fst")),
					"ardour-vst-scanner", scanner_bin_path)) {
			PBD::error << "VST scanner app not found.'" << endmsg;
			// TODO: fall-through !?
			return infos;
		}
		/* note: these are free()d in the dtor of PBD::SystemExec */
		char **argp= (char**) calloc(3,sizeof(char*));
		argp[0] = strdup(scanner_bin_path.c_str());
		argp[1] = strdup(dllpath);
		argp[2] = 0;

		PBD::SystemExec scanner (scanner_bin_path, argp);
		PBD::ScopedConnectionList cons;
		// TODO timeout.., and honor user-terminate
		//scanner->Terminated.connect_same_thread (cons, boost::bind (&scanner_terminated))
		scanner.ReadStdout.connect_same_thread (cons, boost::bind (&parse_scanner_output, _1 ,_2));
		if (scanner.start (2 /* send stderr&stdout via signal */)) {
			PBD::error << "Cannot launch VST scanner app '" << scanner_bin_path << "': "<< strerror(errno) << endmsg;
			return infos;
		} else {
			// TODO idle loop (emit signal to GUI to call gtk_main_iteration()) check cancel.
			scanner.wait();
		}
		/* re-read index */
		vstfx_clear_info_list(infos);
		if (!vstfx_check_blacklist(dllpath)) {
			vstfx_get_info_from_file(dllpath, infos);
		}
		return infos;
	}
	/* else .. instantiate and check in in ardour process itself */
#else
	(void) mode; // unused parameter
#endif

	bool ok;
	/* blacklist in case instantiation fails */
	vstfx_blacklist(dllpath);

	switch (type) { // TODO use lib ardour's type
#ifdef WINDOWS_VST_SUPPORT
		case 1:  ok = vstfx_instantiate_and_get_info_fst(dllpath, infos, 0); break;
#endif
#ifdef LXVST_SUPPORT
		case 2:  ok = vstfx_instantiate_and_get_info_lx(dllpath, infos, 0); break;
#endif
		default: ok = false;
	}

	if (!ok) {
		return infos;
	}

	/* remove from blacklist */
	vstfx_un_blacklist(dllpath);

	/* crate cache/whitelist */
	infofile = vstfx_infofile_for_write (dllpath);
	if (!infofile) {
		PBD::warning << "Cannot cache VST information for " << dllpath << ": cannot create new FST info file." << endmsg;
		return infos;
	} else {
		vstfx_write_info_file (infofile, infos);
		fclose (infofile);
	}
	return infos;
}

/* *** public API *** */

void
vstfx_free_info_list (vector<VSTInfo *> *infos)
{
	for (vector<VSTInfo *>::iterator i = infos->begin(); i != infos->end(); ++i) {
		vstfx_free_info(*i);
	}
	delete infos;
}

string
get_personal_vst_blacklist_dir() {
	string dir = Glib::build_filename (ARDOUR::user_cache_directory(), "fst_blacklist");
	/* if the directory doesn't exist, try to create it */
	if (!Glib::file_test (dir, Glib::FILE_TEST_IS_DIR)) {
		if (g_mkdir (dir.c_str (), 0700)) {
			PBD::error << "Cannt create VST cache folder '" << dir << "'" << endmsg;
			//exit(1);
		}
	}
	return dir;
}

string
get_personal_vst_info_cache_dir() {
	string dir = Glib::build_filename (ARDOUR::user_cache_directory(), "fst_info");
	/* if the directory doesn't exist, try to create it */
	if (!Glib::file_test (dir, Glib::FILE_TEST_IS_DIR)) {
		if (g_mkdir (dir.c_str (), 0700)) {
			PBD::error << "Cannt create VST info folder '" << dir << "'" << endmsg;
			//exit(1);
		}
	}
	return dir;
}

#ifdef LXVST_SUPPORT
vector<VSTInfo *> *
vstfx_get_info_lx (char* dllpath, enum VSTScanMode mode)
{
	return vstfx_get_info(dllpath, 2, mode);
}
#endif

#ifdef WINDOWS_VST_SUPPORT
vector<VSTInfo *> *
vstfx_get_info_fst (char* dllpath, enum VSTScanMode mode)
{
	return vstfx_get_info(dllpath, 1, mode);
}
#endif
