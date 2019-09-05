#include <sigc++/sigc++.h>
#include "midi_clock_test.h"

using namespace std;
using namespace ARDOUR;

CPPUNIT_TEST_SUITE_REGISTRATION(MIDIClock_Test);

void
MclkTestMaster::testStepResponse ()
{
	double      speed    = 1.0;
	samplepos_t position = 0;
	MIDI::Parser* parser = 0;

	samplecnt_t period_size = 4096;
	samplepos_t start_time  = 1000000;

	start (*parser, start_time);

	update_midi_clock (*parser, start_time);

	for (samplecnt_t i = 1; i <= 100 * period_size; i++) {
		/* simulate jitter */
		samplecnt_t input_delta = samplecnt_t (one_ppqn_in_samples + 0.1 * (double(g_random_int()) / double (RAND_MAX)) * one_ppqn_in_samples);

		if (i % input_delta == 0) {
			update_midi_clock (*parser, start_time + i);
		}

		if (i % period_size == 0) {
			samplepos_t most_recent;
			samplepos_t when;
			speed_and_position (speed, position, most_recent, when, start_time + i);
			// TODO CPPUNIT_ASSERT_EQUAL ?!
		}
	}
}

void MIDIClock_Test::run_test ()
{
	/* Note: A running engine is required to construct
	 * ARDOUR::MIDIClock_TransportMaster
	 */
	MclkTestMaster* m = new MclkTestMaster;
	m->set_session (_session);
	m->testStepResponse ();
	delete m;
}
