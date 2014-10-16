#include "SMFTest.hpp"

#ifdef WIN32
#include <io.h> // for R_OK
#endif

using namespace std;

CPPUNIT_TEST_SUITE_REGISTRATION( SMFTest );

void
SMFTest::createNewFileTest ()
{
	TestSMF smf;
	smf.create("NewFile.mid");
	smf.close();
	CPPUNIT_ASSERT(access("NewFile.mid", R_OK) == 0);
	unlink(smf.path().c_str());
}

void
SMFTest::takeFiveTest ()
{
	TestSMF smf;
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
			//cerr << "read smf event type " << hex << int(buf[0]) << endl;
			// make ev.time absolute time in frames
			ev.set_time(time * frames_per_beat / (double)smf.ppqn());
			ev.set_event_type(type_map->midi_event_type(buf[0]));
			seq->append(ev, next_event_id ());
		}
	}

	seq->end_write (Sequence<Time>::Relax, false);
	CPPUNIT_ASSERT(!seq->empty());
}
