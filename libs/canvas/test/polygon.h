#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class PolygonTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (PolygonTest);
	CPPUNIT_TEST (bounding_box);
	CPPUNIT_TEST_SUITE_END ();

public:
	void bounding_box ();
};
