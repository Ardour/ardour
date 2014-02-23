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

#include "ardour/linux_vst_support.h"
#include "ardour/vst_info_file.h"

#define MAX_STRING_LEN 256

using namespace std;

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
		fprintf( fp, "%d\n", infos->size() - 1 );
		++x;
		/* Now write out the info for each plugin */
		for (; x != infos->end(); ++x) {
			vstfx_write_info_block(fp, *x);
		}
	} else if (infos->size() == 1) {
		vstfx_write_info_block(fp, infos->front());
	} else {
		PBD::warning << "Zero plugins in VST." << endmsg; // XXX here?
	}
}

static string
vstfx_infofile_path (char* dllpath, int personal)
{
	string dir;
	if (personal) {
		dir = Glib::build_filename (Glib::get_home_dir (), ".fst");

		/* If the directory doesn't exist, try to create it */
		if (!Glib::file_test (dir, Glib::FILE_TEST_IS_DIR)) {
			if (g_mkdir (dir.c_str (), 0700)) {
				return 0;
			}
		}

	} else {
		dir = Glib::path_get_dirname (std::string(dllpath));
	}

	stringstream s;
	s << "." << Glib::path_get_basename (dllpath) << ".fsi";
	return Glib::build_filename (dir, s.str ());
}

static char *
vstfx_infofile_stat (char *dllpath, struct stat* statbuf, int personal)
{
	if (strstr (dllpath, ".so" ) == 0 && strstr(dllpath, ".dll") == 0) {
		return 0;
	}

	string const path = vstfx_infofile_path (dllpath, personal);

	if (Glib::file_test (path, Glib::FileTest (Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR))) {

		/* info file exists in same location as the shared object, so
		   check if its current and up to date
		*/


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
vstfx_infofile_for_read (char* dllpath)
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
vstfx_infofile_create (char* dllpath, int personal)
{
	if (strstr (dllpath, ".so" ) == 0 && strstr(dllpath, ".dll") == 0) {
		return 0;
	}

	string const path = vstfx_infofile_path (dllpath, personal);
	return fopen (path.c_str(), "w");
}

static FILE *
vstfx_infofile_for_write (char* dllpath)
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

static VSTInfo *
vstfx_info_from_plugin (VSTState* vstfx)
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

	info->UniqueID = plugin->uniqueID;

	info->Category = strdup("None"); /* XXX */
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

/* A simple 'dummy' audiomaster callback which should be ok,
   we will only be instantiating the plugin in order to get its info
*/

static intptr_t
simple_master_callback (AEffect *, int32_t opcode, int32_t, intptr_t, void *, float)
{
#if 0
	static const char* can_do_strings[] = {
		X_("supportShell"),
		X_("shellCategory")
	};
	static const int can_do_string_count = sizeof (can_do_strings) / sizeof (char*);
	static int current_loading_id = 0;
#endif

	if (opcode == audioMasterVersion) {
		return 2;
	}
#if 0
	else if (opcode == audioMasterCanDo) {
		for (int i = 0; i < can_do_string_count; i++) {
			if (! strcmp(can_do_strings[i], (const char*)ptr)) {
				return 1;
			}
		}
		return 0;
	}
	else if (opcode == audioMasterCurrentId) {
		return current_loading_id;
	}
#endif
	else {
		return 0;
	}
}

static bool
vstfx_get_info_from_file(char* dllpath, vector<VSTInfo*> *infos)
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

void
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

void
vstfx_free_info_list (vector<VSTInfo *> *infos)
{
	for (vector<VSTInfo *>::iterator i = infos->begin(); i != infos->end(); ++i) {
		vstfx_free_info(*i);
	}
	delete infos;
}

#ifdef LXVST_SUPPORT
/** Try to get plugin info - first by looking for a .fsi cache of the
    data, and if that doesn't exist, load the plugin, get its data and
    then cache it for future ref
*/

vector<VSTInfo *> *
vstfx_get_info_lx (char* dllpath)
{
	FILE* infofile;
	VSTHandle* h;
	VSTState* vstfx;
	vector<VSTInfo*> *infos = new vector<VSTInfo*>;

	// TODO check blacklist
	// TODO pre-check file extension ?

	if (vstfx_get_info_from_file(dllpath, infos)) {
		PBD::info << "using cache for LinuxVST plugin '" << dllpath << "'" << endmsg;
		return infos;
	}

	if (!(h = vstfx_load(dllpath))) {
		PBD::warning << "Cannot get LinuxVST information from " << dllpath << ": load failed." << endmsg;
		return infos;
	}

	if (!(vstfx = vstfx_instantiate(h, simple_master_callback, 0))) {
		vstfx_unload(h);
		PBD::warning << "Cannot get LinuxVST information from " << dllpath << ": instantiation failed." << endmsg;
		return infos;
	}

	infofile = vstfx_infofile_for_write (dllpath);

	if (!infofile) {
		vstfx_close(vstfx);
		vstfx_unload(h);
		PBD::warning << "Cannot get LinuxVST information from " << dllpath << ": cannot create new FST info file." << endmsg;
		return 0;
	}

	VSTInfo* info = vstfx_info_from_plugin (vstfx);
	infos->push_back(info); //XXX

	vstfx_write_info_file (infofile, infos);
	fclose (infofile);

	vstfx_close (vstfx);
	vstfx_unload (h);
	return infos;
}
#endif

#ifdef WINDOWS_VST_SUPPORT
#include <fst.h>

vector<VSTInfo *> *
vstfx_get_info_fst (char* dllpath)
{
	FILE* infofile;
	VSTHandle* h;
	VSTState* vstfx;
	vector<VSTInfo*> *infos = new vector<VSTInfo*>;

	// TODO check blacklist
	// TODO pre-check file extension ?

	if (vstfx_get_info_from_file(dllpath, infos)) {
		PBD::info << "using cache for VST plugin '" << dllpath << "'" << endmsg;
		return infos;
	}

	if(!(h = fst_load(dllpath))) {
		PBD::warning << "Cannot get VST information from " << dllpath << ": load failed." << endmsg;
		return infos;
	}

	if(!(vstfx = fst_instantiate(h, simple_master_callback, 0))) {
		fst_unload(&h);
		PBD::warning << "Cannot get VST information from " << dllpath << ": instantiation failed." << endmsg;
		return infos;
	}

	infofile = vstfx_infofile_for_write (dllpath);

	if (!infofile) {
		fst_close(vstfx);
		//fst_unload(&h); // XXX -> fst_close()
		PBD::warning << "Cannot get VST information from " << dllpath << ": cannot create new FST info file." << endmsg;
		return 0;
	}

	VSTInfo* info = vstfx_info_from_plugin(vstfx);
	infos->push_back(info); //XXX

	vstfx_write_info_file (infofile, infos);
	fclose (infofile);

	fst_close(vstfx);
	//fst_unload(&h); // XXX -> fst_close()
	return infos;
}
#endif
