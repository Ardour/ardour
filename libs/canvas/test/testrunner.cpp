#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/BriefTestProgressListener.h>

int
main()
{
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

    return collectedresults.wasSuccessful () ? 0 : 1;
}
