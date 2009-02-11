#include "SMFTest.hpp"

using namespace std;

CPPUNIT_TEST_SUITE_REGISTRATION( SMFTest );

void
SMFTest::takeFiveTest ()
{
	TestSMF<Time> smf;
	smf.open("./test/testdata/TakeFive.mid");
	CPPUNIT_ASSERT(!smf.is_empty());
	
	seq->start_write();
	smf.seek_to_start();

	uint64_t time = 0; /* in SMF ticks */
	Evoral::Event<double> ev;
	
	const double frames_per_beat = 100.0;
	
	uint32_t delta_t = 0;
	uint32_t size    = 0;
	uint8_t* buf     = NULL;
	int ret;
	while ((ret = smf.read_event(&delta_t, &size, &buf)) >= 0) {
		ev.set(buf, size, 0.0);
		time += delta_t;
		
		if (ret > 0) { // didn't skip (meta) event
			cerr << "read smf event type " << hex << int(buf[0]) << endl;
			// make ev.time absolute time in frames
			ev.time() = time * frames_per_beat / (double)smf.ppqn();
			ev.set_event_type(type_map->midi_event_type(buf[0]));
			seq->append(ev);
		}
	}

	seq->end_write(false);
	CPPUNIT_ASSERT(!seq->empty());
}
