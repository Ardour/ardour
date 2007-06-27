
#include <pbd/filesystem.h>

#include <ardour/template_utils.h>
#include <ardour/ardour.h>
#include <ardour/directory_names.h>
#include <ardour/filesystem_paths.h>

namespace ARDOUR {

sys::path
system_template_directory ()
{
	sys::path p(get_system_data_path());
	p /= templates_dir_name;

	return p;
}

sys::path
user_template_directory ()
{
	sys::path p(user_config_directory());
	p /= templates_dir_name;

	return p;
}

} // namespace ARDOUR
