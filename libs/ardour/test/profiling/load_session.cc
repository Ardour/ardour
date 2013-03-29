#include "test_util.h"
#include "pbd/failed_constructor.h"
#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "midi++/manager.h"
#include <iostream>
#include <cstdlib>

using namespace std;
using namespace ARDOUR;

static const char* localedir = LOCALEDIR;

int main (int argc, char* argv[])
{
	if (argc != 3) {
		cerr << "Syntax: " << argv[0] << " <dir> <snapshot-name>\n";
		exit (EXIT_FAILURE);
	}

	ARDOUR::init (false, true, localedir);

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

	AudioEngine::instance()->remove_session ();
	delete s;
	AudioEngine::instance()->stop (true);

	MIDI::Manager::destroy ();
	AudioEngine::destroy ();

	return 0;
}
