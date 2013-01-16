/*
    Copyright (C) 2012 Paul Davis 

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

#include "ardour/directory_names.h"

#include "i18n.h"

namespace ARDOUR {

const char* const old_sound_dir_name = X_("sounds");
const char* const sound_dir_name = X_("audiofiles");
const char* const midi_dir_name = X_("midifiles");
const char* const midi_patch_dir_name = X_("patchfiles");
const char* const peak_dir_name = X_("peaks");
const char* const dead_dir_name = X_("dead");
const char* const interchange_dir_name = X_("interchange");
const char* const export_dir_name = X_("export");
const char* const export_formats_dir_name = X_("export");
const char* const templates_dir_name = X_("templates");
const char* const route_templates_dir_name = X_("route_templates");
const char* const surfaces_dir_name = X_("surfaces");
const char* const panner_dir_name = X_("panners");

/* these should end up using variants of PROGRAM_NAME */
#ifdef __APPLE__
const char* const user_config_dir_name = X_("Ardour" "3");
#else
const char* const user_config_dir_name = X_("ardour" "3");
#endif

}
