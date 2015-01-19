#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class CurveTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (CurveTest);
	CPPUNIT_TEST (interpolateTest1);
	CPPUNIT_TEST_SUITE_END ();

public:
	void interpolateTest1 ();
};

	
