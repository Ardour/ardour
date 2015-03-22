#include <stdio.h>

#include "pbd/pbd.h"
#include "pbd/xml++.h"
#include "ardour/rc_configuration.h"
#include "pbd/enumwriter.h"

using namespace ARDOUR;
using namespace std;

int main (int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage %s <file-name>\n", argv[0]);
		return -1;
	}

	setenv ("ARDOUR_DLL_PATH", "/xxx", 1);
	setenv ("ARDOUR_CONFIG_PATH", "/xxx", 1);

	if (!ARDOUR::init (false, true, "/xxx")) {
		fprintf(stderr, "Failed to initialize libardour\n");
		return -1;
	}

	RCConfiguration * rc = new RCConfiguration;
	XMLNode& state = rc->get_state();

	// TODO strip some nodes here ?

	XMLTree tree;
	tree.set_root (&state);

	if (!tree.write (argv[1])) {
		fprintf(stderr, "Error saving config file '%s'\n", argv[1]);
		return -1;
	}

	return 0;
}
