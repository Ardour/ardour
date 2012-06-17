#include <fstream>
#include <sstream>
#include "pbd/xml++.h"
#include "pbd/textreceiver.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include <cppunit/extensions/HelperMacros.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

void
check_xml (XMLNode* node, string ref_file)
{
	system ("rm -f libs/ardour/test/test.xml");
	ofstream f ("libs/ardour/test/test.xml");
	node->dump (f);
	f.close ();

	stringstream cmd;
	cmd << "diff -u libs/ardour/test/test.xml " << ref_file;
	CPPUNIT_ASSERT_EQUAL (0, system (cmd.str().c_str ()));
}

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

Session *
load_session (string dir, string state)
{
	SessionEvent::create_per_thread_pool ("test", 512);

	test_receiver.listen_to (error);
	test_receiver.listen_to (info);
	test_receiver.listen_to (fatal);
	test_receiver.listen_to (warning);

	/* We can't use VSTs here as we have a stub instead of the
	   required bits in gtk2_ardour.
	*/
	Config->set_use_lxvst (false);

	AudioEngine* engine = new AudioEngine ("test", "");
	init_post_engine ();

	CPPUNIT_ASSERT (engine->start () == 0);

	Session* session = new Session (*engine, dir, state);
	engine->set_session (session);
	return session;
}
