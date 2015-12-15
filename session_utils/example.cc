#include <iostream>
#include <cstdlib>

#include "common.h"

using namespace std;
using namespace ARDOUR;
using namespace SessionUtils;

int main (int argc, char* argv[])
{
	SessionUtils::init();
	Session* s = 0;

	s = SessionUtils::load_session (
			"/home/rgareus/Documents/ArdourSessions/TestA/",
			"TestA"
			);

	printf ("SESSION INFO: routes: %lu\n", s->get_routes()->size ());

	sleep(2);

	//s->save_state ("");

	SessionUtils::unload_session(s);
	SessionUtils::cleanup();

	return 0;
}
