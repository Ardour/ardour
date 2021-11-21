#include <cstdint>
#include <cstdlib>
#include <pthread.h>

#include "pbd/pbd.h"
#include "pbd/signals.h"
#include "pbd/event_loop.h"

class Tx {
public:
	PBD::Signal1<void, int> sig1;
};

/* ****************************************************************************/

class Rx1 {
public:
	Rx1 (Tx& sender) {
		sender.sig1.connect_same_thread (_connection, boost::bind (&Rx1::cb, this, _1));
	}

private:
	void cb (int i) {
		printf ("Rx1 %d\n", i);
	}

	PBD::ScopedConnection _connection;
};

/* ****************************************************************************/

struct MyInvalidationRecord : public PBD::EventLoop::InvalidationRecord
{
	~MyInvalidationRecord () {
		assert (use_count () == 0);
	}
};

MyInvalidationRecord _ir;

class Rx2 : public PBD::ScopedConnectionList {
public:
	Rx2 (Tx& sender) {
		sender.sig1.connect (*this, &_ir, boost::bind (&Rx2::cb, this, _1), /* PBD::EventLoop */ 0);
	}

private:
	void cb (int i) {
		printf ("CB %d\n", i);
	}
};

/* ****************************************************************************/

pthread_barrier_t barrier;

static void* delete_tx (void* arg) {
	Tx* tx = static_cast<Tx*> (arg);
	pthread_barrier_wait (&barrier);
	delete tx;
	//printf ("Deleted tx\n");
	return 0;
}

static void* delete_rx1 (void* arg) {
	Rx1* rx1 = static_cast<Rx1*> (arg);
	pthread_barrier_wait (&barrier);
	delete rx1;
	//printf ("Deleted rx1\n");
	return 0;
}

static void* delete_rx2 (void* arg) {
	Rx2* rx2 = static_cast<Rx2*> (arg);
	pthread_barrier_wait (&barrier);
	delete rx2;
	//printf ("Deleted rx2\n");
	return 0;
}

/* ****************************************************************************/

int
main (int argc, char** argv)
{
	PBD::init ();
	Tx*  tx = new Tx ();
	Rx1* rx1 = new Rx1 (*tx);
	Rx2* rx2 = new Rx2 (*tx);

	pthread_barrier_init (&barrier, NULL, 3);
	pthread_t t[3];

	pthread_create (&t[0], NULL, delete_tx, (void*)tx);
	pthread_create (&t[1], NULL, delete_rx1, (void*)rx1);
	pthread_create (&t[2], NULL, delete_rx2, (void*)rx2);

	for (int i = 0; i < 3; ++i) {
		pthread_join (t[i], NULL);
	}

	pthread_barrier_destroy (&barrier);
	PBD::cleanup ();
	return 0;
}
