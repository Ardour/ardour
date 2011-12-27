#include "midi++/manager.h"
#include "pbd/textreceiver.h"
#include "pbd/compose.h"
#include "pbd/enumwriter.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "test_needing_session.h"
#include "test_receiver.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

TestReceiver test_receiver;

void
TestNeedingSession::setUp ()
{
	string const test_session_path = "libs/ardour/test/test_session";
	system (string_compose ("rm -rf %1", test_session_path).c_str());
	
	init (false, true);
	SessionEvent::create_per_thread_pool ("test", 512);

	test_receiver.listen_to (error);
	test_receiver.listen_to (info);
	test_receiver.listen_to (fatal);
	test_receiver.listen_to (warning);

	AudioEngine* engine = new AudioEngine ("test", "");
	MIDI::Manager::create (engine->jack ());
	CPPUNIT_ASSERT (engine->start () == 0);

	_session = new Session (*engine, test_session_path, "test_session");
	engine->set_session (_session);
}

void
TestNeedingSession::tearDown ()
{
	AudioEngine::instance()->remove_session ();
	
	delete _session;

	EnumWriter::destroy ();
	MIDI::Manager::destroy ();
	AudioEngine::destroy ();
}
