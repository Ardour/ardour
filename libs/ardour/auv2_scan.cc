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
#include <utime.h>

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include "CAAudioUnit.h"
#include "CAAUParameter.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioUnitUtilities.h>
#ifdef WITH_CARBON
#include <Carbon/Carbon.h>
#endif

#undef nil

#ifdef COREAUDIO105
#define ArdourComponent Component
#define ArdourDescription ComponentDescription
#define ArdourFindNext FindNextComponent
#else
#define ArdourComponent AudioComponent
#define ArdourDescription AudioComponentDescription
#define ArdourFindNext AudioComponentFindNext
#endif

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/basename.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/whitespace.h"

#include "ardour/filesystem_paths.h"
#include "ardour/auv2_scan.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;

ARDOUR::AUv2DescStr::AUv2DescStr (std::string const& desc)
{
	if (desc.empty ()) {
		return;
	}
	if (desc.size () == 14 && desc[4] == '-' && desc[9] == '-') {
		type = desc.substr (0,4);
		subt = desc.substr (5,4);
		manu = desc.substr (10,4);
	}
	if (!valid ()) {
		type = "";
		subt = "";
		manu = "";
	}
}

std::string
ARDOUR::AUv2DescStr::to_s () const
{
	return type + "-" + subt + "-" + manu;
}

bool
ARDOUR::AUv2DescStr::valid () const {
	return type.size () == 4 && subt.size() == 4 && manu.size() == 4;
}

CAComponentDescription
ARDOUR::AUv2DescStr::desc () const {

	CFStringRef s_type = CFStringCreateWithCString (kCFAllocatorDefault, type.c_str(), kCFStringEncodingUTF8);
	CFStringRef s_subt = CFStringCreateWithCString (kCFAllocatorDefault, subt.c_str(), kCFStringEncodingUTF8);
	CFStringRef s_manu = CFStringCreateWithCString (kCFAllocatorDefault, manu.c_str(), kCFStringEncodingUTF8);

	OSType t = UTGetOSTypeFromString (s_type);
	OSType s = UTGetOSTypeFromString (s_subt);
	OSType m = UTGetOSTypeFromString (s_manu);

	CAComponentDescription desc (t, s, m);

	CFRelease (s_type);
	CFRelease (s_subt);
	CFRelease (s_manu);

	return desc;
}

/* ****************************************************************************/

/* copied from ardour/utils.cc */
static string
CFStringRefToStdString(CFStringRef stringRef)
{
	CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(stringRef) , kCFStringEncodingUTF8);
	char *buf = new char[size];

	std::string result;

	if(CFStringGetCString(stringRef, buf, size, kCFStringEncodingUTF8)) {
		result = buf;
	}
	delete [] buf;
	return result;
}

std::string
ARDOUR::auv2_stringify_descriptor (CAComponentDescription const& desc)
{
	CFStringRef s_type = UTCreateStringForOSType (desc.Type ());
	CFStringRef s_subt = UTCreateStringForOSType (desc.SubType ());
	CFStringRef s_manu = UTCreateStringForOSType (desc.Manu ());

	ARDOUR::AUv2DescStr foo;
	if (s_type && s_subt && s_manu) {
		char tmp[5];
		CFStringGetCString (s_type, tmp, 5, kCFStringEncodingUTF8);
		foo.type = tmp;
		CFStringGetCString (s_subt, tmp, 5, kCFStringEncodingUTF8);
		foo.subt = tmp;
		CFStringGetCString (s_manu, tmp, 5, kCFStringEncodingUTF8);
		foo.manu = tmp;
	} else {
		assert (0);
	}

	CFRelease (s_type);
	CFRelease (s_subt);
	CFRelease (s_manu);

	return foo.to_s ();
}

static void
#ifdef COREAUDIO105
get_names (CAComponentDescription& comp_desc, std::string& name, std::string& maker)
#else
get_names (ArdourComponent& comp, std::string& name, std::string& maker)
#endif
{
	CFStringRef itemName = NULL;
	// Marc Poirier-style item name
#ifdef COREAUDIO105
	CAComponent auComponent (comp_desc);
	if (auComponent.IsValid()) {
		CAComponentDescription dummydesc;
		Handle nameHandle = NewHandle(sizeof(void*));
		if (nameHandle != NULL) {
			OSErr err = GetComponentInfo(auComponent.Comp(), &dummydesc, nameHandle, NULL, NULL);
			if (err == noErr) {
				ConstStr255Param nameString = (ConstStr255Param) (*nameHandle);
				if (nameString != NULL) {
					itemName = CFStringCreateWithPascalString(kCFAllocatorDefault, nameString, CFStringGetSystemEncoding());
				}
			}
			DisposeHandle(nameHandle);
		}
	}
#else
	assert (comp);
	AudioComponentCopyName (comp, &itemName);
#endif

	// if Marc-style fails, do the original way
	if (itemName == NULL) {
#ifndef COREAUDIO105
		CAComponentDescription comp_desc;
		AudioComponentGetDescription (comp, &comp_desc);
#endif
		CFStringRef compTypeString = UTCreateStringForOSType(comp_desc.componentType);
		CFStringRef compSubTypeString = UTCreateStringForOSType(comp_desc.componentSubType);
		CFStringRef compManufacturerString = UTCreateStringForOSType(comp_desc.componentManufacturer);

		itemName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@ - %@ - %@"),
				compTypeString, compManufacturerString, compSubTypeString);

		if (compTypeString != NULL)
			CFRelease(compTypeString);
		if (compSubTypeString != NULL)
			CFRelease(compSubTypeString);
		if (compManufacturerString != NULL)
			CFRelease(compManufacturerString);
	}

	string str = CFStringRefToStdString(itemName);
	string::size_type colon = str.find (':');

	if (colon) {
		name = str.substr (colon+1);
		maker = str.substr (0, colon);
		strip_whitespace_edges (maker);
		strip_whitespace_edges (name);
	} else {
		name = str;
		maker = "unknown";
		strip_whitespace_edges (name);
	}
#if 0
	if (itemName != NULL) {
		CFRelease(itemName);
	}
#endif
}

static void
auv2_plugin_info (ArdourComponent& comp, CAComponentDescription& desc, std::vector<ARDOUR::AUv2Info>& rv, bool verbose)
{
	ARDOUR::AUv2Info info;

	switch (desc.Type()) {
		case kAudioUnitType_Panner:
		case kAudioUnitType_OfflineEffect:
		case kAudioUnitType_FormatConverter:
			//
			return;
		case kAudioUnitType_Output:
			info.category = _("Output");
			break;
		case kAudioUnitType_MusicDevice:
			info.category = _("Instrument");
			info.n_midi_inputs = 1;
			break;
		case kAudioUnitType_MusicEffect:
			info.category = _("Effect");
			info.n_midi_inputs = 1;
			break;
		case kAudioUnitType_Effect:
			info.category = _("Effect");
			break;
		case kAudioUnitType_Mixer:
			info.category = _("Mixer");
			break;
		case kAudioUnitType_Generator:
			info.category = _("Generator");
			break;
		default:
			info.category = _("(Unknown)");
			break;
	}
	info.id = ARDOUR::auv2_stringify_descriptor (desc);

#ifdef COREAUDIO105
	get_names (desc, info.name, info.creator);
#else
	get_names (comp, info.name, info.creator);
#endif

	CAComponent cacomp (desc);

	UInt32 version;
#ifdef COREAUDIO105
	if (cacomp.GetResourceVersion (version) != noErr)
#else
	if (cacomp.GetVersion (version) != noErr)
#endif
	{
		info.version = 0;
	}

	info.version = version;
	info.max_outputs = 0;

	/// DEBUG_TRACE (DEBUG::AudioUnitConfig, "get AU channel info\n");

	CAAudioUnit unit;
	AUChannelInfo* channel_info;
	UInt32 cnt;
	int ret;

	try {
		if (CAAudioUnit::Open (cacomp, unit) != noErr) {
			return;
		}
	} catch (...) {
		warning << string_compose (_("Could not load AU plugin %1 - ignored"), info.name) << endmsg;
		return;
	}

	if ((ret = unit.GetChannelInfo (&channel_info, cnt)) < 0) {
		return;
	}

	if (ret > 0) {
		/* AU is expected to deal with same channel valance in and out */
		info.io_configs.push_back (pair<int,int> (-1, -1));
	} else {
		/* CAAudioUnit::GetChannelInfo silently merges bus formats
		 * check if this was the case and if so, add
		 * bus configs as incremental options.
		 */
		Boolean* isWritable = 0;
		UInt32   dataSize   = 0;
		OSStatus result = AudioUnitGetPropertyInfo (unit.AU(),
				kAudioUnitProperty_SupportedNumChannels,
				kAudioUnitScope_Global, 0,
				&dataSize, isWritable);
		if (result != noErr && (cacomp.Desc().IsGenerator() || cacomp.Desc().IsMusicDevice())) {
			/* incrementally add busses */
			int in = 0;
			int out = 0;
			for (uint32_t n = 0; n < cnt; ++n) {
				in += channel_info[n].inChannels;
				out += channel_info[n].outChannels;
				info.io_configs.push_back (pair<int,int> (in, out));
			}
		} else {
			/* store each configuration */
			for (uint32_t n = 0; n < cnt; ++n) {
				info.io_configs.push_back (pair<int,int> (channel_info[n].inChannels, channel_info[n].outChannels));
			}
		}

		free (channel_info);
	}

	/* here we have to map apple's wildcard system to a simple pair
	 * of values. in ::can_do() we use the whole system, but here
	 * we need a single pair of values. XXX probably means we should
	 * remove any use of these values.
	 *
	 * for now, if the plugin provides a wildcard, treat it as 1. we really
	 * don't care much, because whether we can handle an i/o configuration
	 * depends upon ::configure_variable_io(), not these counts.
	 *
	 * they exist because other parts of ardour try to present i/o configuration
	 * info to the user, which should perhaps be revisited.
	 */

	const vector<pair<int,int> >& ioc (info.io_configs);
	for (vector<pair<int,int> >::const_iterator i = ioc.begin(); i != ioc.end(); ++i) {
		int32_t possible_out = i->second;
		if (possible_out < 0) {
			continue;
		} else if (possible_out > info.max_outputs) {
			info.max_outputs = possible_out;
		}
	}

	int32_t possible_in = ioc.front().first;
	int32_t possible_out = ioc.front().second;

	if (possible_in > 0) {
		info.n_inputs = possible_in;
	} else {
		info.n_inputs = 1;
	}

	if (possible_out > 0) {
		info.n_outputs = possible_out;
	} else {
		info.n_outputs = 1;
	}

	//DEBUG_TRACE (DEBUG::AudioUnitConfig, string_compose ("detected AU %1 with %2 i/o configurations - %3\n", info->name.c_str(), info->cache.io_configs.size(), info->unique_id));

	rv.push_back (info);
}

/* ****************************************************************************/

static bool
discover_auv2 (CAComponentDescription& desc, std::vector<ARDOUR::AUv2Info>& rv, bool verbose)
{
	ArdourComponent comp = ArdourFindNext (NULL, &desc);

	if (comp == NULL) {
		error << ("AU was not found.") << endmsg;
		return false;
	}

	while (comp != NULL) {
		CAComponentDescription temp;
#ifdef COREAUDIO105
		GetComponentInfo (comp, &temp, NULL, NULL, NULL);
#else
		AudioComponentGetDescription (comp, &temp);
#endif
		info << ("Component loaded") << endmsg;

		assert (temp.componentType == desc.componentType);
		assert (temp.componentSubType == desc.componentSubType);
		assert (temp.componentManufacturer == desc.componentManufacturer);

		auv2_plugin_info (comp, desc, rv, verbose);

		comp = ArdourFindNext (comp, &desc);
		assert (comp == NULL);
	}

	return true;
}

static string
auv2_info_cache_dir ()
{
	string dir = Glib::build_filename (ARDOUR::user_cache_directory (), "auv2");
	/* if the directory doesn't exist, try to create it */
	if (!Glib::file_test (dir, Glib::FILE_TEST_IS_DIR)) {
		if (g_mkdir (dir.c_str (), 0700)) {
			PBD::fatal << "Cannot create AudioUnit cache folder '" << dir << "'" << endmsg;
		}
	}
	return dir;
}

#include "sha1.c"

string
ARDOUR::auv2_cache_file (CAComponentDescription const& desc)
{
	std::string id = auv2_stringify_descriptor (desc);
	char hash[41];
	Sha1Digest s;
	sha1_init (&s);
	sha1_write (&s, (const uint8_t *) id.c_str(), id.size());
	sha1_result_hash (&s, hash);
	return Glib::build_filename (auv2_info_cache_dir (), std::string (hash) + std::string (".a2i"));
}

string
ARDOUR::auv2_valid_cache_file (CAComponentDescription const& desc, bool verbose, bool* is_new)
{
	string const cache_file = ARDOUR::auv2_cache_file (desc);
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
		info << "Found cache file: '" << cache_file <<"'" << endmsg;
	}

	// TODO check version

	return cache_file;
}

static bool
auv2_save_cache_file (CAComponentDescription& desc, XMLNode* root, bool verbose)
{
	string const cache_file = ARDOUR::auv2_cache_file (desc);

	XMLTree tree;
	tree.set_root (root);
	if (!tree.write (cache_file)) {
		error << "Could not save AUv2 plugin cache to: " << cache_file << endmsg;
		return false;
	}
	if (verbose) {
		root->dump (std::cout, "\t");
	}
	return true;
}

bool
ARDOUR::auv2_scan_and_cache (CAComponentDescription& desc, boost::function<void (CAComponentDescription const&, AUv2Info const&)> cb, bool verbose)
{
	XMLNode* root = new XMLNode ("AUv2Cache");
	root->set_property ("version", 2);

	try {
		std::vector<AUv2Info> nfo;
		if (!discover_auv2 (desc, nfo, verbose)) {
			delete root;
			return false;
		}
		if (nfo.empty ()) {
			cerr << "No plugins matching ID: '" << auv2_stringify_descriptor (desc) << "'\n";
			delete root;
			return false;
		}
		for (std::vector<AUv2Info>::const_iterator i = nfo.begin(); i != nfo.end(); ++i) {
			cb (desc, *i);
			root->add_child_nocopy (i->state ());
		}
	} catch (...) {
		cerr << "Cannot load AudioUnit plugin: '" << auv2_stringify_descriptor (desc) << "'\n";
		delete root;
		return false;
	}

	return auv2_save_cache_file (desc, root, verbose);
}

/* ****************************************************************************/
static void
index_components (std::vector<ARDOUR::AUv2DescStr>& rv, CAComponentDescription &desc)
{
	ArdourComponent comp = 0;
	do {
		CAComponentDescription temp;
		comp = ArdourFindNext (comp, &desc);

		if (!comp) {
			break;
		}

#ifdef COREAUDIO105
		GetComponentInfo (comp, &temp, NULL, NULL, NULL);
#else
		AudioComponentGetDescription (comp, &temp);
#endif

		CAComponent cacomp (desc);
		UInt32 version;
#ifdef COREAUDIO105
		if (cacomp.GetResourceVersion (version) != noErr)
#else
		if (cacomp.GetVersion (version) != noErr)
#endif
		{
			version = 0;
			//continue;
		}

		switch (temp.Type()) {
			case kAudioUnitType_Panner:
			case kAudioUnitType_OfflineEffect:
			case kAudioUnitType_FormatConverter:
				continue;
			default:
				break;
		}

		CFStringRef s_type = UTCreateStringForOSType (temp.componentType);
		CFStringRef s_subt = UTCreateStringForOSType (temp.componentSubType);
		CFStringRef s_manu = UTCreateStringForOSType (temp.componentManufacturer);

		if (s_type && s_subt && s_manu) {
			ARDOUR::AUv2DescStr foo;
			char tmp[5];
			CFStringGetCString (s_type, tmp, 5, kCFStringEncodingUTF8);
			foo.type = tmp;
			CFStringGetCString (s_subt, tmp, 5, kCFStringEncodingUTF8);
			foo.subt = tmp;
			CFStringGetCString (s_manu, tmp, 5, kCFStringEncodingUTF8);
			foo.manu = tmp;
			// TODO add version
			rv.push_back (foo);
		}

		CFRelease (s_type);
		CFRelease (s_subt);
		CFRelease (s_manu);
	}
	while (true);
}

void
ARDOUR::auv2_list_plugins (std::vector<AUv2DescStr>& rv)
{
	CAComponentDescription desc;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	desc.componentSubType = 0;
	desc.componentManufacturer = 0;

	desc.componentType = kAudioUnitType_Effect;
	index_components (rv, desc);

	desc.componentType = kAudioUnitType_MusicEffect;
	index_components (rv, desc);

	desc.componentType = kAudioUnitType_Generator;
	index_components (rv, desc);

	desc.componentType = kAudioUnitType_MusicDevice;
	index_components (rv, desc);
}

/* ****************************************************************************/

using namespace ARDOUR;

AUv2Info::AUv2Info (XMLNode const& node)
	: version (0)
	, n_inputs (0)
	, n_outputs (0)
	, n_midi_inputs (0)
	, n_midi_outputs (0)
	, max_outputs (0)
{
	bool err = false;

	if (node.name() != "AUv2Info") {
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
	err |= !node.get_property ("max_outputs", max_outputs);

	const XMLNodeList children = node.children();
	for (XMLNodeConstIterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() != X_("io_config")) {
			continue;
		}
		int32_t in;
		int32_t out;
		if ((*i)->get_property (X_("in"), in) && (*i)->get_property (X_("out"), out)) {
			io_configs.push_back (std::pair<int,int> (in, out));
		}
	}

	if (err) {
		throw failed_constructor ();
	}
}

XMLNode&
AUv2Info::state () const
{
	XMLNode* node = new XMLNode("AUv2Info");
	node->set_property ("id",       id);
	node->set_property ("name",     name);
	node->set_property ("creator",  creator);
	node->set_property ("category", category);
	node->set_property ("version",  version);

	node->set_property ("n_inputs",       n_inputs);
	node->set_property ("n_outputs",      n_outputs);
	node->set_property ("n_midi_inputs",  n_midi_inputs);
	node->set_property ("n_midi_outputs", n_midi_outputs);
	node->set_property ("max_outputs",    max_outputs);

	for (vector<pair<int, int> >::const_iterator i = io_configs.begin(); i != io_configs.end(); ++i) {
		XMLNode* child = new XMLNode (X_("io_config"));
		child->set_property (X_("in"), i->first);
		child->set_property (X_("out"), i->second);
		node->add_child_nocopy (*child);
	}
	return *node;
}
