#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class DSPLoadCalculatorTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (DSPLoadCalculatorTest);
	CPPUNIT_TEST (basicTest);
	CPPUNIT_TEST_SUITE_END ();

public:
	void basicTest ();
};
