#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "pbd/mutex.h"

class MutexTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (MutexTest);
	CPPUNIT_TEST (test_mutex);
	CPPUNIT_TEST (test_cond);
	CPPUNIT_TEST_SUITE_END ();

public:
	MutexTest ();
	void test_mutex ();
	void test_cond ();

private:
	PBD::Mutex _mutex;
	PBD::Cond  _cond;
};
