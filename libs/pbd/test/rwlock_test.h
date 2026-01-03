#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "pbd/rwlock.h"

class RWLockTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (RWLockTest);
	CPPUNIT_TEST (single_thread_test);
	CPPUNIT_TEST (run_thread_test);
	CPPUNIT_TEST_SUITE_END ();

public:
	RWLockTest ();
	void single_thread_test ();
	void run_thread_test ();

private:
	void reader_thread ();
	static void* launch_reader (void*);

	PBD::RWLock _rwlock;
};
