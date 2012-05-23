#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class SignalsTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (SignalsTest);
	CPPUNIT_TEST (testEmission);
	CPPUNIT_TEST (testDestruction);
	CPPUNIT_TEST (testScopedConnectionList);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp ();
	void testEmission ();
	void testDestruction ();
	void testScopedConnectionList ();
};
