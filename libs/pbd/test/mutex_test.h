#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "glibmm/threads.h"

class MutexTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (MutexTest);
	CPPUNIT_TEST (testBasic);
	CPPUNIT_TEST_SUITE_END ();

public:
	MutexTest ();
	void testBasic ();

private:
	Glib::Threads::Mutex m_mutex;
};
