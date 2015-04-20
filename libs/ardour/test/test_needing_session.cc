#include <glibmm/miscutils.h>

#include "pbd/compose.h"
#include "pbd/enumwriter.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "test_needing_session.h"
#include "test_util.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

void
TestNeedingSession::setUp ()
{
	const string session_name("test_session");
	std::string new_session_dir = Glib::build_filename (new_test_output_dir(), session_name);
	create_and_start_dummy_backend ();
	_session = load_session (new_session_dir, "test_session");
}

void
TestNeedingSession::tearDown ()
{
	delete _session;
	stop_and_destroy_backend ();
	_session = 0;
}
