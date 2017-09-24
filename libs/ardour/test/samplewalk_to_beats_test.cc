#include "samplewalk_to_beats_test.h"
#include "ardour/tempo.h"
#include "temporal/bbt_time.h"

CPPUNIT_TEST_SUITE_REGISTRATION (FramewalkToBeatsTest);

using namespace std;
using namespace ARDOUR;
using namespace Timecode;

void
FramewalkToBeatsTest::singleTempoTest ()
{
	int const sampling_rate = 48000;
	int const bpm = 120;

	double const samples_per_beat = (60 / double (bpm)) * double (sampling_rate);

	TempoMap map (sampling_rate);
	Tempo tempo (bpm, 4.0);
	Meter meter (4, 4);

	map.replace_meter (map.meter_section_at_sample (0), meter, BBT_Time (1, 1, 0), 0, AudioTime);
	map.replace_tempo (map.tempo_section_at_sample (0), tempo, 0.0, 0, AudioTime);

	/* Walk 1 beats-worth of samples from beat 3 */
	double r = map.framewalk_to_qn (samples_per_beat * 2, samples_per_beat * 1).to_double();
	CPPUNIT_ASSERT_EQUAL (1.0, r);

	/* Walk 6 beats-worth of samples from beat 4 */
	r = map.framewalk_to_qn (samples_per_beat * 3, samples_per_beat * 6).to_double();
	CPPUNIT_ASSERT_EQUAL (6.0, r);

	/* Walk 1.5 beats-worth of samples from beat 3 */
	r = map.framewalk_to_qn (samples_per_beat * 2, samples_per_beat * 1.5).to_double();
	CPPUNIT_ASSERT_EQUAL (1.5, r);

	/* Walk 1.5 beats-worth of samples from beat 2.5 */
	r = map.framewalk_to_qn (samples_per_beat * 2.5, samples_per_beat * 1.5).to_double();
	CPPUNIT_ASSERT_EQUAL (1.5, r);
}

void
FramewalkToBeatsTest::doubleTempoTest ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meter (4, 4);
	map.replace_meter (map.meter_section_at_sample (0), meter, BBT_Time (1, 1, 0), 0, AudioTime);

	/*
	  120bpm at bar 1, 240bpm at bar 4

	  120bpm = 24e3 samples per beat
	  240bpm = 12e3 samples per beat
	*/


	/*

	  120bpm                                          240bpm
	  0 beats                                         12 beats
	  0 samples                                        288e3 samples
	  24e3 samples per beat                            12e3 samples per beat
	  0 pulses                                        4 pulses
	  |               |               |               |               |
	  1.1 1.2 1.3 1.4 2.1 2.2 2.3 2.4 3.1 3.2 3.3 3.4 4.1 4.2 4.3 4.4 5.1
	  0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15  16

	*/

	Tempo tempoA (120);
	map.replace_tempo (map.tempo_section_at_sample (0), tempoA, 0.0, 0, AudioTime);
	Tempo tempoB (240);
	map.add_tempo (tempoB, 12.0 / tempoB.note_type(), 0, MusicTime);

	/* Now some tests */

	/* Walk 1 beat from 1|2 */
	double r = map.framewalk_to_qn (24e3, 24e3).to_double();
	CPPUNIT_ASSERT_EQUAL (1.0, r);

	/* Walk 2 beats from 3|3 to 4|1 (over the tempo change) */
	r = map.framewalk_to_qn (240e3, (24e3 + 24e3)).to_double();
	CPPUNIT_ASSERT_EQUAL (2.0, r);

	/* Walk 2.5 beats from 3|3.5 to 4.2 (over the tempo change) */
	r = map.framewalk_to_qn (264e3 - 12e3, (24e3 + 12e3 + 12e3)).to_double();
	CPPUNIT_ASSERT_EQUAL (2.5, r);
	/* Walk 3 beats from 3|4.5 to 4|3.5 (over the tempo change) */
	r = map.framewalk_to_qn (264e3 - 12e3, (24e3 + 12e3 + 12e3 + 6e3)).to_double();
	CPPUNIT_ASSERT_EQUAL (3.0, r);

	/* Walk 3.5 beats from 3|4.5 to 4.4 (over the tempo change) */
	r = map.framewalk_to_qn (264e3 - 12e3, (24e3 + 12e3 + 12e3 + 12e3)).to_double();
	CPPUNIT_ASSERT_EQUAL (3.5, r);
}

void
FramewalkToBeatsTest::tripleTempoTest ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meter (4, 4);
	map.replace_meter (map.meter_section_at_sample (0), meter, BBT_Time (1, 1, 0), 0, AudioTime);

	/*
	  120bpm at bar 1, 240bpm at bar 2, 160bpm at bar 3

	  120bpm = 24e3 samples per beat
	  160bpm = 18e3 samples per beat
	  240bpm = 12e3 samples per beat
	*/


	/*

	  120bpm            240bpm            160bpm
	  0 beats           4 beats           8 beats
	  0 samples          96e3 samples       144e3 samples
	  0 pulses          1 pulse           2 pulses
	  |                 |                 |                 |                 |
	  | 1.1 1.2 1.3 1.4 | 2.1 2.2 2.3.2.4 | 3.1 3.2 3.3 3.4 | 4.1 4.2 4.3 4.4 |

	*/

	Tempo tempoA (120, 4.0);
	map.replace_tempo (map.tempo_section_at_sample (0), tempoA, 0.0, 0, AudioTime);
	Tempo tempoB (240, 4.0);
	map.add_tempo (tempoB, 4.0 / tempoB.note_type(), 0, MusicTime);
	Tempo tempoC (160, 4.0);
	map.add_tempo (tempoC, 8.0 / tempoB.note_type(), 0, MusicTime);

	/* Walk from 1|3 to 4|1 */
	double r = map.framewalk_to_qn (2 * 24e3, (2 * 24e3) + (4 * 12e3) + (4 * 18e3)).to_double();
	CPPUNIT_ASSERT_EQUAL (10.0, r);
}

void
FramewalkToBeatsTest::singleTempoMeterTest ()
{
	int const sampling_rate = 48000;
	int const bpm = 120;

	double const samples_per_beat = (60 / double (bpm)) * double (sampling_rate);

	TempoMap map (sampling_rate);
	Tempo tempo (bpm, 4.0);
	Meter meter (7, 8);

	map.replace_meter (map.meter_section_at_sample (0), meter, BBT_Time (1, 1, 0), 0, AudioTime);
	map.replace_tempo (map.tempo_section_at_sample (0), tempo, 0.0, 0, AudioTime);

	/* Walk 1 qn beats-worth of samples from beat 3 */
	double r = map.framewalk_to_qn (samples_per_beat * 2, samples_per_beat * 1).to_double();
	CPPUNIT_ASSERT_EQUAL (1.0, r);

	/* Walk 6 qn beats-worth of samples from beat 4 */
	r = map.framewalk_to_qn (samples_per_beat * 3, samples_per_beat * 6).to_double();
	CPPUNIT_ASSERT_EQUAL (6.0, r);

	/* Walk 1.5 qn beats-worth of samples from beat 3 */
	r = map.framewalk_to_qn (samples_per_beat * 2, samples_per_beat * 1.5).to_double();
	CPPUNIT_ASSERT_EQUAL (1.5, r);

	/* Walk 1.5 qn beats-worth of samples from beat 2.5 */
	r = map.framewalk_to_qn (samples_per_beat * 2.5, samples_per_beat * 1.5).to_double();
	CPPUNIT_ASSERT_EQUAL (1.5, r);
}
