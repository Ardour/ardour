#include "samplepos_plus_beats_test.h"
#include "ardour/tempo.h"
#include "temporal/bbt_time.h"

CPPUNIT_TEST_SUITE_REGISTRATION (FrameposPlusBeatsTest);

using namespace std;
using namespace ARDOUR;
using namespace Timecode;

/* Basic tests with no tempo / meter changes */
void
FrameposPlusBeatsTest::singleTempoTest ()
{
	int const sampling_rate = 48000;
	int const bpm = 120;

	double const samples_per_beat = (60 / double (bpm)) * double (sampling_rate);

	TempoMap map (sampling_rate);
	Tempo tempo (bpm, 4.0);
	Meter meter (4, 4);

	map.replace_meter (map.first_meter(), meter, BBT_Time (1, 1, 0), 0, AudioTime);
	map.replace_tempo (map.first_tempo(), tempo, 0.0, 0, AudioTime);

	/* Add 1 beat to beat 3 of the first bar */
	samplepos_t r = map.samplepos_plus_qn (samples_per_beat * 2, Temporal::Beats(1));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (samples_per_beat * 3), r);

	/* Add 4 beats to a -ve sample of 1 beat before zero */
	r = map.samplepos_plus_qn (-samples_per_beat * 1, Temporal::Beats(4));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (samples_per_beat * 3), r);
}

/* Test adding things that overlap a tempo change */
void
FrameposPlusBeatsTest::doubleTempoTest ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meter (4, 4);
	map.replace_meter (map.first_meter(), meter, BBT_Time (1, 1, 0), 0, AudioTime);

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
	  |                 |                 |                 |                 |
	  | 1.1 1.2 1.3 1.4 | 2.1 2.2 2.3.2.4 | 3.1 3.2 3.3 3.4 | 4.1 4.2 4.3 4.4 |

	*/

	Tempo tempoA (120, 4.0);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, AudioTime);
	Tempo tempoB (240, 4.0);
	map.add_tempo (tempoB, 12.0 / tempoA.note_type(), 0, MusicTime);

	/* Now some tests */

	/* Add 1 beat to 1|2 */
	samplepos_t r = map.samplepos_plus_qn (24e3, Temporal::Beats(1));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (48e3), r);

	/* Add 2 beats to 3|4 (over the tempo change) */
	r = map.samplepos_plus_qn (264e3, Temporal::Beats(2));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (264e3 + 24e3 + 12e3), r);

	/* Add 2.5 beats to 3|3|960 (over the tempo change) */
	r = map.samplepos_plus_qn (264e3 - 12e3, Temporal::Beats(2.5));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (264e3 + 24e3 + 12e3), r);
}

/* Same as doubleTempoTest () except put a meter change at the same time as the
   tempo change (which shouldn't affect anything, since we are just dealing with
   beats)
*/

void
FrameposPlusBeatsTest::doubleTempoWithMeterTest ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meterA (4, 4);
	map.replace_meter (map.first_meter(), meterA, BBT_Time (1, 1, 0), 0, AudioTime);

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

	Tempo tempoA (120, 4.0);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, AudioTime);
	Tempo tempoB (240, 4.0);
	map.add_tempo (tempoB, 12.0 / tempoA.note_type(), 0, MusicTime);
	Meter meterB (3, 8);
	map.add_meter (meterB, BBT_Time (4, 1, 0), 0, MusicTime);

	/* Now some tests */

	/* Add 1 beat to 1|2 */
	samplepos_t r = map.samplepos_plus_qn (24e3, Temporal::Beats(1));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (48e3), r);

	/* Add 2 beats to 3|4 (over the tempo change) */
	r = map.samplepos_plus_qn (264e3, Temporal::Beats(2));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (264e3 + 24e3 + 12e3), r);

	/* Add 2.5 beats to 3|3|960 (over the tempo change) */
	r = map.samplepos_plus_qn (264e3 - 12e3, Temporal::Beats(2.5));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (264e3 + 24e3 + 12e3), r);
}

/* Same as doubleTempoWithMeterTest () except use odd meter divisors
   (which shouldn't affect anything, since we are just dealing with
   beats)
*/

void
FrameposPlusBeatsTest::doubleTempoWithComplexMeterTest ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meterA (3, 4);
	map.replace_meter (map.first_meter(), meterA, BBT_Time (1, 1, 0), 0, AudioTime);

	/*
	  120bpm at bar 1, 240bpm at bar 4

	  120bpm = 24e3 samples per beat
	  240bpm = 12e3 samples per beat
	*/


	/*

	  120bpm                                    5/8                    240bpm
	  0 beats                                   9 quarter note beats   12 quarter note beats
	                                            9 meter-based beat     15 meter-based beat
	  0 samples                                                         288e3 samples
	  0 pulses                                  |                      3 pulses
	  |             |             |             |                      |
	  | 1.1 1.2 1.3 | 2.1 2.2 2.3 | 3.1 3.2 3.3 |4.14.24.34.44.5|5.15.2^5.35.45.5|
	                                            |
						    4|1|0
	*/

	Tempo tempoA (120, 4.0);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, AudioTime);
	Tempo tempoB (240, 4.0);
	map.add_tempo (tempoB, 12.0 / 4.0, 0, MusicTime);
	Meter meterB (5, 8);
	map.add_meter (meterB, BBT_Time (4, 1, 0), 0, MusicTime);
	/* Now some tests */

	/* Add 1 beat to 1|2 */
	samplepos_t r = map.samplepos_plus_qn (24e3, Temporal::Beats(1));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (48e3), r);

	/* Add 2 beats to 5|1 (over the tempo change) */
	r = map.samplepos_plus_qn (264e3, Temporal::Beats(2));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (264e3 + 24e3 + 12e3), r);

	/* Add 2.5 beats to 4|5 (over the tempo change) */
	r = map.samplepos_plus_qn (264e3 - 12e3, Temporal::Beats(2.5));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (264e3 + 24e3 + 12e3), r);
}


