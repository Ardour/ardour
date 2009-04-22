#include <algorithm>
#include <cstring>

#include "pbd/filesystem.h"
#include "pbd/basename.h"
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
	cerr << "Checking into " << str << " using " << template_suffix << endl;
	return (str.length() > strlen(template_suffix) &&
		str.find (template_suffix) == (str.length() - strlen (template_suffix)));
}

void
find_session_templates (vector<TemplateInfo>& template_names)
{
	vector<string *> *templates;
	PathScanner scanner;
	SearchPath spath (system_template_directory());
	spath += user_template_directory ();

	templates = scanner (spath.to_string(), template_filter, 0, false, true);
	
	if (!templates) {
		cerr << "Found nothing along " << spath.to_string() << endl;
		return;
	}

	cerr << "Found " << templates->size() << " along " << spath.to_string() << endl;
	
	for (vector<string*>::iterator i = templates->begin(); i != templates->end(); ++i) {
		string fullpath = *(*i);

		XMLTree tree;

		if (!tree.read (fullpath.c_str())) {
			continue;
		}

		XMLNode* root = tree.root();
		
		TemplateInfo rti;

		rti.name = basename_nosuffix (fullpath);
		rti.path = fullpath;

		template_names.push_back (rti);
	}

	free (templates);
}

void
find_route_templates (vector<TemplateInfo>& template_names)
{
	vector<string *> *templates;
	PathScanner scanner;
	SearchPath spath (system_route_template_directory());
	spath += user_route_template_directory ();

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
		
		TemplateInfo rti;

		rti.name = IO::name_from_state (*root->children().front());
		rti.path = fullpath;

		template_names.push_back (rti);
	}

	free (templates);
}

}
