#include <glibmm/thread.h>

#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/BriefTestProgressListener.h>

#include "pbd/debug.h"
#include "ardour/ardour.h"

static const char* localedir = LOCALEDIR;

int
main(int argc, char* argv[])
{
	ARDOUR::init (&argc, &argv, localedir);
	
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
