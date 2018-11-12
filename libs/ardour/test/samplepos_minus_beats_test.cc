#include "samplepos_minus_beats_test.h"
#include "ardour/tempo.h"
#include "temporal/bbt_time.h"

CPPUNIT_TEST_SUITE_REGISTRATION (FrameposMinusBeatsTest);

using namespace std;
using namespace ARDOUR;
using namespace Timecode;
using namespace Evoral;

/* Basic tests with no tempo / meter changes */
void
FrameposMinusBeatsTest::singleTempoTest ()
{
	int const sampling_rate = 48000;
	int const bpm = 120;

	double const samples_per_beat = (60 / double (bpm)) * double (sampling_rate);

	TempoMap map (sampling_rate);
	Tempo tempo (bpm);
	Meter meter (4, 4);

	map.replace_meter (map.first_meter(), meter, BBT_Time (1, 1, 0), (samplepos_t) 0, AudioTime);
	map.replace_tempo (map.first_tempo(), tempo, 0.0, 0, TempoSection::Constant, AudioTime);

	/* Subtract 1 beat from beat 3 of the first bar */
	samplepos_t r = map.samplepos_minus_qn (samples_per_beat * 2, Beats(1));
	CPPUNIT_ASSERT_EQUAL (r, samplepos_t (samples_per_beat * 1));

	/* Subtract 4 beats from 3 beats in, to go beyond zero */
	r = map.samplepos_minus_qn (samples_per_beat * 3, Beats(4));
	CPPUNIT_ASSERT_EQUAL (r, samplepos_t (- samples_per_beat));
}

/* Test adding things that overlap a tempo change */
void
FrameposMinusBeatsTest::doubleTempoTest ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meter (4, 4);
	map.replace_meter (map.first_meter(), meter, BBT_Time (1, 1, 0), (samplepos_t) 0, AudioTime);

	/*
	  120bpm at bar 1, 240bpm at bar 4

	  120bpm = 24e3 samples per beat
	  240bpm = 12e3 samples per beat
	*/


	/*

	  120bpm                                                240bpm
	  0 beats                                               12 beats
	  0 samples                                              288e3 samples
	  0 pulses                                              4 pulses
	  |                 |                 |                 |                 |
	  | 1.1 1.2 1.3 1.4 | 2.1 2.2 2.3.2.4 | 3.1 3.2 3.3 3.4 | 4.1 4.2 4.3 4.4 |

	*/

	Tempo tempoA (120);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, TempoSection::Constant, AudioTime);
	Tempo tempoB (240);
	map.add_tempo (tempoB, 12.0 / tempoA.note_type(), 0, TempoSection::Constant, MusicTime);

	/* Now some tests */

	/* Subtract 1 beat from 1|2 */
	samplepos_t r = map.samplepos_minus_qn (24e3, Beats(1));
	CPPUNIT_ASSERT_EQUAL (r, samplepos_t (0));

	/* Subtract 2 beats from 4|2 (over the tempo change) */
	r = map.samplepos_minus_qn (288e3 + 12e3, Beats(2));
	CPPUNIT_ASSERT_EQUAL (r, samplepos_t (288e3 - 24e3));

	/* Subtract 2.5 beats from 4|2 (over the tempo change) */
	r = map.samplepos_minus_qn (288e3 + 12e3, Beats(2.5));
	CPPUNIT_ASSERT_EQUAL (r, samplepos_t (288e3 - 24e3 - 12e3));
}

/* Same as doubleTempoTest () except put a meter change at the same time as the
   tempo change (which shouldn't affect anything, since we are just dealing with
   beats)
*/

void
FrameposMinusBeatsTest::doubleTempoWithMeterTest ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meterA (4, 4);
	map.replace_meter (map.first_meter(), meterA, BBT_Time (1, 1, 0), (samplepos_t) 0, AudioTime);

	/*
	  120bpm at bar 1, 240bpm at bar 4

	  120bpm = 24e3 samples per beat
	  240bpm = 12e3 samples per beat
	*/


	/*

	  120bpm                                                240bpm
	  0 beats                                               12 beats
	  0 samples                                              288e3 samples
	  0 pulses                                              3 pulses
	  |                 |                 |                 |             |
	  | 1.1 1.2 1.3 1.4 | 2.1 2.2 2.3.2.4 | 3.1 3.2 3.3 3.4 | 4.1 4.2 4.3 |

	*/

	Tempo tempoA (120);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, TempoSection::Constant, AudioTime);
	Tempo tempoB (240);
	map.add_tempo (tempoB, 12.0 / tempoA.note_type(), 0, TempoSection::Constant, MusicTime);
	Meter meterB (3, 4);
	map.add_meter (meterB, 12.0, BBT_Time (4, 1, 0), 0, MusicTime);

	/* Now some tests */

	/* Subtract 1 beat from 1|2 */
	samplepos_t r = map.samplepos_minus_qn (24e3, Beats(1));
	CPPUNIT_ASSERT_EQUAL (r, samplepos_t (0));

	/* Subtract 2 beats from 4|2 (over the tempo change) */
	r = map.samplepos_minus_qn (288e3 + 12e3, Beats(2));
	CPPUNIT_ASSERT_EQUAL (r, samplepos_t (288e3 - 24e3));

	/* Subtract 2.5 beats from 4|2 (over the tempo change) */
	r = map.samplepos_minus_qn (288e3 + 12e3, Beats(2.5));
	CPPUNIT_ASSERT_EQUAL (r, samplepos_t (288e3 - 24e3 - 12e3));
}


