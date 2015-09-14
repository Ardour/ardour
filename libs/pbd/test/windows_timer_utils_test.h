#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class WindowsTimerUtilsTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (WindowsTimerUtilsTest);
	CPPUNIT_TEST (testQPC);
	CPPUNIT_TEST (testMMTimers);
	CPPUNIT_TEST_SUITE_END ();

public:
	void testQPC ();
	void testMMTimers ();

};
