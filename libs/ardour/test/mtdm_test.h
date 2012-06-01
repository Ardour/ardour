#include <sigc++/sigc++.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class MTDMTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (MTDMTest);
	CPPUNIT_TEST (basicTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp () {}
	void tearDown () {}

	void basicTest ();
};

