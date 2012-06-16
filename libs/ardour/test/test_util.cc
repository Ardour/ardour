#include <fstream>
#include <sstream>
#include "pbd/xml++.h"
#include <cppunit/extensions/HelperMacros.h>

using namespace std;

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
	
