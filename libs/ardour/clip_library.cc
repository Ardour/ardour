/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/string_convert.h"
#include "pbd/xml++.h"

#include "ardour/clip_library.h"
#include "ardour/rc_configuration.h"
#include "ardour/region.h"
#include "ardour/types.h"
#include "ardour/utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

PBD::Signal2<void, std::string, void*> ARDOUR::LibraryClipAdded;

string
ARDOUR::clip_library_dir (bool create_if_missing)
{
	std::string p = Config->get_clip_library_dir ();

	if (p == X_("@default@")) {
		const char* c = 0;
		if ((c = getenv ("XDG_DATA_HOME")) != 0) {
			p = c;
			p = Glib::build_filename (p, "sounds", "clips");
		} else {
#ifdef __APPLE__
			/* Logic Saves "loops" to  '~Library/Audio/Apple Loops/Apple/'
			 * and "samples" to '~/Library/Application Support/Logic/XYZ/'
			 * By default the following folders also exist
			 *  '~/Library/Audio/Sounds/Alerts/'
			 *  '~/Library/Audio/Sounds/Banks/'
			 */
			p = Glib::build_filename (Glib::get_home_dir (), "Library/Audio/Sounds/Clips");
#elif defined PLATFORM_WINDOWS
			/* %localappdata%\ClipLibrary */
			p = Glib::build_filename (Glib::get_user_data_dir (), "Clip Library");
#else
			/* Linux, *BSD: use XDG_DATA_HOME prefix, version-independent app folder */
			p = Glib::build_filename (Glib::get_user_data_dir (), ".local", "share", "sounds", "clips");
#endif
		}

		info << string_compose (_("Set Clip Library directory to '%1'"), p) << endmsg;
		Config->set_clip_library_dir (p);
	}

	if (!Glib::file_test (p, Glib::FILE_TEST_EXISTS)) {
		if (!create_if_missing || p.empty ()) {
			return "";
		}
		if (g_mkdir_with_parents (p.c_str (), 0755)) {
			error << string_compose (_("Cannot create Clip Library directory '%1'"), p) << endmsg;
			return "";
		}
		XMLNode* root = new XMLNode (X_("DAWDirectory"));
		XMLNode* node = root->add_child (X_("title"));
		//node->set_property (X_("lang"), "en-us");
		node->add_content (_("Clip Library"));

		XMLTree tree;
		tree.set_root (root);
		if (!tree.write (Glib::build_filename (p, ".daw-meta.xml"))) {
			error << string_compose (_("Could not save Clip Libary meta-data in '%1'"), p) << endmsg;
		}

	} else if (!Glib::file_test (p, Glib::FILE_TEST_IS_DIR)) {
		error << string_compose (_("Clip Library directory '%1' already exists and is not a directory/folder"), p) << endmsg;
		return "";
	}

	return p;
}

bool
ARDOUR::export_to_clip_library (boost::shared_ptr<Region> r, void* src)
{
	std::string const lib = clip_library_dir (true);
	if (lib.empty () || !r) {
		return false;
	}

	std::string region_name = r->name ();
	std::string path        = Glib::build_filename (lib, region_name + native_header_format_extension (FLAC, r->data_type ()));

	while (Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		region_name = bump_name_once (region_name, '.');
		path        = Glib::build_filename (lib, region_name + native_header_format_extension (FLAC, r->data_type ()));
	}

	if (r->do_export (path)) {
		LibraryClipAdded (path, src); /* EMIT SIGNAL */
		return true;
	}
	return false;
}
