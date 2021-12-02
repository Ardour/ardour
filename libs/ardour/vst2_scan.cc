/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#ifdef COMPILER_MSVC
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#ifdef PLATFORM_WINDOWS
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#else
#include <sys/utsname.h>
#endif

#include "pbd/basename.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/localtime_r.h"

#include "ardour/filesystem_paths.h"
#include "ardour/linux_vst_support.h"
#include "ardour/mac_vst_support.h"
#include "ardour/vst_types.h"
#include "ardour/vst2_scan.h"

#ifdef WINDOWS_VST_SUPPORT
#include <fst.h>
#endif

#include "pbd/i18n.h"

using namespace std;

/* ID for shell plugins */
static int  vstfx_current_loading_id = 0;
static bool vstfx_verbose_log = false;

/* ****************************************************************************
 * VST system-under-test methods
 */

static
bool vstfx_midi_input (VSTState* vstfx)
{
	AEffect* plugin = vstfx->plugin;

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
		intptr_t rv = 0;
		for (int i = 0; i < vstfx_can_do_string_count; i++) {
			if (! strcmp (vstfx_can_do_strings[i], (const char*)ptr)) {
				rv = 1;
				break;
			}
		}
		if (vstfx_verbose_log) {
			PBD::info << string_compose ("Callback CanDo '%1': %2", (const char*)ptr, rv ? "yes" : "no") << endmsg;
		}
		return rv;
	}
	else if (opcode == audioMasterCurrentId) {
		return vstfx_current_loading_id;
	}
	else {
		if (vstfx_verbose_log) {
			PBD::info << string_compose ("Callback opcode = %1 (ignored)", opcode) << endmsg;
		}
		return 0;
	}
}

/** query VST Info */
static void
vstfx_parse_vst_state (ARDOUR::VST2Info& nfo, VSTState* vstfx, bool verbose)
{
	assert (vstfx);
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
		nfo.name = vstfx->handle->name;
	} else {
		nfo.name = name;
	}

	/*If the plugin doesn't bother to implement GetVendorString we will
	 * have pre-stuffed the string with 'Unknown' */

	plugin->dispatcher (plugin, effGetVendorString, 0, 0, creator, 0);

	/* Some plugins DO implement GetVendorString, but DON'T put a name in it
	 * so if its just a zero length string we replace it with 'Unknown' */

	if (strlen (creator) == 0) {
		nfo.creator = "Unknown";
	} else {
		nfo.creator = creator;
	}

	switch (plugin->dispatcher (plugin, effGetPlugCategory, 0, 0, 0, 0))
	{
		case kPlugCategEffect:         nfo.category = "Effect"; break;
		case kPlugCategSynth:          nfo.category = "Instrument"; break;
		case kPlugCategAnalysis:       nfo.category = "Analyser"; break;
		case kPlugCategMastering:      nfo.category = "Mastering"; break;
		case kPlugCategSpacializer:    nfo.category = "Spatial"; break;
		case kPlugCategRoomFx:         nfo.category = "RoomFx"; break;
		case kPlugSurroundFx:          nfo.category = "SurroundFx"; break;
		case kPlugCategRestoration:    nfo.category = "Restoration"; break;
		case kPlugCategOfflineProcess: nfo.category = "Offline"; break;
		case kPlugCategShell:          nfo.category = "Shell"; break;
		case kPlugCategGenerator:      nfo.category = "Generator"; break;
		default:                       nfo.category = "Unknown"; break;
	}

	nfo.id = plugin->uniqueID;
	nfo.n_inputs = plugin->numInputs;
	nfo.n_outputs = plugin->numOutputs;
	nfo.has_editor = plugin->flags & effFlagsHasEditor ? true : false;
	nfo.can_process_replace = plugin->flags & effFlagsCanReplacing ? true : false;
	nfo.n_midi_inputs = vstfx_midi_input (vstfx) ? 1 : 0;
	nfo.n_midi_outputs = vstfx_midi_output (vstfx) ? 1 : 0;
	nfo.is_instrument = (plugin->flags & effFlagsIsSynth) ? 1 : 0;

#ifdef __APPLE__
	if (nfo.has_editor) {
		/* we only support Cocoa UIs (just like Reaper) */
		nfo.has_editor = (plugin->dispatcher (plugin, effCanDo, 0, 0, const_cast<char*> ("hasCockosViewAsConfig"), 0.0f) & 0xffff0000) == 0xbeef0000;
	}
#endif

#if 0
	for (int i = 0; i < plugin->numParams; ++i) {
		char name[VestigeMaxLabelLen];
		char label[VestigeMaxLabelLen];

		/* Not all plugins give parameters labels as well as names */
		strcpy (name, "No Name");
		strcpy (label, "No Label");

		plugin->dispatcher (plugin, effGetParamName, i, 0, name, 0);
		plugin->dispatcher (plugin, 6 /* effGetParamLabel */, i, 0, label, 0);

		nfp.param[name] = label;
	}
#endif
}

static void
vst2_close (VSTState* vstfx, ARDOUR::PluginType type)
{
	/* mark as used, prevent *_close from unloading the plugin */
	VSTHandle* h = vstfx->handle;
	h->plugincnt++;

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
	h->plugincnt--;
}

static void
vst2_unload (VSTHandle* h, ARDOUR::PluginType type)
{
	switch (type) {
#ifdef WINDOWS_VST_SUPPORT
		case ARDOUR::Windows_VST:
			fst_unload (&h);
			break;
#endif
#ifdef LXVST_SUPPORT
		case ARDOUR::LXVST:
			vstfx_unload (h);
			break;
#endif
#ifdef MACVST_SUPPORT
		case ARDOUR::MacVST:
			mac_vst_unload (h);
			break;
#endif
		default:
			assert (0);
			break;
	}
}

static VSTHandle*
vst2_load (const char* dllpath, ARDOUR::PluginType type)
{
	VSTHandle* h = NULL;
	switch (type) {
#ifdef WINDOWS_VST_SUPPORT
		case ARDOUR::Windows_VST:
			h = fst_load (dllpath);
			break;
#endif
#ifdef LXVST_SUPPORT
		case ARDOUR::LXVST:
			h = vstfx_load (dllpath);
			break;
#endif
#ifdef MACVST_SUPPORT
		case ARDOUR::MacVST:
			h = mac_vst_load (dllpath);
			break;
#endif
		default:
			assert (0);
			break;
	}
	return h;
}

static VSTState*
vst2_instantiate (VSTHandle* h, ARDOUR::PluginType type)
{
	VSTState* s = NULL;
	switch (type) {
#ifdef WINDOWS_VST_SUPPORT
		case ARDOUR::Windows_VST:
			s = fst_instantiate (h, simple_master_callback, 0);
			break;
#endif
#ifdef LXVST_SUPPORT
		case ARDOUR::LXVST:
			s = vstfx_instantiate (h, simple_master_callback, 0);
			break;
#endif
#ifdef MACVST_SUPPORT
		case ARDOUR::MacVST:
			s = mac_vst_instantiate (h, simple_master_callback, 0);
			break;
#endif
		default:
			assert (0);
			break;
	}
	return s;
}

#ifdef PLATFORM_WINDOWS
static std::string
dll_info (std::string path)
{
	std::string rv;
	uint8_t buf[68];
	uint16_t type = 0;
	off_t pe_hdr_off = 0;

	int fd = g_open(path.c_str(), O_RDONLY, 0444);

	if (fd < 0) {
		return string_compose (_("cannot open dll '%1' (%2)"), path, g_strerror (errno));
	}

	if (68 != read (fd, buf, 68)) {
		rv = _("invalid dll, file too small");
		goto errorout;
	}
	if (buf[0] != 'M' && buf[1] != 'Z') {
		rv = _("not a dll");
		goto errorout;
	}

	pe_hdr_off = *((int32_t*) &buf[60]);
	if (pe_hdr_off !=lseek (fd, pe_hdr_off, SEEK_SET)) {
		rv = _("cannot determine dll type");
		goto errorout;
	}
	if (6 != read (fd, buf, 6)) {
		rv = _("cannot read dll PE header");
		goto errorout;
	}

	if (buf[0] != 'P' && buf[1] != 'E') {
		rv = _("invalid dll PE header");
		goto errorout;
	}

	type = *((uint16_t*) &buf[4]);
	switch (type) {
		case 0x014c:
			rv = _("i386 (32-bit)");
			break;
		case  0x0200:
			rv = _("Itanium");
			break;
		case 0x8664:
			rv = _("x64 (64-bit)");
			break;
		case 0:
			rv = _("Native Architecture");
			break;
		default:
			rv = _("Unknown Architecture");
			break;
	}
errorout:
	assert (rv.length() > 0);
	close (fd);
	return rv;
}
#endif

std::string
ARDOUR::vst2_id_to_str (int32_t id)
{
	std::string rv;
	for (int i = 0; i < 4; ++i) {
		char a = ((char*)&id)[i];
		if (isprint (a)) {
			rv += a;
		} else {
			rv += '.';
		}
	}
	return rv;
}
std::string
ARDOUR::vst2_arch ()
{
#ifndef PLATFORM_WINDOWS
	struct utsname utb;
	if (uname (&utb) >= 0) {
		return utb.machine;
	}
#endif

#if ( defined(__x86_64__) || defined(_M_X64) )
	return "x86_64";
#elif defined __i386__  || defined _M_IX86
	return "i386";
#elif defined __ppc__ && defined  __LP64__
	return "ppc64";
#elif defined __ppc__
	return "ppc";
#elif defined  __aarch64__
	return "aarch64";
#elif defined  __arm__ && defined __ARM_NEON
	return "armhf";
#elif defined  __arm__
	return "arm";
#elif defined  __LP64__
	return "x64";
#else
	return "x32";
#endif
}

/** wrapper around \ref vstfx_parse_vst_state,
 * iterate over plugins in shell, translate VST-info into ardour VSTState
 */
static void
vstfx_info_from_plugin (VSTHandle* h, VSTState* vstfx, vector<ARDOUR::VST2Info>& rv, enum ARDOUR::PluginType type, bool verbose)
{
	assert (vstfx);

	ARDOUR::VST2Info nfo;
	vstfx_parse_vst_state (nfo, vstfx, verbose);

	if (strncmp (nfo.category.c_str(), "Shell", 5) /*|| vstfx->handle->plugincnt != 1 */) {
		rv.push_back (nfo);
		vst2_close (vstfx, type);
		return;
	}

	if (verbose) {
		PBD::info << "VST Shell Plugin" << endmsg;
	}

	/* shell plugin.
	 * read the info for all of the plugins contained in this shell.
	 */
	int id;
	vector< pair<int, string> > ids;
	AEffect* plugin = vstfx->plugin;

	do {
		char name[65] = "Unknown";
		id = plugin->dispatcher (plugin, effShellGetNextPlugin, 0, 0, name, 0);
		ids.push_back (std::make_pair (id, name));
	} while (id != 0);

	vst2_close (vstfx, type);

	if (verbose) {
		PBD::info << "Found " << ids.size() << " Plugin(s) in shell)" << endmsg;
	}

	for (vector< pair<int, string> >::iterator x = ids.begin (); x != ids.end (); ++x) {
		id = (*x).first;
		if (id == 0) {
			if (verbose) {
				PBD::info << string_compose (_("Skipping VST2 Shell ID: '%1' Name: '%2'"), id, (*x).second) << endmsg;
			}
			continue;
		}
		/* recurse vstfx_get_info() */
		vstfx_current_loading_id = id;
		if (verbose) {
			PBD::info << string_compose (_("Instantiating VST2 Shell ID: '%1' Name: '%2'"), ARDOUR::vst2_id_to_str(id), (*x).second) << endmsg;
		}
		vstfx = vst2_instantiate (h, type);
		if (!vstfx) {
			PBD::warning << string_compose (_("Error scanning VST2 Shell ID: '%1' Name: '%2'"), ARDOUR::vst2_id_to_str(id), (*x).second) << endmsg;
			continue;
		}
		vstfx_info_from_plugin (h, vstfx, rv, type, verbose);
	}
}

static bool
discover_vst2 (std::string const& path, ARDOUR::PluginType type, std::vector<ARDOUR::VST2Info>& rv, bool verbose)
{
	VSTHandle* h = vst2_load (path.c_str (), type);

	if (!h) {
		PBD::warning << string_compose (_("Cannot open VST2 module '%1'"), path) << endmsg;
		return false;
	}

	vstfx_current_loading_id = 0;
	vstfx_verbose_log        = verbose;
	VSTState* vstfx = vst2_instantiate (h, type);

	if (!vstfx) {
		vst2_unload (h, type);
		PBD::warning << string_compose (_("Cannot get VST information from '%1': instantiation failed."), path) << endmsg;
		return false;
	}

	vstfx_info_from_plugin (h, vstfx, rv, type, verbose);
	vst2_unload (h, type);
	return true;
}

static string
vst2_info_cache_dir ()
{
	string dir = Glib::build_filename (ARDOUR::user_cache_directory (), "vst");
	/* if the directory doesn't exist, try to create it */
	if (!Glib::file_test (dir, Glib::FILE_TEST_IS_DIR)) {
		if (g_mkdir (dir.c_str (), 0700)) {
			PBD::fatal << "Cannot create VST info folder '" << dir << "'" << endmsg;
		}
	}
	return dir;
}

#include "sha1.c"

string
ARDOUR::vst2_cache_file (std::string const& path)
{
	char hash[41];
	Sha1Digest s;
	sha1_init (&s);
	sha1_write (&s, (const uint8_t *) path.c_str(), path.size());
	sha1_result_hash (&s, hash);
	/* Arch specifix suffix or arch specific folder?
	 * start 32bit Ardour, scan 32bit plugin -> create cache file.
	 * start 64bit Ardour, scan 32bit plugin -> invalid -> cleanup -> cache file is removed
	 */
# if ( defined(__x86_64__) || defined(_M_X64) )
	return Glib::build_filename (vst2_info_cache_dir (), std::string (hash) + std::string ("-x64.v2i"));
#else
	return Glib::build_filename (vst2_info_cache_dir (), std::string (hash) + std::string ("-x86.v2i"));
#endif
}

string
ARDOUR::vst2_valid_cache_file (std::string const& path, bool verbose, bool* is_new)
{
	string const cache_file = ARDOUR::vst2_cache_file (path);
	if (!Glib::file_test (cache_file, Glib::FileTest (Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR))) {
		if (is_new) {
			*is_new = true;
		}
		return "";
	}

	if (is_new) {
		*is_new = false;
	}

	if (verbose) {
		PBD::info << "Found cache file: '" << cache_file <<"'" << endmsg;
	}

	GStatBuf sb_vst;
	GStatBuf sb_v2i;

	if (g_stat (path.c_str(), &sb_vst) == 0 && g_stat (cache_file.c_str (), &sb_v2i) == 0) {
		if (sb_vst.st_mtime < sb_v2i.st_mtime) {
			/* plugin is older than cache file */
			if (verbose) {
				PBD::info << "Cache file is up-to-date." << endmsg;
			}
			return cache_file;
		} else if  (verbose) {
			PBD::info << "Stale cache." << endmsg;
		}
	}
	return "";
}

static void
touch_cachefile (std::string const& path, std::string const& cache_file, bool verbose)
{
	GStatBuf sb_vst;
	GStatBuf sb_v2i;
	if (g_stat (path.c_str(), &sb_vst) == 0 && g_stat (cache_file.c_str (), &sb_v2i) == 0) {
		struct utimbuf utb;
		utb.actime = sb_v2i.st_atime;
		utb.modtime = std::max (sb_vst.st_mtime, sb_v2i.st_mtime);
		if (0 != g_utime (cache_file.c_str (), &utb)) {
			PBD::error << "Could not set cachefile timestamp." << endmsg;
		} else if (verbose) {
			const time_t mtime = sb_vst.st_mtime;
			char v2itme[128];
			char vsttme[128];
			struct tm local_time;
			localtime_r (&utb.modtime, &local_time);
			strftime (v2itme, sizeof(v2itme), "%Y-%m-%d %H:%M:%S", &local_time);
			localtime_r (&mtime, &local_time);
			strftime (vsttme, sizeof(vsttme), "%Y-%m-%d %H:%M:%S", &local_time);
			PBD::info << "Touch cachefile: set mtime = "
			          << utb.modtime << " (" << v2itme << "), plugin mtime = "
			          << sb_vst.st_mtime << " (" << vsttme << ")" << endmsg;
		}
	} else {
		PBD::error << "Could not stat plugin." << endmsg;
	}
}

static bool
vst2_save_cache_file (std::string const& path, XMLNode* root, bool verbose)
{
	string const cache_file = ARDOUR::vst2_cache_file (path);

	XMLTree tree;
	tree.set_root (root);
	if (!tree.write (cache_file)) {
		PBD::error << "Could not save VST2 plugin cache to: " << cache_file << endmsg;
		return false;
	} else {
		touch_cachefile (path, cache_file, verbose);
	}
	if (verbose) {
		root->dump (std::cout, "\t");
	}
	return true;
}

bool
ARDOUR::vst2_scan_and_cache (std::string const& path, ARDOUR::PluginType type, boost::function<void (std::string const&, PluginType, VST2Info const&)> cb, bool verbose)
{
	XMLNode* root = new XMLNode ("VST2Cache");
	root->set_property ("version", 1);
	root->set_property ("binary", path);
	root->set_property ("arch", vst2_arch ());

#ifdef PLATFORM_WINDOWS
	if (type == ARDOUR::Windows_VST) {
		PBD::info << "File type: " << dll_info (path) << endmsg;
	}
#endif

	try {
		std::vector<VST2Info> nfo;
		if (!discover_vst2 (path, type, nfo, verbose)) {
			delete root;
			return false;
		}
		if (nfo.empty ()) {
			PBD::warning << string_compose (_("No plugins in VST2 module '%1'"), path) << endmsg;
			delete root;
			return false;
		}
		for (std::vector<VST2Info>::const_iterator i = nfo.begin(); i != nfo.end(); ++i) {
			cb (path, type, *i);
			root->add_child_nocopy (i->state ());
		}
	} catch (...) {
		PBD::warning << string_compose (_("Cannot load VST plugin '%1'"), path) << endmsg;
		delete root;
		return false;
	}

	return vst2_save_cache_file (path, root, verbose);
}


using namespace ARDOUR;

VST2Info::VST2Info (XMLNode const& node)
	: id (0)
	, n_inputs (0)
	, n_outputs (0)
	, n_midi_inputs (0)
	, n_midi_outputs (0)
	, is_instrument (false)
	, can_process_replace (false)
	, has_editor (false)
{
	bool err = false;

	if (node.name() != "VST2Info") {
		throw failed_constructor ();
	}
	err |= !node.get_property ("id", id);
	err |= !node.get_property ("name", name);
	err |= !node.get_property ("creator", creator);
	err |= !node.get_property ("category", category);
	err |= !node.get_property ("version", version);

	err |= !node.get_property ("n_inputs", n_inputs);
	err |= !node.get_property ("n_outputs", n_outputs);
	err |= !node.get_property ("n_midi_inputs", n_midi_inputs);
	err |= !node.get_property ("n_midi_outputs", n_midi_outputs);

	err |= !node.get_property ("is_instrument", is_instrument);
	err |= !node.get_property ("can_process_replace", can_process_replace);
	err |= !node.get_property ("has_editor", has_editor);

	if (err) {
		throw failed_constructor ();
	}
}

XMLNode&
VST2Info::state () const
{
	XMLNode* node = new XMLNode("VST2Info");
	node->set_property ("id",       id);
	node->set_property ("name",     name);
	node->set_property ("creator",  creator);
	node->set_property ("category", category);
	node->set_property ("version",  version);

	node->set_property ("n_inputs",       n_inputs);
	node->set_property ("n_outputs",      n_outputs);
	node->set_property ("n_midi_inputs",  n_midi_inputs);
	node->set_property ("n_midi_outputs", n_midi_outputs);

	node->set_property ("is_instrument",        is_instrument);
	node->set_property ("can_process_replace",  can_process_replace);
	node->set_property ("has_editor",           has_editor);
	return *node;
}

