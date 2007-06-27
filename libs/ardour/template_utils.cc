#include <algorithm>

#include <pbd/filesystem.h>
#include <pbd/error.h>

#include <ardour/template_utils.h>
#include <ardour/directory_names.h>
#include <ardour/filesystem_paths.h>

namespace ARDOUR {

sys::path
system_template_directory ()
{
	SearchPath spath(system_data_search_path());
	spath.add_subdirectory_to_paths(templates_dir_name);

	// just return the first directory in the search path that exists
	SearchPath::const_iterator i = std::find_if(spath.begin(), spath.end(), sys::exists);

	if (i == spath.end())
	{
		warning << "System template directory does not exist" << endmsg;
		return sys::path("");
	}

	return *i;
}

sys::path
user_template_directory ()
{
	sys::path p(user_config_directory());
	p /= templates_dir_name;

	return p;
}

} // namespace ARDOUR
