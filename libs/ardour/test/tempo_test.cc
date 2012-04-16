#include "ardour/tempo.h"
#include "tempo_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (TempoTest);

using namespace std;
using namespace ARDOUR;
using namespace Timecode;

void
TempoTest::recomputeMapTest ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meterA (4, 4);
	map.add_meter (meterA, BBT_Time (1, 1, 0));

	/*
	  120bpm at bar 1, 240bpm at bar 4
	  
	  120bpm = 24e3 samples per beat
	  240bpm = 12e3 samples per beat
	*/
	

	/*
	  
	  120bpm                                                240bpm
	  0 beats                                               12 beats
	  0 frames                                              288e3 frames
	  |                 |                 |                 |             |
	  | 1.1 1.2 1.3 1.4 | 2.1 2.2 2.3.2.4 | 3.1 3.2 3.3 3.4 | 4.1 4.2 4.3 |

	*/

	Tempo tempoA (120);
	map.add_tempo (tempoA, BBT_Time (1, 1, 0));
	Tempo tempoB (240);
	map.add_tempo (tempoB, BBT_Time (4, 1, 0));
	Meter meterB (3, 4);
	map.add_meter (meterB, BBT_Time (4, 1, 0));

	list<MetricSection*>::iterator i = map.metrics.begin();
	CPPUNIT_ASSERT_EQUAL (framepos_t (0), (*i)->frame ());

	i = map.metrics.end();
	--i;
	CPPUNIT_ASSERT_EQUAL (framepos_t (288e3), (*i)->frame ());
}
