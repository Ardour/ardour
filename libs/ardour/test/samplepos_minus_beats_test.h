#include <sigc++/sigc++.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class FrameposMinusBeatsTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (FrameposMinusBeatsTest);
	CPPUNIT_TEST (singleTempoTest);
	CPPUNIT_TEST (doubleTempoTest);
	CPPUNIT_TEST (doubleTempoWithMeterTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp () {}
	void tearDown () {}

	void singleTempoTest ();
	void doubleTempoTest ();
	void doubleTempoWithMeterTest ();
};

