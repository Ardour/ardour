#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class XPathTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (XPathTest);
	CPPUNIT_TEST (testMisc);
	CPPUNIT_TEST_SUITE_END ();

public:

	void testMisc ();
};
