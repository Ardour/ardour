
#ifndef TEMPLATE_UTILS_INCLUDED
#define TEMPLATE_UTILS_INCLUDED

#include <vector>

#include "pbd/filesystem.h"

namespace ARDOUR {

	using std::vector;
	using namespace PBD;

	sys::path system_template_directory ();
	sys::path system_route_template_directory ();

	sys::path user_template_directory ();
	sys::path user_route_template_directory ();

	struct RouteTemplateInfo {
	    std::string name;
	    std::string path;
	};

	void find_route_templates (std::vector<RouteTemplateInfo>& template_names);

} // namespace ARDOUR

#endif
