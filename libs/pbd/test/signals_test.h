#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class SignalsTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (SignalsTest);
	CPPUNIT_TEST (testDestruction);
	CPPUNIT_TEST_SUITE_END ();

public:
	void testDestruction ();
};
