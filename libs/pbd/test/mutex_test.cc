#include "mutex_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (MutexTest);

using namespace std;

MutexTest::MutexTest ()
{
}

void
MutexTest::testBasic ()
{
	Glib::Threads::Mutex::Lock lm (m_mutex);

	CPPUNIT_ASSERT (lm.locked());

	/* This will fail on POSIX systems but not on some older versions of glib
	 * on win32 as TryEnterCriticalSection is used and it will return true
	 * as CriticalSection is reentrant and fail the assertion.
	 */
	CPPUNIT_ASSERT (!m_mutex.trylock());

}
