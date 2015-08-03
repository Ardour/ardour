#include <sigc++/sigc++.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class Sha1Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (Sha1Test);
	CPPUNIT_TEST (basicTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp () {}
	void tearDown () {}

	void basicTest ();
};
