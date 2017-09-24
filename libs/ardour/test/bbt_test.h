#include <cassert>
#include <sigc++/sigc++.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "temporal/bbt_time.h"

class BBTTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (BBTTest);
	CPPUNIT_TEST (addTest);
	CPPUNIT_TEST (subtractTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp () {
	}

	void tearDown () {
	}

	void addTest ();
	void subtractTest ();

private:
};
