#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class NatSortTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (NatSortTest);
	CPPUNIT_TEST (testBasic);
	CPPUNIT_TEST_SUITE_END ();

public:
	NatSortTest () { }
	void testBasic ();

private:
};
