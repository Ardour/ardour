/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#include <iostream>

#include "pbd/gstdio_compat.h"
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#ifdef COMPILER_MSVC
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#ifndef PLATFORM_WINDOWS
#include <sys/utsname.h>
#endif

#include "vst3/vst3.h"

#include "pbd/basename.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/localtime_r.h"

#include "ardour/filesystem_paths.h"
#include "ardour/vst3_module.h"
#include "ardour/vst3_host.h"
#include "ardour/vst3_scan.h"

using namespace std;
using namespace Steinberg;

static const char* fmt_media (Vst::MediaType m) {
	switch (m) {
		case Vst::kAudio: return "kAudio";
		case Vst::kEvent: return "kEvent";
		default: return "?";
	}
}

static const char* fmt_dir (Vst::BusDirection d) {
	switch (d) {
		case Vst::kInput: return "kInput";
		case Vst::kOutput: return "kOutput";
		default: return "?";
	}
}

static const char* fmt_type (Vst::BusType t) {
	switch (t) {
		case Vst::kMain: return "kMain";
		case Vst::kAux: return "kAux";
		default: return "?";
	}
}

static int32
count_channels (Vst::IComponent* c, Vst::MediaType media, Vst::BusDirection dir, Vst::BusType type, bool verbose = false)
{
	/* see also libs/ardour/vst3_plugin.cc VST3PI::count_channels */
	int32 n_busses = c->getBusCount (media, dir);
	if (verbose) {
		PBD::info << "VST3: media: " << fmt_media (media) << " dir: " << fmt_dir (dir) << " type: " << fmt_type (type) << " n_busses: " << n_busses << endmsg;
	}
	int32 n_channels = 0;
	for (int32 i = 0; i < n_busses; ++i) {
		Vst::BusInfo bus;
		tresult rv = c->getBusInfo (media, dir, i, bus);
		if (rv == kResultTrue && bus.busType == type) {
#if 1
			if ((type == Vst::kMain && i != 0) || (type == Vst::kAux && i != 1)) {
				/* For now allow we only support one main bus, and one aux-bus.
				 * Also an aux-bus by itself is currently N/A.
				 */
				PBD::info << "VST3: \\ ignored bus: " << i << " type: " << fmt_type (bus.busType) << " count: " << bus.channelCount << endmsg;
				continue;
			}
#endif
			if (verbose) {
				PBD::info << "VST3: - bus: " << i << " count: " << bus.channelCount << endmsg;
			}
			if (media == Vst::kEvent) {
#if 0
				/* Supported MIDI Channel count (for a single MIDI input) */
				return std::min<int32> (1, bus.channelCount);
#else
				/* Some plugin leave it at zero, even though they accept events */
				return 1;
#endif
			} else {
				n_channels += bus.channelCount;
			}
		} else if (verbose && rv == kResultTrue) {
			PBD::info << "VST3: \\ ignored bus: " << i << " mismatched type: " << fmt_type (bus.busType) << endmsg;
		} else {
			PBD::info << "VST3: \\ error getting busInfo for bus: " << i << " rv: " << rv << ", got type: " << fmt_type (bus.busType) << endmsg;
		}
	}
	return n_channels;
}

static bool
discover_vst3 (boost::shared_ptr<ARDOUR::VST3PluginModule> m, std::vector<ARDOUR::VST3Info>& rv, bool verbose)
{
	using namespace std;
	using namespace ARDOUR;

	IPluginFactory* factory = m->factory ();

	if (!factory) {
		cerr << "Failed to get VST3 plug-in factory\n";
		return false;
	}

	PFactoryInfo fi;
	if (factory->getFactoryInfo (&fi) != kResultTrue) {
		cerr << "Failed to get VST3 factory info\n";
		return false;
	}

	if (verbose) {
		PBD::info << "FactoryInfo: '" << fi.vendor << "' '" << fi.url << "' '" << fi.email << "'" << endmsg;
	}

	IPtr<IPluginFactory2> factory2 = FUnknownPtr<IPluginFactory2> (factory);

	int32 class_cnt = factory->countClasses ();
	if (verbose) {
		PBD::info << "Class count: " << class_cnt << endmsg;
	}
	for (int32 i = 0; i < class_cnt; ++i) {
		PClassInfo ci;
		if (factory->getClassInfo (i, &ci) == kResultTrue) {
			if (strcmp (ci.category, kVstAudioEffectClass)) {
				if (verbose) {
					PBD::info << "Skipping non-effect class: " << ci.category << endmsg;
				}
				continue;
			}

			VST3Info nfo;
			TUID     uid;

			if (verbose) {
				PBD::info << "Class: " << i << " '" << ci.name << "' '" << ci.category << "'" << endmsg;
			}

			/* pre-fill with factory settings */
			nfo.vendor = strlen (fi.vendor) ==  0 ? "Unknown" : fi.vendor;
			nfo.url    = fi.url;
			nfo.email  = fi.email;

			PClassInfo2      ci2;
			if (factory2 && factory2->getClassInfo2 (i, &ci2) == kResultTrue) {
				memcpy (uid, ci2.cid, sizeof (TUID));
				nfo.name       = ci2.name;
				if (strlen (ci2.vendor) > 0) {
					nfo.vendor     = ci2.vendor;
				}
				nfo.category    = ci2.subCategories;
				nfo.version     = ci2.version;
				nfo.sdk_version = ci2.sdkVersion;
			} else {
				memcpy (uid, ci.cid, sizeof (TUID));
				nfo.name        = ci.name;
				nfo.version     = "0.0.0";
				nfo.sdk_version = "VST 3";
			}

			{
				char suid[33] = "";
				FUID::fromTUID (uid).toString (suid);
				nfo.uid = suid;
			}

			Vst::IComponent* component;
			if (factory->createInstance (uid, Vst::IComponent::iid, (void**)&component) != kResultTrue) {
				cerr << "Failed to create VST3 component\n";
				continue;
			}

			// TODO init params

			if (component->initialize (HostApplication::getHostContext ()) != kResultOk) {
				cerr << "Failed to initialize VST3 component\n";
				continue;
			}

			IPtr<Vst::IAudioProcessor> processor;
			if (!(processor = FUnknownPtr<Vst::IAudioProcessor> (component))) {
				cerr << "VST3: No valid processor";
				component->terminate ();
				component->release ();
				continue;
			}

			if (processor->canProcessSampleSize (Vst::kSample32) != kResultTrue) {
				cerr << "VST3: Cannot process 32bit float";
				component->terminate ();
				component->release ();
				continue;
			}

			nfo.n_inputs       = count_channels (component, Vst::kAudio, Vst::kInput,  Vst::kMain, verbose);
			nfo.n_aux_inputs   = count_channels (component, Vst::kAudio, Vst::kInput,  Vst::kAux, verbose);
			nfo.n_outputs      = count_channels (component, Vst::kAudio, Vst::kOutput, Vst::kMain, verbose);
			nfo.n_aux_outputs  = count_channels (component, Vst::kAudio, Vst::kOutput, Vst::kAux, verbose);
			nfo.n_midi_inputs  = count_channels (component, Vst::kEvent, Vst::kInput,  Vst::kMain, verbose);
			nfo.n_midi_outputs = count_channels (component, Vst::kEvent, Vst::kOutput, Vst::kMain, verbose);

			processor->setProcessing (false);
			component->setActive (false);

			component->terminate ();
			component->release ();
			rv.push_back (nfo);
		}
	}

	return true;
}

static std::string vst3_bindir () {
#ifdef __APPLE__
	return "MacOS";
#elif defined PLATFORM_WINDOWS
# if defined __x86_64__ || defined _M_X64
	return "x86_64-win";
# else
	return "x86-win";
# endif
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
# warning VST3 bundle on *BSD is not defined in the spec
	/* https://steinbergmedia.github.io/vst3_doc/vstinterfaces/vst3loc.html
	 * does not mention BSD. The following follows the Linux convention:
	 */
	static std::string _vst3_bindir;
	if (_vst3_bindir.empty ()) {
		struct utsname utb;
		if (uname (&utb) >= 0) {
			_vst3_bindir = std::string (utb.machine) + "-bsd";
		}
	}
	return _vst3_bindir;
#else // Linux
# if defined __x86_64__ || defined _M_X64
	return "x86_64-linux";
# elif defined __i386__  || defined _M_IX86
	return "i386-linux";
# elif defined __aarch64__
	return "aarch64-linux";
# else
	/* generic fallback, mainly used for __arm__ to expand
	 * to armv6l-linux, armv7l-linux, armv8l-linux.
	 */
	static std::string _vst3_bindir;
	if (_vst3_bindir.empty ()) {
		struct utsname utb;
		if (uname (&utb) >= 0) {
			_vst3_bindir = std::string (utb.machine) + "-linux";
		}
	}
	return _vst3_bindir;
# endif
#endif
	return "";
}

static std::string vst3_suffix () {
#ifdef __APPLE__
	return "";
#elif defined PLATFORM_WINDOWS
	return ".vst3";
#else // Linux
	return ".so";
#endif
}

std::string
ARDOUR::module_path_vst3 (string const& path)
{
	string module_path;

	if (!Glib::file_test (path, Glib::FILE_TEST_IS_DIR)) {
#ifdef PLATFORM_WINDOWS
		/* Until VST 3.6.10, the SDK allowed VST3 as a single dll file with the
		 * vst3 extension. Since the folder is scanned recursively this leads to
		 * an ambiguity (bundle and file):
		 * ...\plugin.vst3
		 * ...\plugin.vst3\Contents\x64_64-win\plugin.vst3
		 */
		std::string p1 = Glib::path_get_dirname (path);
		std::string p2 = Glib::path_get_dirname (p1);
		std::string p3 = Glib::path_get_dirname (p2);
		if (  (Glib::path_get_basename (p1) == "x86_64-win" || Glib::path_get_basename (p1) == "x86-win")
		    && Glib::path_get_basename (p2) == "Contents"
		    && Glib::path_get_basename (p3) == Glib::path_get_basename (path)
		   ) {
			/* Ignore *.vst3 dll if it resides inside a bundle with the same name.
			 * Ardour will instead use the bundle.
			 */
			return "";
		}
#endif
		module_path = path;
	} else {
		module_path = Glib::build_filename (path, "Contents",
				vst3_bindir (), PBD::basename_nosuffix (path) + vst3_suffix ());
	}

#ifdef __APPLE__
	/* Check for "Contents/MacOS/" and "Context/Info.plist".
	 * VST3MacModule calls CFBundleCreate() which handles Info.plist files.
	 * (on macOS/X the binrary name may differ from the bundle name)
	 */
	string plist = Glib::build_filename (path, "Contents", "Info.plist");
	if (Glib::file_test (Glib::path_get_dirname (module_path), Glib::FILE_TEST_IS_DIR) &&
	    Glib::file_test (Glib::build_filename (path, "Contents", "Info.plist"), Glib::FILE_TEST_IS_REGULAR)) {
		return plist;
	} else {
		cerr << "VST3 not a valid bundle: '" << path << "'\n";
		return "";
	}
#endif

	if (!Glib::file_test (module_path, Glib::FILE_TEST_IS_REGULAR)) {
		cerr << "VST3 not a valid bundle: '" << module_path << "'\n";
		return "";
	}
	return module_path;
}

static string
vst3_info_cache_dir ()
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
ARDOUR::vst3_cache_file (std::string const& module_path)
{
	char hash[41];
	Sha1Digest s;
	sha1_init (&s);
	sha1_write (&s, (const uint8_t *) module_path.c_str(), module_path.size());
	sha1_result_hash (&s, hash);
	return Glib::build_filename (vst3_info_cache_dir (), std::string (hash) + std::string (".v3i"));
}

string
ARDOUR::vst3_valid_cache_file (std::string const& module_path, bool verbose, bool* is_new)
{
	string const cache_file = ARDOUR::vst3_cache_file (module_path);
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
	GStatBuf sb_v3i;

	if (g_stat (module_path.c_str(), &sb_vst) == 0 && g_stat (cache_file.c_str (), &sb_v3i) == 0) {
		if (sb_vst.st_mtime < sb_v3i.st_mtime) {
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
touch_cachefile (std::string const& module_path, std::string const& cache_file, bool verbose)
{
	GStatBuf sb_vst;
	GStatBuf sb_v3i;
	if (g_stat (module_path.c_str(), &sb_vst) == 0 && g_stat (cache_file.c_str (), &sb_v3i) == 0) {
		struct utimbuf utb;
		utb.actime = sb_v3i.st_atime;
		utb.modtime = std::max (sb_vst.st_mtime, sb_v3i.st_mtime);
		if (0 != g_utime (cache_file.c_str (), &utb)) {
			PBD::error << "Could not set cachefile timestamp." << endmsg;
		} else if (verbose) {
			const time_t mtime = sb_vst.st_mtime;
			char v3itme[128];
			char vsttme[128];
			struct tm local_time;
			localtime_r (&utb.modtime, &local_time);
			strftime (v3itme, sizeof(v3itme), "%Y-%m-%d %H:%M:%S", &local_time);
			localtime_r (&mtime, &local_time);
			strftime (vsttme, sizeof(vsttme), "%Y-%m-%d %H:%M:%S", &local_time);
			PBD::info << "Touch cachefile: set mtime = "
			          << utb.modtime << " (" << v3itme << "), plugin mtime = "
			          << sb_vst.st_mtime << " (" << vsttme << ")" << endmsg;
		}
	} else {
		PBD::error << "Could not stat VST3 module." << endmsg;
	}
}

static bool
vst3_save_cache_file (std::string const& module_path, XMLNode* root, bool verbose)
{
	string const cache_file = ARDOUR::vst3_cache_file (module_path);

	XMLTree tree;
	tree.set_root (root);
	if (!tree.write (cache_file)) {
		PBD::error << "Could not save VST3 plugin cache to: " << cache_file << endmsg;
		return false;
	} else {
		touch_cachefile (module_path, cache_file, verbose);
	}
	if (verbose) {
		root->dump (std::cout, "\t");
	}
	return true;
}

bool
ARDOUR::vst3_scan_and_cache (std::string const& module_path, std::string const& bundle_path, boost::function<void (std::string const&, std::string const&, VST3Info const&)> cb, bool verbose)
{
	XMLNode* root = new XMLNode ("VST3Cache");
	root->set_property ("version", 1);
	root->set_property ("bundle", bundle_path);
	root->set_property ("module", module_path);

	try {
		boost::shared_ptr<VST3PluginModule> m = VST3PluginModule::load (module_path);
		std::vector<VST3Info> nfo;
		if (!discover_vst3 (m, nfo, verbose)) {
			delete root;
			return false;
		}
		if (nfo.empty ()) {
			cerr << "No plugins in VST3 module: '" << module_path << "'\n";
			delete root;
			return false;
		}
		for (std::vector<VST3Info>::const_iterator i = nfo.begin(); i != nfo.end(); ++i) {
			cb (module_path, bundle_path, *i);
			root->add_child_nocopy (i->state ());
		}

	} catch (...) {
		cerr << "Cannot load VST3 module: '" << module_path << "'\n";
		delete root;
		return false;
	}

	return vst3_save_cache_file (module_path, root, verbose);
}


using namespace ARDOUR;

VST3Info::VST3Info (XMLNode const& node)
	: n_inputs (0)
	, n_outputs (0)
	, n_aux_inputs (0)
	, n_aux_outputs (0)
	, n_midi_inputs (0)
	, n_midi_outputs (0)
{
	bool err = false;

	if (node.name() != "VST3Info") {
		throw failed_constructor ();
	}
	err |= !node.get_property ("uid", uid);
	err |= !node.get_property ("name", name);
	err |= !node.get_property ("vendor", vendor);
	err |= !node.get_property ("category", category);
	err |= !node.get_property ("version", version);
	err |= !node.get_property ("sdk-version", sdk_version);
	err |= !node.get_property ("url", url);
	err |= !node.get_property ("email", email);

	err |= !node.get_property ("n_inputs", n_inputs);
	err |= !node.get_property ("n_outputs", n_outputs);
	err |= !node.get_property ("n_aux_inputs", n_aux_inputs);
	err |= !node.get_property ("n_aux_outputs", n_aux_outputs);
	err |= !node.get_property ("n_midi_inputs", n_midi_inputs);
	err |= !node.get_property ("n_midi_outputs", n_midi_outputs);

	if (err) {
		throw failed_constructor ();
	}
}

XMLNode&
VST3Info::state () const
{
	XMLNode* node = new XMLNode("VST3Info");
	node->set_property ("uid",         uid);
	node->set_property ("name",        name);
	node->set_property ("vendor",      vendor);
	node->set_property ("category",    category);
	node->set_property ("version",     version);
	node->set_property ("sdk-version", sdk_version);
	node->set_property ("url",         url);
	node->set_property ("email",       email);

	node->set_property ("n_inputs",       n_inputs);
	node->set_property ("n_outputs",      n_outputs);
	node->set_property ("n_aux_inputs",   n_aux_inputs);
	node->set_property ("n_aux_outputs",  n_aux_outputs);
	node->set_property ("n_midi_inputs",  n_midi_inputs);
	node->set_property ("n_midi_outputs", n_midi_outputs);
	return *node;
}
