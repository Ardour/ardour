#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class PluginsTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (PluginsTest);
	CPPUNIT_TEST (test);
	CPPUNIT_TEST_SUITE_END ();

public:
	void test ();
};
