#include <getopt.h>

#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/BriefTestProgressListener.h>

#include "pbd/debug.h"
#include "ardour/ardour.h"

int
main(int argc, char* argv[])
{
	if (!Glib::thread_supported()) {
		Glib::thread_init();
	}

	const struct option longopts[] = {
		{ "debug", 1, 0, 'D' },
		{ 0, 0, 0, 0 }
	};
	const char *optstring = "D:";
	int option_index = 0;
	int c = 0;

	while (1) {
		c = getopt_long (argc, argv, optstring, longopts, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 0:
			break;

		case 'D':
			if (PBD::parse_debug_options (optarg)) {
				exit (0);
			}
			break;
		}
	}

	ARDOUR::init (false, true);
	
	CppUnit::TestResult testresult;
	
	CppUnit::TestResultCollector collectedresults;
	testresult.addListener (&collectedresults);
	
	CppUnit::BriefTestProgressListener progress;
	testresult.addListener (&progress);
	
	CppUnit::TestRunner testrunner;
	testrunner.addTest (CppUnit::TestFactoryRegistry::getRegistry ().makeTest ());
	testrunner.run (testresult);
	
	CppUnit::CompilerOutputter compileroutputter (&collectedresults, std::cerr);
	compileroutputter.write ();

	ARDOUR::cleanup ();
	
	return collectedresults.wasSuccessful () ? 0 : 1;
}
