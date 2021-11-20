#include <cstdint>
#include <cstdlib>
#include <pthread.h>

#include "pbd/pbd.h"
#include "pbd/signals.h"

class Tx {
public:
	PBD::Signal1<void, int> sig1;
};

class Rx {
public:
	Rx (Tx& sender) {
		sender.sig1.connect_same_thread (_connection, boost::bind (&Rx::cb, this, _1));
	}

private:
	void cb (int i) {
		printf ("CB %d\n", i);
	}

	PBD::ScopedConnection _connection;
};

pthread_barrier_t barrier;

static void* delete_t (void* arg) {
	Tx* t = static_cast<Tx*> (arg);
	pthread_barrier_wait (&barrier);
	delete t;
	//printf ("Deleted tx\n");
	return 0;
}

static void* delete_r (void* arg) {
	Rx* r = static_cast<Rx*> (arg);
	pthread_barrier_wait (&barrier);
	delete r;
	//printf ("Deleted rx\n");
	return 0;
}

int
main (int argc, char** argv)
{
	PBD::init ();
	Tx* t = new Tx ();
	Rx* r = new Rx (*t);

	//t->sig1 (11); /* EMIT SIGNAL */

	pthread_barrier_init (&barrier, NULL, 2);
	pthread_t dt, dr;
	pthread_create (&dt, NULL, delete_t, (void*)t);
	pthread_create (&dr, NULL, delete_r, (void*)r);

	pthread_join (dt, NULL);
	pthread_join (dr, NULL);

	pthread_barrier_destroy (&barrier);
	PBD::cleanup ();
	return 0;
}
