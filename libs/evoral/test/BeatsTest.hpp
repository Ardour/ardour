#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class BeatsTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (BeatsTest);
	CPPUNIT_TEST (operator_eq);
	CPPUNIT_TEST_SUITE_END ();

public:
	void operator_eq ();
};
