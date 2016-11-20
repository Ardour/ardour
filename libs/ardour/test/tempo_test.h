#include <sigc++/sigc++.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TempoTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (TempoTest);
	CPPUNIT_TEST (recomputeMapTest48);
	CPPUNIT_TEST (recomputeMapTest44);
	CPPUNIT_TEST (qnDistanceTestConstant);
	CPPUNIT_TEST (qnDistanceTestRamp);
	CPPUNIT_TEST (rampTest48);
	CPPUNIT_TEST (rampTest44);
	CPPUNIT_TEST (tempoAtPulseTest);
	CPPUNIT_TEST (tempoFundamentalsTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp () {}
	void tearDown () {}

	void recomputeMapTest ();
	void recomputeMapTest44 ();
	void recomputeMapTest48 ();
	void qnDistanceTestConstant ();
	void qnDistanceTestRamp ();
	void rampTest48 ();
	void rampTest44 ();
	void tempoAtPulseTest();
	void tempoFundamentalsTest();
};

