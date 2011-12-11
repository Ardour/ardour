
#ifndef TEMPLATE_UTILS_INCLUDED
#define TEMPLATE_UTILS_INCLUDED

#include <vector>

#include "pbd/filesystem.h"

namespace ARDOUR {

	PBD::sys::path system_template_directory ();
	PBD::sys::path system_route_template_directory ();

	PBD::sys::path user_template_directory ();
	PBD::sys::path user_route_template_directory ();

	struct TemplateInfo {
		std::string name;
		std::string path;
	};

	void find_route_templates (std::vector<TemplateInfo>& template_names);
	void find_session_templates (std::vector<TemplateInfo>& template_names);

	std::string session_template_dir_to_file (std::string const &);

} // namespace ARDOUR

#endif
