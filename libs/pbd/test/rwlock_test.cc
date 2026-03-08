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

/* ****************************************************************************/

#define N_WORKERS 3

void*
RWLockTest::launch_worker (void* d)
{
	RWLockTest* self = static_cast<RWLockTest*> (d);
	self->worker_thread ();
	return NULL;
}

void
RWLockTest::worker_thread ()
{
	RWLock::ReaderLock rl (_rwlock);
	g_usleep (1000000); // 1 sec
}

void
RWLockTest::run_thread_test ()
{
	pthread_t workers[N_WORKERS];
	for (int i = 0; i < N_WORKERS; ++i) {
		CPPUNIT_ASSERT (0 == pthread_create (&workers[i], NULL, &RWLockTest::launch_worker, this));
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

/* ****************************************************************************/

void*
RWLockTest::launch_reader_a (void* d)
{
	RWLockTest* self = static_cast<RWLockTest*> (d);
	self->reader_thread_a ();
	return NULL;
}

void*
RWLockTest::launch_reader_b (void* d)
{
	RWLockTest* self = static_cast<RWLockTest*> (d);
	self->reader_thread_b ();
	return NULL;
}

void
RWLockTest::reader_thread_a ()
{
	RWLock::ReaderLock rl1 (_rwlock);
	/* release main thread */
	_sequence++; // = 1

	/* wait for main thread to ack .. */
	int want = 2;
	while (!_sequence.compare_exchange_strong (want, 3)) {
		want = 2;
		sched_yield ();
		g_usleep (1000);
	}

	/* wait for main thread to ask for the writer-lock */
	sched_yield ();
	g_usleep (1000000); // 1 sec

	/* Now test if 2nd read-lock can be taken while writer-lock waits on rl1 */

	RWLock::ReaderLock rl2 (_rwlock, RWLock::TryLock);
	CPPUNIT_ASSERT (rl2.locked());

	_sequence++; // = 4

	want = 6;
	while (!_sequence.compare_exchange_strong (want, 7)) {
		want = 6;
		sched_yield ();
		g_usleep (1000);
	}
}

void
RWLockTest::reader_thread_b ()
{
	int want = 4;
	while (!_sequence.compare_exchange_strong (want, 5)) {
		want = 4;
		sched_yield ();
		g_usleep (1000);
	}

	RWLock::ReaderLock rl (_rwlock, RWLock::TryLock);
	CPPUNIT_ASSERT (rl.locked());
	_sequence++; // = 6

	want = 7;
	while (!_sequence.compare_exchange_strong (want, 8)) {
		want = 7;
		sched_yield ();
		g_usleep (1000);
	}
}


void
RWLockTest::run_thread_sequence_test ()
{
	/* 1. Thread A takes read-lock
	 * 2. Thread M asks for write-lock (blocks)
	 * 3. Thread A takes another read-lock
	 * 4. Thread B takes another read-lock
	 *
	 * Test that additional reader-lock can still be
	 * taken while writer-lock waits.
	 */

	_sequence = 0;

	pthread_t r[2];
	CPPUNIT_ASSERT (0 == pthread_create (&r[0], NULL, &RWLockTest::launch_reader_a, this));
	CPPUNIT_ASSERT (0 == pthread_create (&r[1], NULL, &RWLockTest::launch_reader_b, this));

	int want = 1;
	while (!_sequence.compare_exchange_strong (want, 2)) {
		want = 1;
		sched_yield ();
		g_usleep (1000);
	}

	/* worker thread(s) should hold read-lock by now */
	RWLock::WriterLock wl (_rwlock);
	CPPUNIT_ASSERT (wl.locked());
	CPPUNIT_ASSERT (_sequence == 8);

	CPPUNIT_ASSERT (0 == pthread_join (r[0], NULL));
	CPPUNIT_ASSERT (0 == pthread_join (r[1], NULL));
}
