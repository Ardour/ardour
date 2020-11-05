#include <glibmm.h>

#include "rcu_test.h"

using namespace std;

RCUTest::RCUTest ()
	: CppUnit::TestFixture ()
	, _values (new Values)
{
}

CPPUNIT_TEST_SUITE_REGISTRATION (RCUTest);

void
RCUTest::setUp ()
{
	if (!Glib::thread_supported ()) {
		Glib::thread_init ();
	}
}

static void*
launch_reader(void* self)
{
	RCUTest* r = static_cast<RCUTest *>(self);
	r->read_thread ();
	return NULL;
}

static void*
launch_writer(void* self)
{
	RCUTest* r = static_cast<RCUTest *>(self);
	r->write_thread ();
	return NULL;
}

void
RCUTest::race ()
{
#ifdef __APPLE__
	pthread_mutex_init (&_mutex, NULL);
	pthread_cond_init (&_cond, NULL);
	_cnt = 0;
#else
	pthread_barrier_init (&_barrier, NULL, 2);
#endif

	pthread_t reader_thread;
	pthread_t writer_thread;

	CPPUNIT_ASSERT (pthread_create (&writer_thread, NULL, launch_writer, this) == 0);
	CPPUNIT_ASSERT (pthread_create (&reader_thread, NULL, launch_reader, this) == 0);

	void* return_value;
	CPPUNIT_ASSERT (pthread_join (writer_thread, &return_value) == 0);
	CPPUNIT_ASSERT (pthread_join (reader_thread, &return_value) == 0);

#ifdef __APPLE__
	pthread_mutex_destroy (&_mutex);
	pthread_cond_destroy (&_cond);
#else
	pthread_barrier_destroy (&_barrier);
#endif
}

/* ****************************************************************************/

void
RCUTest::read_thread ()
{
#ifdef __APPLE__
	pthread_mutex_lock (&_mutex);
	if (++_cnt == 2) {
		pthread_cond_broadcast (&_cond);
		pthread_mutex_unlock (&_mutex);
	} else {
		pthread_cond_wait (&_cond, &_mutex);
	}
	pthread_mutex_unlock (&_mutex);
#else
	pthread_barrier_wait (&_barrier);
#endif

	for (int i = 0; i < 15000; ++i) {
		boost::shared_ptr<Values> reader  = _values.reader ();
		for (Values::const_iterator i = reader->begin (); i != reader->end(); ++i) {
			CPPUNIT_ASSERT (i->first == i->second->val);
		}
	}
}

void
RCUTest::write_thread ()
{
#ifdef __APPLE__
	pthread_mutex_lock (&_mutex);
	if (++_cnt == 2) {
		pthread_cond_broadcast (&_cond);
		pthread_mutex_unlock (&_mutex);
	} else {
		pthread_cond_wait (&_cond, &_mutex);
	}
	pthread_mutex_unlock (&_mutex);
#else
	pthread_barrier_wait (&_barrier);
#endif

	for (int i = 0; i < 10000; ++i) {
		RCUWriter<Values> writer (_values);
		boost::shared_ptr<Values> w = writer.get_copy ();
		char tmp [64];
		sprintf (tmp, "foo %d", i);
		w->insert (make_pair (tmp, new Value (tmp)));
	}

	/* replace */
	for (int i = 0; i < 2500; ++i) {
		RCUWriter<Values> writer (_values);
		boost::shared_ptr<Values> w = writer.get_copy ();

		char tmp [64];
		sprintf (tmp, "foo %d", i);

		Values::iterator x = w->find (tmp);
		CPPUNIT_ASSERT (x != w->end ());
		sprintf (tmp, "bar %d", i);

		w->erase (x);
		w->insert (make_pair (tmp, new Value (tmp)));
	}

	/* clear */
	{
		RCUWriter<Values> writer (_values);
		boost::shared_ptr<Values> w = writer.get_copy ();
		w->clear ();
	}
	_values.flush ();
}
