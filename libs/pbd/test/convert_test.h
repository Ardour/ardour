#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ConvertTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ConvertTest);
	CPPUNIT_TEST (testUrlDecode);
	CPPUNIT_TEST_SUITE_END ();

public:
	void testUrlDecode ();

};
