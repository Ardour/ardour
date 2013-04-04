#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ArrowTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ArrowTest);
	CPPUNIT_TEST (bounding_box);
	CPPUNIT_TEST_SUITE_END ();

public:
	void bounding_box ();
};
