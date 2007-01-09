#include <fstream>
#include <unistd.h>

#include <pbd/copyfile.h>
#include <pbd/error.h>
#include <pbd/compose.h>

#include "i18n.h"

using namespace PBD;
using namespace std;

int
PBD::copy_file (Glib::ustring from, Glib::ustring to)
{
	ifstream in (from.c_str());
	ofstream out (to.c_str());
	
	if (!in) {
		error << string_compose (_("Could not open %1 for copy"), from) << endmsg;
		return -1;
	}
	
	if (!out) {
		error << string_compose (_("Could not open %1 as copy"), to) << endmsg;
		return -1;
	}
	
	out << in.rdbuf();
	
	if (!in || !out) {
		error << string_compose (_("Could not copy existing file %1 to %2"), from, to) << endmsg;
		unlink (to.c_str());
		return -1;
	}
	
	return 0;
}
