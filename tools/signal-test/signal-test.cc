#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <getopt.h>
#include <pthread.h>

#include "pbd/event_loop.h"
#include "pbd/pbd.h"
#include "pbd/pcg_rand.h"
#include "pbd/signals.h"

class Tx
{
public:
	PBD::Signal1<void, int> sig1;
};

/* ****************************************************************************/

class Rx1
{
public:
	Rx1 (Tx& sender)
	{
		sender.sig1.connect_same_thread (_connection, boost::bind (&Rx1::cb, this, _1));
	}

private:
	void cb (int i)
	{
		printf ("Rx1(%d) ", i);
	}

	PBD::ScopedConnection _connection;
};

/* ****************************************************************************/

class MyEventLoop : public sigc::trackable, public PBD::EventLoop
{
public:
	MyEventLoop (std::string const& name)
	: EventLoop (name)
	{
		run_loop_thread = Glib::Threads::Thread::self ();
	}

	void call_slot (InvalidationRecord* ir, const boost::function<void ()>& f)
	{
		if (Glib::Threads::Thread::self () == run_loop_thread) {
			f ();
		} else {
			assert (!ir);
			assert (0);
			f (); // XXX really queue and process during run ()
		}
	}

	void run ()
	{
		; // process Events, if any
	}

	Glib::Threads::Mutex& slot_invalidation_mutex ()
	{
		return request_buffer_map_lock;
	}

private:
	Glib::Threads::Thread* run_loop_thread;
	Glib::Threads::Mutex   request_buffer_map_lock;
};

struct MyInvalidationRecord : public PBD::EventLoop::InvalidationRecord {
	~MyInvalidationRecord ()
	{
		assert (use_count () == 0);
	}
};

static MyEventLoop          event_loop ("foo");
static MyInvalidationRecord _ir;

class Rx2 : public PBD::ScopedConnectionList
{
public:
	Rx2 (Tx& sender)
	{
		sender.sig1.connect (*this, &_ir, boost::bind (&Rx2::cb, this, _1), &event_loop);
	}

private:
	void cb (int i)
	{
		printf ("Rx2(%d) ", i);
	}
};

/* ****************************************************************************/

pthread_barrier_t barrier;

static void*
delete_tx (void* arg)
{
	Tx* tx = static_cast<Tx*> (arg);
	pthread_barrier_wait (&barrier);
	delete tx;
	return 0;
}

static void*
delete_rx1 (void* arg)
{
	Rx1* rx1 = static_cast<Rx1*> (arg);
	pthread_barrier_wait (&barrier);
	delete rx1;
	return 0;
}

static void*
delete_rx2 (void* arg)
{
	Rx2* rx2 = static_cast<Rx2*> (arg);
	pthread_barrier_wait (&barrier);
	delete rx2;
	return 0;
}

/* ****************************************************************************/

static PBD::PCGRand pcg;
static bool         emit_signal = false;

static void
run_test ()
{
	Tx*  tx  = new Tx ();
	Rx1* rx1 = new Rx1 (*tx);
	Rx2* rx2 = new Rx2 (*tx);

	/* randomize thread start, not that it matters much since 
	 * pthread_barrier_wait() leaves it undefined which thread
	 * continues with PTHREAD_BARRIER_SERIAL_THREAD, but some
	 * implementations may special-case the last */
	static int rnd[3] = { 0, 1, 2 };
	for (int i = 2; i > 0; --i) {
		int j   = pcg.rand (i + 1);
		int tmp = rnd[i];
		rnd[i]  = rnd[j];
		rnd[j]  = tmp;
	}

	if (emit_signal) {
		tx->sig1 (rnd[0]); /* EMIT SIGNAL */
	}

	pthread_t t[3];
	for (int i = 0; i < 3; ++i) {
		switch (rnd[i]) {
			case 0:
				pthread_create (&t[0], NULL, delete_tx, (void*)tx);
				break;
			case 1:
				pthread_create (&t[1], NULL, delete_rx1, (void*)rx1);
				break;
			case 2:
				pthread_create (&t[2], NULL, delete_rx2, (void*)rx2);
				break;
		}
	}

	for (int i = 0; i < 3; ++i) {
		pthread_join (t[i], NULL);
	}

	if (emit_signal) {
		printf ("\n");
	}
}

int
main (int argc, char** argv)
{
	int         n_iter    = 0;
	const char* optstring = "ei:";

	/* clang-format off */
	const struct option longopts[] = {
		{ "emit",       no_argument,       0, 'e' },
		{ "iterations", required_argument, 0, 'i' },
	};
	/* clang-format on */

	int c = 0;
	while (EOF != (c = getopt_long (argc, argv, optstring, longopts, (int*)0))) {
		switch (c) {
			case 'e':
				emit_signal = true;
				break;
			case 'i':
				n_iter = atoi (optarg);
				break;
			default:
				fprintf (stderr, "Error: unrecognized option.\n");
				::exit (EXIT_FAILURE);
				break;
		}
	}

	if (optind != argc) {
		fprintf (stderr, "Error: unrecognized option.\n");
		::exit (EXIT_FAILURE);
	}

	if (n_iter <= 0 || n_iter > 1000000) {
		n_iter = 1000;
	}

	PBD::init ();
	pthread_barrier_init (&barrier, NULL, 3);

	for (int i = 0; i < n_iter; ++i) {
		run_test ();
	}

	pthread_barrier_destroy (&barrier);
	PBD::cleanup ();
	return 0;
}
