#include <iostream>
#include "pbd/textreceiver.h"
#include "pbd/compose.h"
#include "pbd/enumwriter.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "test_ui.h"
#include "test_util.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

static const char* localedir = LOCALEDIR;

int
main (int argc, char* argv[])
{
	if (argc < 2) {
		cerr << argv[0] << ": <session>\n";
		exit (EXIT_FAILURE);
	}

	ARDOUR::init (true, localedir);
	TestUI* test_ui = new TestUI();
	create_and_start_dummy_backend ();

	Session* session = load_session (
		string_compose ("../libs/ardour/test/profiling/sessions/%1", argv[1]),
		string_compose ("%1.ardour", argv[1])
		);

	cout << "INFO: " << session->get_routes()->size() << " routes.\n";

	{
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance ()->process_lock ());
		for (int i = 0; i < 32768; ++i) {
			session->process (session->engine().samples_per_cycle ());
		}
	}

	delete session;
	stop_and_destroy_backend ();
	delete test_ui;
	ARDOUR::cleanup ();
	return 0;
}
