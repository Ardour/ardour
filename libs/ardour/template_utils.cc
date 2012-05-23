#include <algorithm>
#include <cstring>

#include <glibmm.h>

#include "pbd/filesystem.h"
#include "pbd/basename.h"
#include "pbd/pathscanner.h"
#include "pbd/xml++.h"

#include "ardour/template_utils.h"
#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"
#include "ardour/filename_extensions.h"
#include "ardour/io.h"

using namespace std;
using namespace PBD;

namespace ARDOUR {

SearchPath
template_search_path ()
{
	SearchPath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths(templates_dir_name);
	return spath;
}

SearchPath
route_template_search_path ()
{
	SearchPath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths(route_templates_dir_name);
	return spath;
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
template_filter (const string &str, void */*arg*/)
{
	if (!Glib::file_test (str, Glib::FILE_TEST_IS_DIR)) {
		return false;
	}
	
	return true;
}

static bool
route_template_filter (const string &str, void */*arg*/)
{
	if (str.find (template_suffix) == str.length() - strlen (template_suffix)) {
		return true;
	}
	
	return false;
}

string
session_template_dir_to_file (string const & dir)
{
	sys::path dir_path = dir;
	sys::path file_path = dir;
	file_path /= dir_path.leaf() + template_suffix;
	return file_path.to_string ();
}


void
find_session_templates (vector<TemplateInfo>& template_names)
{
	vector<string *> *templates;
	PathScanner scanner;
	SearchPath spath (template_search_path());

	templates = scanner (spath.to_string(), template_filter, 0, true, true);

	if (!templates) {
		cerr << "Found nothing along " << spath.to_string() << endl;
		return;
	}

	cerr << "Found " << templates->size() << " along " << spath.to_string() << endl;

	for (vector<string*>::iterator i = templates->begin(); i != templates->end(); ++i) {
		string file = session_template_dir_to_file (**i);

		XMLTree tree;

		if (!tree.read (file.c_str())) {
			continue;
		}

		TemplateInfo rti;

		rti.name = basename_nosuffix (**i);
		rti.path = **i;

		template_names.push_back (rti);
	}

	delete templates;
}

void
find_route_templates (vector<TemplateInfo>& template_names)
{
	vector<string *> *templates;
	PathScanner scanner;
	SearchPath spath (route_template_search_path());

	templates = scanner (spath.to_string(), route_template_filter, 0, false, true);

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

	delete templates;
}

}
