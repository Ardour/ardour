#include <iostream>
#include "midi++/manager.h"
#include "pbd/textreceiver.h"
#include "pbd/compose.h"
#include "pbd/enumwriter.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
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

	ARDOUR::init (false, true, localedir);

	Session* session = load_session (
		string_compose ("../libs/ardour/test/profiling/sessions/%1", argv[1]),
		string_compose ("%1.ardour", argv[1])
		);

	cout << "INFO: " << session->get_routes()->size() << " routes.\n";

	for (int i = 0; i < 32768; ++i) {
		session->process (session->engine().frames_per_cycle ());
	}

	return 0;
}
