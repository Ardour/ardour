#include "midi++/manager.h"
#include "pbd/textreceiver.h"
#include "pbd/compose.h"
#include "pbd/enumwriter.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "test_needing_session.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

class TestReceiver : public Receiver 
{
protected:
	void receive (Transmitter::Channel chn, const char * str) {
		const char *prefix = "";
		
		switch (chn) {
		case Transmitter::Error:
			prefix = ": [ERROR]: ";
			break;
		case Transmitter::Info:
			/* ignore */
			return;
		case Transmitter::Warning:
			prefix = ": [WARNING]: ";
			break;
		case Transmitter::Fatal:
			prefix = ": [FATAL]: ";
			break;
		case Transmitter::Throw:
			/* this isn't supposed to happen */
			abort ();
		}
		
		/* note: iostreams are already thread-safe: no external
		   lock required.
		*/
		
		cout << prefix << str << endl;
		
		if (chn == Transmitter::Fatal) {
			exit (9);
		}
	}
};

TestReceiver test_receiver;

void
TestNeedingSession::setUp ()
{
	string const test_session_path = "libs/ardour/test/test_session";
	system (string_compose ("rm -rf %1", test_session_path).c_str());
	
	SessionEvent::create_per_thread_pool ("test", 512);

	test_receiver.listen_to (error);
	test_receiver.listen_to (info);
	test_receiver.listen_to (fatal);
	test_receiver.listen_to (warning);

	AudioEngine* engine = new AudioEngine ("test", "");
	init_post_engine ();

	CPPUNIT_ASSERT (engine->start () == 0);

	_session = new Session (*engine, test_session_path, "test_session");
	engine->set_session (_session);
}

void
TestNeedingSession::tearDown ()
{
	AudioEngine::instance()->remove_session ();
	AudioEngine::instance()->stop (true);
	
	delete _session;

	MIDI::Manager::destroy ();
	AudioEngine::destroy ();
}
