#include "framewalk_to_beats_test.h"
#include "ardour/tempo.h"
#include "timecode/bbt_time.h"

CPPUNIT_TEST_SUITE_REGISTRATION (FramewalkToBeatsTest);

using namespace std;
using namespace ARDOUR;
using namespace Timecode;

void
FramewalkToBeatsTest::singleTempoTest ()
{
	int const sampling_rate = 48000;
	int const bpm = 120;

	double const frames_per_beat = (60 / double (bpm)) * double (sampling_rate);
	
	TempoMap map (sampling_rate);
	Tempo tempo (bpm);
	Meter meter (4, 4);

	map.add_meter (meter, BBT_Time (1, 1, 0));
	map.add_tempo (tempo, BBT_Time (1, 1, 0));

	/* Add 1 beats-worth of frames to a 2 beat starting point */
	double r = map.framewalk_to_beats (frames_per_beat * 2, frames_per_beat * 1);
	CPPUNIT_ASSERT (r == 1);

	/* Add 6 beats-worth of frames to a 3 beat starting point */
	r = map.framewalk_to_beats (frames_per_beat * 3, frames_per_beat * 6);
	CPPUNIT_ASSERT (r == 6);
}
