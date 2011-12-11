#include "framepos_minus_beats_test.h"
#include "ardour/tempo.h"
#include "timecode/bbt_time.h"

CPPUNIT_TEST_SUITE_REGISTRATION (FrameposMinusBeatsTest);

using namespace std;
using namespace ARDOUR;
using namespace Timecode;

/* Basic tests with no tempo / meter changes */
void
FrameposMinusBeatsTest::singleTempoTest ()
{
	int const sampling_rate = 48000;
	int const bpm = 120;

	double const frames_per_beat = (60 / double (bpm)) * double (sampling_rate);
	
	TempoMap map (sampling_rate);
	Tempo tempo (bpm);
	Meter meter (4, 4);

	map.add_meter (meter, BBT_Time (1, 1, 0));
	map.add_tempo (tempo, BBT_Time (1, 1, 0));

	/* Subtract 1 beat from beat 3 of the first bar */
	framepos_t r = map.framepos_minus_beats (frames_per_beat * 2, 1);
	CPPUNIT_ASSERT_EQUAL (r, framepos_t (frames_per_beat * 1));

	/* Subtract 4 beats from 3 beats in, to go beyond zero */
	r = map.framepos_minus_beats (frames_per_beat * 3, 4);
	CPPUNIT_ASSERT_EQUAL (r, framepos_t (- frames_per_beat));
}

/* Test adding things that overlap a tempo change */
void
FrameposMinusBeatsTest::doubleTempoTest ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meter (4, 4);
	map.add_meter (meter, BBT_Time (1, 1, 0));

	/*
	  120bpm at bar 1, 240bpm at bar 4
	  
	  120bpm = 24e3 samples per beat
	  240bpm = 12e3 samples per beat
	*/
	

	/*
	  
	  120bpm                                                240bpm
	  0 beats                                               12 beats
	  0 frames                                              288e3 frames
	  |                 |                 |                 |                 |
	  | 1.1 1.2 1.3 1.4 | 2.1 2.2 2.3.2.4 | 3.1 3.2 3.3 3.4 | 4.1 4.2 4.3 4.4 |

	*/

	Tempo tempoA (120);
	map.add_tempo (tempoA, BBT_Time (1, 1, 0));
	Tempo tempoB (240);
	map.add_tempo (tempoB, BBT_Time (4, 1, 0));

	/* Now some tests */

	/* Subtract 1 beat from 1|2 */
	framepos_t r = map.framepos_minus_beats (24e3, 1);
	CPPUNIT_ASSERT_EQUAL (r, framepos_t (0));

	/* Subtract 2 beats from 4|2 (over the tempo change) */
	r = map.framepos_minus_beats (288e3 + 12e3, 2);
	CPPUNIT_ASSERT_EQUAL (r, framepos_t (288e3 - 24e3));

	/* Subtract 2.5 beats from 4|2 (over the tempo change) */
	r = map.framepos_minus_beats (288e3 + 12e3, 2.5);
	CPPUNIT_ASSERT_EQUAL (r, framepos_t (288e3 - 24e3 - 12e3));
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

	/* Now some tests */

	/* Subtract 1 beat from 1|2 */
	framepos_t r = map.framepos_minus_beats (24e3, 1);
	CPPUNIT_ASSERT_EQUAL (r, framepos_t (0));

	/* Subtract 2 beats from 4|2 (over the tempo change) */
	r = map.framepos_minus_beats (288e3 + 12e3, 2);
	CPPUNIT_ASSERT_EQUAL (r, framepos_t (288e3 - 24e3));

	/* Subtract 2.5 beats from 4|2 (over the tempo change) */
	r = map.framepos_minus_beats (288e3 + 12e3, 2.5);
	CPPUNIT_ASSERT_EQUAL (r, framepos_t (288e3 - 24e3 - 12e3));
}


