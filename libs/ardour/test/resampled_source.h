#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ResampledSourceTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ResampledSourceTest);
	CPPUNIT_TEST (seekTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void seekTest ();
};
