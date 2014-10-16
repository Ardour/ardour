#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class AudioEngineTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (AudioEngineTest);
	CPPUNIT_TEST (test_backends);
	CPPUNIT_TEST (test_start);
	CPPUNIT_TEST_SUITE_END ();

public:
	void test_backends ();
	void test_start ();
};
