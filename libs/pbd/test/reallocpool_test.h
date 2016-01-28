#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "glibmm/threads.h"

class ReallocPoolTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReallocPoolTest);
	CPPUNIT_TEST (testBasic);
	CPPUNIT_TEST_SUITE_END ();

public:
	ReallocPoolTest ();
	void testBasic ();

private:
};
