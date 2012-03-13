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
 *  @return true on success, false on failure.
 */
static bool
read_int (FILE* fp, int* n)
{
	char buf[MAX_STRING_LEN];

	char* p = fgets (buf, MAX_STRING_LEN, fp);
	if (p == 0) {
		return false;
	}

	return (sscanf (p, "%d", n) != 1);
}

static VSTInfo *
load_vstfx_info_file (FILE* fp)
{
	VSTInfo *info;
	
	if ((info = (VSTInfo*) malloc (sizeof (VSTInfo))) == 0) {
		return 0;
	}

	if ((info->name = read_string(fp)) == 0) goto error;
	if ((info->creator = read_string(fp)) == 0) goto error;
	if (read_int (fp, &info->UniqueID)) goto error;
	if ((info->Category = read_string(fp)) == 0) goto error;
	if (read_int (fp, &info->numInputs)) goto error;
	if (read_int (fp, &info->numOutputs)) goto error;
	if (read_int (fp, &info->numParams)) goto error;
	if (read_int (fp, &info->wantMidi)) goto error;
	if (read_int (fp, &info->hasEditor)) goto error;
	if (read_int (fp, &info->canProcessReplacing)) goto error;

	if ((info->ParamNames = (char **) malloc(sizeof(char*)*info->numParams)) == 0) {
		goto error;
	}

	for (int i = 0; i < info->numParams; ++i) {
		if ((info->ParamNames[i] = read_string(fp)) == 0) goto error;
	}

	if ((info->ParamLabels = (char **) malloc(sizeof(char*)*info->numParams)) == 0) {
		goto error;
	}
	
	for (int i = 0; i < info->numParams; ++i) {
		if ((info->ParamLabels[i] = read_string(fp)) == 0) goto error;
	}
	
	return info;
	
  error:
	free (info);
	return 0;
}

static int
save_vstfx_info_file (VSTInfo *info, FILE* fp)
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
	
    return 0;
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
		dir = Glib::path_get_dirname (dllpath);
	}

	stringstream s;
	s << "." << Glib::path_get_basename (dllpath) << ".fsi";
	return Glib::build_filename (dir, s.str ());
}

static char *
vstfx_infofile_stat (char *dllpath, struct stat* statbuf, int personal)
{
	if (strstr (dllpath, ".so" ) == 0) {
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
	
	char* own_info = vstfx_infofile_stat (dllpath, &own_statbuf, 1);
	char* sys_info = vstfx_infofile_stat (dllpath, &sys_statbuf, 0);

	if (own_info) {
		if (sys_info) {
			if (own_statbuf.st_mtime <= sys_statbuf.st_mtime) {
				/* system info file is newer, use it */
				return g_fopen (sys_info, "rb");
			}
		} else {
			return g_fopen (own_info, "rb");
		}
	}

	return 0;
}

static FILE *
vstfx_infofile_create (char* dllpath, int personal)
{
	if (strstr (dllpath, ".so" ) == 0) {
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
	if (opcode == audioMasterVersion) {
		return 2;
	} else {
		return 0;
	}
}

/** Try to get plugin info - first by looking for a .fsi cache of the
    data, and if that doesn't exist, load the plugin, get its data and
    then cache it for future ref
*/

VSTInfo *
vstfx_get_info (char* dllpath)
{
	FILE* infofile;
	VSTHandle* h;
	VSTState* vstfx;

	if ((infofile = vstfx_infofile_for_read (dllpath)) != 0) {
		VSTInfo *info;
		info = load_vstfx_info_file (infofile);
		fclose (infofile);
		if (info == 0) {
			PBD::warning << "Cannot get LinuxVST information form " << dllpath << ": info file load failed." << endmsg;
		}
		return info;
	} 
	
	if (!(h = vstfx_load(dllpath))) {
		PBD::warning << "Cannot get LinuxVST information from " << dllpath << ": load failed." << endmsg;
		return 0;
	}
	
	if (!(vstfx = vstfx_instantiate(h, simple_master_callback, 0))) {
	    	vstfx_unload(h);
		PBD::warning << "Cannot get LinuxVST information from " << dllpath << ": instantiation failed." << endmsg;
	    	return 0;
	}
	
	infofile = vstfx_infofile_for_write (dllpath);
	
	if (!infofile) {
		vstfx_close(vstfx);
		vstfx_unload(h);
		PBD::warning << "Cannot get LinuxVST information from " << dllpath << ": cannot create new FST info file." << endmsg;
		return 0;
	}
	
	VSTInfo* info = vstfx_info_from_plugin (vstfx);
	
	save_vstfx_info_file (info, infofile);
	fclose (infofile);
	
	vstfx_close (vstfx);
	vstfx_unload (h);
	
	return info;
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
	free (info);
}


