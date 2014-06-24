#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/BriefTestProgressListener.h>
#include <glibmm/thread.h>
#include "scalar_properties.h"

#include "pbd/pbd.h"


int
main ()
{
	if (!PBD::init ()) return 1;

	ScalarPropertiesTest::make_property_quarks ();
	
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

	PBD::cleanup ();

	return collectedresults.wasSuccessful () ? 0 : 1;

}
