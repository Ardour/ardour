
#ifndef TEMPLATE_UTILS_INCLUDED
#define TEMPLATE_UTILS_INCLUDED

#include <vector>

#include <pbd/filesystem.h>

namespace ARDOUR {

	using std::vector;
	using namespace PBD;

	sys::path system_template_directory ();

	sys::path user_template_directory ();

} // namespace ARDOUR

#endif
