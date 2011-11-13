#include <stdexcept>
#include "midi++/manager.h"
#include "pbd/textreceiver.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/smf_source.h"
#include "ardour/midi_model.h"
#include "test/mantis_3356.h"

CPPUNIT_TEST_SUITE_REGISTRATION (Mantis3356Test);

using namespace std;
using namespace ARDOUR;
using namespace PBD;

TextReceiver text_receiver ("test");

void
Mantis3356Test::test ()
{
	init (false, true);
	SessionEvent::create_per_thread_pool ("test", 512);

	text_receiver.listen_to (error);
	text_receiver.listen_to (info);
	text_receiver.listen_to (fatal);
	text_receiver.listen_to (warning);

	AudioEngine engine ("test", "");
	MIDI::Manager::create (engine.jack ());
	CPPUNIT_ASSERT (engine.start () == 0);

	Session session (engine, "../libs/ardour/test/data/mantis_3356", "mantis_3356");
	engine.set_session (&session);

	Session::SourceMap sources = session.get_sources ();

	boost::shared_ptr<SMFSource> source = boost::dynamic_pointer_cast<SMFSource> (sources[ID ("87")]);
	CPPUNIT_ASSERT (source);

	boost::shared_ptr<MidiModel> model = source->model ();
	CPPUNIT_ASSERT (model);

	stringstream result;

	for (MidiModel::const_iterator i = model->begin(); i != model->end(); ++i) {
		result << *i << "\n";
	}

	ifstream ref ("../libs/ardour/test/data/mantis_3356.ref");

	while (1) {
		string a;
		string b;

		getline (ref, a);
		getline (result, b);

		CPPUNIT_ASSERT (a == b);

		if (result.eof() && ref.eof()) {
			break;
		}

		CPPUNIT_ASSERT (!result.eof ());
		CPPUNIT_ASSERT (!ref.eof ());
	}

}
