#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/BriefTestProgressListener.h>
#include <glibmm/thread.h>
#include "scalar_properties.h"

#include "pbd/pbd.h"
#include "pbd/error.h"
#include "pbd/textreceiver.h"

int
main ()
{
	TextReceiver text_receiver ("pbd_test");

	if (!PBD::init ()) return 1;

	text_receiver.listen_to (PBD::info);
	text_receiver.listen_to (PBD::warning);
	text_receiver.listen_to (PBD::error);
	text_receiver.listen_to (PBD::fatal);

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
