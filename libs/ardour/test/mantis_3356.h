#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

/** Test for Mantis bug #3356 */
class Mantis3356Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (Mantis3356Test);
	CPPUNIT_TEST (test);
	CPPUNIT_TEST_SUITE_END ();

public:
	void test ();
};
