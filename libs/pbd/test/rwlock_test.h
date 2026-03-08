#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <atomic>

#include "pbd/rwlock.h"

class RWLockTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (RWLockTest);
	CPPUNIT_TEST (single_thread_test);
	CPPUNIT_TEST (run_thread_test);
	CPPUNIT_TEST (run_thread_sequence_test);
	CPPUNIT_TEST_SUITE_END ();

public:
	RWLockTest ();
	void single_thread_test ();
	void run_thread_test ();
	void run_thread_sequence_test ();

private:
	void worker_thread ();
	static void* launch_worker (void*);

	void reader_thread_a ();
	void reader_thread_b ();
	static void* launch_reader_a (void*);
	static void* launch_reader_b (void*);

	PBD::RWLock      _rwlock;
	std::atomic<int> _sequence;
};
