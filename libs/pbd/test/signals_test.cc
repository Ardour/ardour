#include "signals_test.h"
#include "pbd/signals.h"

CPPUNIT_TEST_SUITE_REGISTRATION (SignalsTest);

class Emitter {
public:
	void emit () {
		Fred ();
	}
	
	PBD::Signal0<void> Fred;
};

void
receiver ()
{

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

