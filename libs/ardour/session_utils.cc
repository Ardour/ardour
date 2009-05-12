
#include "pbd/error.h"

#include "ardour/session_directory.h"

#include "i18n.h"

namespace ARDOUR {

using namespace std;	
using namespace PBD;

bool
create_session_directory (const string& session_directory_path)
{
	SessionDirectory sdir(session_directory_path);

	try
	{
		// create all the required session directories
		sdir.create();
	}
	catch(sys::filesystem_error& ex)
	{
		// log the exception
		warning << string_compose
			(
			 _("Unable to create session directory at path %1 : %2"),
			 session_directory_path,
			 ex.what()
			);

		return false;
	}

	// successfully created the session directory
	return true;
}

} // namespace ARDOUR
