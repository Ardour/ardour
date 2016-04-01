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
	map.add_meter (meterA, 0.0, BBT_Time (1, 1, 0));

	/*
	  120bpm at bar 1, 240bpm at bar 4

	  120bpm = 24e3 samples per beat
	  240bpm = 12e3 samples per beat
	*/


	/*

	  120bpm                                                240bpm
	  0 beats                                               12 beats
	  0 frames                                              288e3 frames
	  0 pulses                                              4 pulses
	  |                 |                 |                 |             |
	  | 1.1 1.2 1.3 1.4 | 2.1 2.2 2.3.2.4 | 3.1 3.2 3.3 3.4 | 4.1 4.2 4.3 |

	*/

	Tempo tempoA (120);
	map.add_tempo (tempoA, 0.0, TempoSection::Constant);
	Tempo tempoB (240);
	map.add_tempo (tempoB, 12.0, TempoSection::Constant);
	Meter meterB (3, 4);
	map.add_meter (meterB, 12.0, BBT_Time (4, 1, 0));

	list<MetricSection*>::iterator i = map._metrics.begin();
	CPPUNIT_ASSERT_EQUAL (framepos_t (0), (*i)->frame ());

	i = map._metrics.end();
	--i;
	CPPUNIT_ASSERT_EQUAL (framepos_t (288e3), (*i)->frame ());
}

void
TempoTest::rampTest ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meterA (4, 4);
	Tempo tempoA (77.0, 4.0);
	Tempo tempoB (217.0, 4.0);
	map.add_tempo (tempoA, 0.0, TempoSection::Ramp);
	map.add_tempo (tempoB, 100.0, TempoSection::Ramp);
	map.add_meter (meterA, 0.0, BBT_Time (1, 1, 0));

	/*

	  120bpm                                                240bpm
	  0 beats                                               12 beats
	  0 frames                                              288e3 frames
	  0 pulses                                              4 pulses
	  |                 |                 |                 |             |
	  | 1.1 1.2 1.3 1.4 | 2.1 2.2 2.3.2.4 | 3.1 3.2 3.3 3.4 | 4.1 4.2 4.3 |

	*/

	TempoSection* tA = 0;
	TempoSection* tB;
	list<MetricSection*>::iterator i;

	for (i = map._metrics.begin(); i != map._metrics.end(); ++i) {
		if ((tB = dynamic_cast<TempoSection*> (*i)) != 0) {
			if (tA) {
				break;
			}
			tA = tB;
		}
	}
	map.recompute_map (map._metrics);

	CPPUNIT_ASSERT_EQUAL (tB->frame(), tA->frame_at_tempo (tB->beats_per_minute() / tB->note_type(), 100.0, sampling_rate));
	CPPUNIT_ASSERT_EQUAL (tB->frame(), tA->frame_at_pulse (tB->pulse(), sampling_rate));
}
