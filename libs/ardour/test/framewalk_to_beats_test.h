#include <sigc++/sigc++.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class FramewalkToBeatsTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (FramewalkToBeatsTest);
	CPPUNIT_TEST (singleTempoTest);
	CPPUNIT_TEST (doubleTempoTest);
	CPPUNIT_TEST (tripleTempoTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp () {}
	void tearDown () {}

	void singleTempoTest ();
	void doubleTempoTest ();
	void tripleTempoTest ();
};

