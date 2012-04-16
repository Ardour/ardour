#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class RangeTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (RangeTest);
	CPPUNIT_TEST (coalesceTest);
	CPPUNIT_TEST (subtractTest1);
	CPPUNIT_TEST (subtractTest3);
	CPPUNIT_TEST_SUITE_END ();

public:
	void coalesceTest ();
	void subtractTest1 ();
	void subtractTest2 ();
	void subtractTest3 ();
};

	
