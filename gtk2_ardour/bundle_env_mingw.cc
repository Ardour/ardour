/*
    Copyright (C) 2001-2012 Paul Davis

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

#include "bundle_env.h"
#include "i18n.h"

#include <fontconfig/fontconfig.h>

#include "ardour/ardour.h"
#include "ardour/search_paths.h"
#include "ardour/filesystem_paths.h"

#include "pbd/file_utils.h"
#include "pbd/epa.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;


void
fixup_bundle_environment (int, char* [], const char** localedir)
{
	/* what to do ? */
}

void load_custom_fonts() 
{
	std::string ardour_mono_file;

	if (!find_file (ardour_data_search_path(), "ArdourMono.ttf", ardour_mono_file)) {
		cerr << _("Cannot find ArdourMono TrueType font") << endl;
	}

	FcConfig *config = FcInitLoadConfigAndFonts();
	FcBool ret = FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(ardour_mono_file.c_str()));

	if (ret == FcFalse) {
		cerr << _("Cannot load ArdourMono TrueType font.") << endl;
	}

	ret = FcConfigSetCurrent(config);

	if (ret == FcFalse) {
		cerr << _("Failed to set fontconfig configuration.") << endl;
	}
}
