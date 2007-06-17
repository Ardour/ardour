
#include <pbd/filesystem.h>

#include <ardour/template_utils.h>
#include <ardour/ardour.h>
#include <ardour/directory_names.h>

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
	sys::path p(get_user_ardour_path());
	p /= templates_dir_name;

	return p;
}

} // namespace ARDOUR
