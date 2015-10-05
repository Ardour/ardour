#include <glibmm/thread.h>

#include "signals_test.h"
#include "pbd/signals.h"

using namespace std;

CPPUNIT_TEST_SUITE_REGISTRATION (SignalsTest);

void
SignalsTest::setUp ()
{
	if (!Glib::thread_supported ()) {
		Glib::thread_init ();
	}
}

class Emitter {
public:
	void emit () {
		Fred ();
	}

	PBD::Signal0<void> Fred;
};

static int N = 0;

void
receiver ()
{
	++N;
}

void
SignalsTest::testEmission ()
{
	Emitter* e = new Emitter;
	PBD::ScopedConnection c;
	e->Fred.connect_same_thread (c, boost::bind (&receiver));

	N = 0;
	e->emit ();
	e->emit ();
	CPPUNIT_ASSERT_EQUAL (2, N);

	PBD::ScopedConnection d;
	e->Fred.connect_same_thread (d, boost::bind (&receiver));
	N = 0;
	e->emit ();
	CPPUNIT_ASSERT_EQUAL (2, N);
}

void
SignalsTest::testDestruction ()
{
	Emitter* e = new Emitter;
	PBD::ScopedConnection c;
	e->Fred.connect_same_thread (c, boost::bind (&receiver));
	e->emit ();
	delete e;
	c.disconnect ();

	CPPUNIT_ASSERT (true);
}

class AReceiver : public PBD::ScopedConnectionList
{
public:
	AReceiver (Emitter* e) {
		e->Fred.connect_same_thread (*this, boost::bind (&AReceiver::receiver, this));
	}

	void receiver () {
		++N;
	}
};

void
SignalsTest::testScopedConnectionList ()
{
	Emitter* e = new Emitter;
	AReceiver* r = new AReceiver (e);

	N = 0;
	e->emit ();
	delete r;
	e->emit ();

	CPPUNIT_ASSERT_EQUAL (1, N);
}
