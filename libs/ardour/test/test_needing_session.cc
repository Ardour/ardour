#include "midi++/manager.h"
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
	string const test_session_path = "libs/ardour/test/test_session";
	system (string_compose ("rm -rf %1", test_session_path).c_str());
	_session = load_session (test_session_path, "test_session");
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
