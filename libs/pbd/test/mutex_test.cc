#include "mutex_test.h"
#include <cppunit/TestAssert.h>
#include <thread>
#include <sys/time.h>

CPPUNIT_TEST_SUITE_REGISTRATION (MutexTest);

using namespace std;

MutexTest::MutexTest ()
{
}

void
MutexTest::test_mutex ()
{
	PBD::Mutex::Lock lm (_mutex);

	CPPUNIT_ASSERT (lm.locked( ));

	/* This will fail on POSIX systems but not on some older versions of glib
	 * on win32 as TryEnterCriticalSection is used and it will return true
	 * as CriticalSection is reentrant and fail the assertion.
	 */
	CPPUNIT_ASSERT (!_mutex.trylock());
}

void
MutexTest::test_cond ()
{
	PBD::Mutex::Lock lm (_mutex);

	CPPUNIT_ASSERT (!_cond.wait_for (_mutex, std::chrono::milliseconds (1000)));

	thread t1 ([&]() { _cond.signal (); });
	_cond.wait (_mutex);
	t1.join();

	thread t2 ([&]() { PBD::Mutex::Lock lm (_mutex); _cond.signal (); });
	if (_cond.wait_for (_mutex, std::chrono::milliseconds (1000))) {
		t2.join();
	} else {
		t2.detach();
		CPPUNIT_ASSERT_MESSAGE ("_cond.wait_until failed", false);
	}
}
