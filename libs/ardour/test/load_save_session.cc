#include "test_util.h"

#include <iostream>
#include <cstdlib>

#include <glib.h>

#include "pbd/failed_constructor.h"
#include "pbd/timing.h"

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/session.h"

#include "test_ui.h"

using namespace std;
using namespace ARDOUR;

static const char* localedir = LOCALEDIR;

static const int sleep_seconds = 2;

static
void
pause_for_effect()
{
	// It may be useful to pause to make it easier to see what is happening in a
	// visual tool like massif visualizer

	std::cerr << "pausing for " << sleep_seconds << " seconds" << std::endl;

	g_usleep(sleep_seconds*1000000);
}

int main (int argc, char* argv[])
{
	if (argc != 3) {
		cerr << "Syntax: " << argv[0] << " <dir> <snapshot-name>\n";
		exit (EXIT_FAILURE);
	}

	std::cerr << "ARDOUR::init" << std::endl;

	PBD::Timing ardour_init_timing;

	ARDOUR::init (true, localedir);
	ardour_init_timing.update();

	TestUI* test_ui = new TestUI();

	std::cerr << "ARDOUR::init time : " << ardour_init_timing.elapsed()
	          << " usecs" << std::endl;

	std::cerr << "Creating Dummy backend" << std::endl;

	create_and_start_dummy_backend ();

	std::cerr << "Loading session: " << argv[2] << std::endl;

	PBD::Timing load_session_timing;

	Session* s = 0;

	try {
		s = load_session (argv[1], argv[2]);
	} catch (failed_constructor& e) {
		cerr << "failed_constructor: " << e.what() << "\n";
		exit (EXIT_FAILURE);
	} catch (AudioEngine::PortRegistrationFailure& e) {
		cerr << "PortRegistrationFailure: " << e.what() << "\n";
		exit (EXIT_FAILURE);
	} catch (exception& e) {
		cerr << "exception: " << e.what() << "\n";
		exit (EXIT_FAILURE);
	} catch (...) {
		cerr << "unknown exception.\n";
		exit (EXIT_FAILURE);
	}

	load_session_timing.update();

	std::cerr << "Loading session time : " << load_session_timing.elapsed()
	          << " usecs" << std::endl;

	PBD::Timing save_session_timing;

	pause_for_effect ();

	std::cerr << "Saving session: " << argv[2] << std::endl;

	s->save_state("");

	save_session_timing.update();

	std::cerr << "Saving session time : " << save_session_timing.elapsed()
	          << " usecs" << std::endl;

	std::cerr << "AudioEngine::remove_session" << std::endl;

	AudioEngine::instance()->remove_session ();

	PBD::Timing destroy_session_timing;

	delete s;

	destroy_session_timing.update();

	std::cerr << "Destroy session time : " << destroy_session_timing.elapsed()
	          << " usecs" << std::endl;

	AudioEngine::instance()->stop ();

	AudioEngine::destroy ();

	delete test_ui;

	ARDOUR::cleanup ();

	return 0;
}
