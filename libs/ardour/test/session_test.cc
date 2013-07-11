
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include <stdexcept>
#include "midi++/manager.h"
#include "pbd/textreceiver.h"
#include "pbd/file_utils.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/smf_source.h"
#include "ardour/midi_model.h"

#include "test_common.h"

#include "session_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (SessionTest);

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static TextReceiver text_receiver ("test");

void
SessionTest::setUp ()
{
	SessionEvent::create_per_thread_pool ("session_test", 512);

	text_receiver.listen_to (error);
	text_receiver.listen_to (info);
	text_receiver.listen_to (fatal);
	text_receiver.listen_to (warning);

	// this is not a good singleton constructor pattern
	AudioEngine* engine = 0;

	try {
		engine = new AudioEngine ("session_test", "");
	} catch (const AudioEngine::NoBackendAvailable& engine_exception) {
		cerr << engine_exception.what ();
	}

	CPPUNIT_ASSERT (engine);

	init_post_engine ();

	CPPUNIT_ASSERT (engine->start () == 0);
}

void
SessionTest::tearDown ()
{
	// this is needed or there is a crash in MIDI::Manager::destroy
	AudioEngine::instance()->stop (true);

	MIDI::Manager::destroy ();
	AudioEngine::destroy ();
}

void
SessionTest::new_session ()
{
	const string session_name("test_session");
	std::string new_session_dir = Glib::build_filename (new_test_output_dir(), session_name);

	CPPUNIT_ASSERT (!Glib::file_test (new_session_dir, Glib::FILE_TEST_EXISTS));

	Session* new_session = 0;

	new_session = new Session (*AudioEngine::instance (), new_session_dir, session_name);

	CPPUNIT_ASSERT (new_session);

	// shouldn't need to do this as it is done in Session constructor
	// via Session::when_engine_running
	//AudioEngine::instance->set_session (new_session);

	new_session->save_state ("");

	delete new_session;
}

void
SessionTest::new_session_from_template ()
{
	const string session_name("two_tracks");
	const string session_template_dir_name("2 Track-template");

	std::string new_session_dir = Glib::build_filename (new_test_output_dir(), session_name);

	CPPUNIT_ASSERT (!Glib::file_test (new_session_dir, Glib::FILE_TEST_EXISTS));

	std::string session_template_dir = test_search_path ().front ();
	session_template_dir = Glib::build_filename (session_template_dir, "2 Track-template");

	CPPUNIT_ASSERT (Glib::file_test (session_template_dir, Glib::FILE_TEST_IS_DIR));

	Session* new_session = 0;
	BusProfile* bus_profile = 0;

	// create a new session based on session template
	new_session = new Session (*AudioEngine::instance (), new_session_dir, session_name,
				bus_profile, session_template_dir);

	CPPUNIT_ASSERT (new_session);

	new_session->save_state ("");

	delete new_session;

	Session* template_session = 0;

	// reopen same session to check that it opens without error
	template_session = new Session (*AudioEngine::instance (), new_session_dir, session_name);

	CPPUNIT_ASSERT (template_session);

	delete template_session;
}
