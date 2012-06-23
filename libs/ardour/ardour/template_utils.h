
#ifndef TEMPLATE_UTILS_INCLUDED
#define TEMPLATE_UTILS_INCLUDED

#include <string>
#include <vector>

namespace ARDOUR {

	std::string system_template_directory ();
	std::string system_route_template_directory ();

	std::string user_template_directory ();
	std::string user_route_template_directory ();

	struct TemplateInfo {
		std::string name;
		std::string path;
	};

	void find_route_templates (std::vector<TemplateInfo>& template_names);
	void find_session_templates (std::vector<TemplateInfo>& template_names);

	std::string session_template_dir_to_file (std::string const &);

} // namespace ARDOUR

#endif
