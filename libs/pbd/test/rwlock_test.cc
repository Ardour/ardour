#include <glib.h>
#include <sched.h>
#include "rwlock_test.h"

using namespace PBD;

CPPUNIT_TEST_SUITE_REGISTRATION (RWLockTest);

RWLockTest::RWLockTest ()
{
}

void
RWLockTest::single_thread_test ()
{
	RWLock::ReaderLock rl (_rwlock);

	CPPUNIT_ASSERT (rl.locked());

	/* trying to get another read-lock should be OK */
	CPPUNIT_ASSERT (_rwlock.reader_trylock());
	_rwlock.reader_unlock ();

	rl.release ();

	RWLock::WriterLock rw (_rwlock);

	CPPUNIT_ASSERT (!_rwlock.reader_trylock());

	/* leaving the scope releases ReaderLock */
}

#define N_WORKERS 3

void*
RWLockTest::launch_reader (void* d)
{
	RWLockTest* self = static_cast<RWLockTest*> (d);
	self->reader_thread ();
	return NULL;
}

void
RWLockTest::reader_thread ()
{
	RWLock::ReaderLock rl (_rwlock);
	g_usleep (1000000); // 1 sec
}

void
RWLockTest::run_thread_test ()
{
	pthread_t workers[N_WORKERS];
	for (int i = 0; i < N_WORKERS; ++i) {
		CPPUNIT_ASSERT (0 == pthread_create (&workers[i], NULL, &RWLockTest::launch_reader, this));
	}

	sched_yield ();
	g_usleep (100);

	/* worker thread(s) should hold read-lock by now */
	RWLock::WriterLock wl (_rwlock, RWLock::NotLock);
	CPPUNIT_ASSERT (!wl.try_acquire());

	RWLock::ReaderLock trl (_rwlock, RWLock::TryLock);
	RWLock::WriterLock twl (_rwlock, RWLock::TryLock);

	CPPUNIT_ASSERT (trl.locked ());
	CPPUNIT_ASSERT (!twl.locked ());
	trl.release ();

	for (int i = 0; i < N_WORKERS; ++i) {
		CPPUNIT_ASSERT (0 == pthread_join (workers[i], NULL));
	}

	CPPUNIT_ASSERT (wl.try_acquire());

	CPPUNIT_ASSERT (!trl.try_acquire());
}
