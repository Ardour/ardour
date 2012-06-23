
#include "pbd/error.h"

#include <stdint.h>
#include "ardour/session_directory.h"

#include "i18n.h"

namespace ARDOUR {

using namespace std;
using namespace PBD;

bool
create_session_directory (const string& session_directory_path)
{
	SessionDirectory sdir(session_directory_path);
	sdir.create();
	return true;
}

} // namespace ARDOUR
