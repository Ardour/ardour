#include <glibmm/miscutils.h>

#include "midi++/manager.h"
#include "pbd/compose.h"
#include "pbd/enumwriter.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "test_needing_session.h"
#include "test_util.h"
#include "test_common.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

void
TestNeedingSession::setUp ()
{
	const string session_name("test_session");
	std::string new_session_dir = Glib::build_filename (new_test_output_dir(), session_name);
	_session = load_session (new_session_dir, "test_session");
}

void
TestNeedingSession::tearDown ()
{
	AudioEngine::instance()->remove_session ();
	delete _session;
	AudioEngine::instance()->stop (true);
	
	MIDI::Manager::destroy ();
	AudioEngine::destroy ();
}
