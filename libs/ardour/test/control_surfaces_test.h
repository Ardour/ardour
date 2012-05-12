#include "test_needing_session.h"

class ControlSurfacesTest : public TestNeedingSession
{
	CPPUNIT_TEST_SUITE (ControlSurfacesTest);
	CPPUNIT_TEST (instantiateAndTeardownTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void instantiateAndTeardownTest ();
};
