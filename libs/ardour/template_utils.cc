#include <algorithm>
#include <cstring>

#include "pbd/filesystem.h"
#include "pbd/pathscanner.h"
#include "pbd/xml++.h"

#include "ardour/template_utils.h"
#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"
#include "ardour/filename_extensions.h"
#include "ardour/io.h"

namespace ARDOUR {

sys::path
system_template_directory ()
{
	SearchPath spath(system_data_search_path());
	spath.add_subdirectory_to_paths(templates_dir_name);

	// just return the first directory in the search path that exists
	SearchPath::const_iterator i = std::find_if(spath.begin(), spath.end(), sys::exists);

	if (i == spath.end()) return sys::path();

	return *i;
}

sys::path
system_route_template_directory ()
{
	SearchPath spath(system_data_search_path());
	spath.add_subdirectory_to_paths(route_templates_dir_name);

	// just return the first directory in the search path that exists
	SearchPath::const_iterator i = std::find_if(spath.begin(), spath.end(), sys::exists);

	if (i == spath.end()) return sys::path();

	return *i;
}

sys::path
user_template_directory ()
{
	sys::path p(user_config_directory());
	p /= templates_dir_name;

	return p;
}

sys::path
user_route_template_directory ()
{
	sys::path p(user_config_directory());
	p /= route_templates_dir_name;

	return p;
}

static bool
template_filter (const string &str, void *arg)
{
	return (str.length() > strlen(temp_suffix) &&
		str.find (temp_suffix) == (str.length() - strlen (temp_suffix)));
}

void
find_route_templates (vector<RouteTemplateInfo>& template_names)
{
	vector<string *> *templates;
	PathScanner scanner;
	SearchPath spath (system_data_search_path());

	spath += user_config_directory();
	spath.add_subdirectory_to_paths(route_templates_dir_name);

	templates = scanner (spath.to_string(), template_filter, 0, false, true);
	
	if (!templates) {
		return;
	}
	
	for (vector<string*>::iterator i = templates->begin(); i != templates->end(); ++i) {
		string fullpath = *(*i);

		XMLTree tree;

		if (!tree.read (fullpath.c_str())) {
			continue;
		}

		XMLNode* root = tree.root();
		
		RouteTemplateInfo rti;

		rti.name = IO::name_from_state (*root->children().front());
		rti.path = fullpath;

		template_names.push_back (rti);
	}

	free (templates);
}

} // namespace ARDOUR
