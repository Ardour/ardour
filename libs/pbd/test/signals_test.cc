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

	e->Fred.connect_same_thread (c, boost::bind (&receiver));
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

class Receiver : public PBD::ScopedConnectionList
{
public:
	Receiver (Emitter* e) {
		e->Fred.connect_same_thread (*this, boost::bind (&Receiver::receiver, this));
	}

	void receiver () {
		cout << "Receiver::receiver\n";
		++N;
	}
};

void
SignalsTest::testScopedConnectionList ()
{
	Emitter* e = new Emitter;
	Receiver* r = new Receiver (e);

	N = 0;
	e->emit ();
	delete r;
	e->emit ();
	
	CPPUNIT_ASSERT_EQUAL (1, N);
}
