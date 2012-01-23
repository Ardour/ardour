#include <iostream>
#include "midi++/manager.h"
#include "pbd/textreceiver.h"
#include "pbd/compose.h"
#include "pbd/enumwriter.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

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

int
main ()
{
	string const test_session_path = "../libs/ardour/test/profiling/sessions/0tracks";
	string const test_session_snapshot = "0tracks.ardour";
	
	init (false, true);
	SessionEvent::create_per_thread_pool ("test", 512);

	test_receiver.listen_to (error);
	test_receiver.listen_to (info);
	test_receiver.listen_to (fatal);
	test_receiver.listen_to (warning);

	AudioEngine* engine = new AudioEngine ("test", "");
	engine->start ();
	MIDI::Manager::create (engine->jack ());

	Session* session = new Session (*engine, test_session_path, test_session_snapshot);
	engine->set_session (session);

	for (int i = 0; i < 32768; ++i) {
		session->process (64);
	}

	return 0;
}
