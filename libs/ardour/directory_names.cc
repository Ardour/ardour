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
