#include "ardour/tempo.h"
#include "tempo_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (TempoTest);

using namespace std;
using namespace ARDOUR;
using namespace Timecode;

void
TempoTest::recomputeMapTest48 ()
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

	Tempo tempoA (120.0, 4.0);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, AudioTime);
	Tempo tempoB (240.0, 4.0);
	map.add_tempo (tempoB, 3.0, 0, MusicTime);
	Meter meterB (3, 4);
	map.add_meter (meterB, BBT_Time (4, 1, 0), 0, MusicTime);
	//map.dump (map._metrics, std::cout);
	list<MetricSection*>::iterator i = map._metrics.begin();
	CPPUNIT_ASSERT_EQUAL (samplepos_t (0), (*i)->sample ());
	i = map._metrics.end();
	--i;
	CPPUNIT_ASSERT_EQUAL (samplepos_t (288e3), (*i)->sample ());

	/* check the tempo section for expected result (no map) */
	const TempoSection& tsa (map.tempo_section_at_sample (0));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1, tsa.minute_at_pulse (3.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1 / 2.0, tsa.minute_at_pulse (1.5), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1 / 3.0, tsa.minute_at_pulse (1.0), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (3.0, tsa.pulse_at_minute (0.1), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.5, tsa.pulse_at_minute (0.1 / 2.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, tsa.pulse_at_minute (0.1 / 3.0), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, tsa.minute_at_sample (60.0 * sampling_rate), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1, tsa.minute_at_ntpm (240.0, 3.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1, tsa.minute_at_ntpm (240.0, 3.0), 1e-17);

	/* do the same via the map */

	/* quarter note */

	/* quarter note - sample*/
	CPPUNIT_ASSERT_EQUAL (samplepos_t (288e3), map.sample_at_quarter_note (12.0));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (144e3), map.sample_at_quarter_note (6.0));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (96e3), map.sample_at_quarter_note (4.0));

	/* sample - quarter note*/
	CPPUNIT_ASSERT_DOUBLES_EQUAL (12.0, map.quarters_at_sample (288e3), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (6.0, map.quarters_at_sample (144e3), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (4.0, map.quarters_at_sample (96e3), 1e-17);

	/* pulse - internal minute based interface */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1, map.minute_at_pulse_locked (map._metrics, 3.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (3.0, map.pulse_at_minute_locked (map._metrics, 0.1), 1e-17);

	/* tempo */

	/* tempo - sample */
	CPPUNIT_ASSERT_EQUAL (samplepos_t (288e3), map.sample_at_tempo (tempoB));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (240.0, map.tempo_at_sample (288e3).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (120.0, map.tempo_at_sample (288e3 - 1).note_types_per_minute(), 1e-17);

	/* tempo - quarter note */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (240.0, map.tempo_at_quarter_note (24.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (240.0, map.tempo_at_quarter_note (12.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (120.0, map.tempo_at_quarter_note (6.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (120.0, map.tempo_at_quarter_note (0.0).note_types_per_minute(), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (12.0, map.quarters_at_tempo (tempoB), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, map.quarters_at_tempo (tempoA), 1e-17);

	/* tempo - internal minute interface  */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (240.0, map.tempo_at_minute_locked (map._metrics, 0.1).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1, map.minute_at_tempo_locked (map._metrics, tempoB), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (240.0, map.tempo_at_pulse_locked (map._metrics, 3.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (3.0, map.pulse_at_tempo_locked (map._metrics, tempoB), 1e-17);
}

void
TempoTest::recomputeMapTest44 ()
{
	int const sampling_rate = 44100;

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

	Tempo tempoA (120.0, 4.0);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, AudioTime);
	Tempo tempoB (240.0, 4.0);
	map.add_tempo (tempoB, 3.0, 0, MusicTime);
	Meter meterB (3, 4);
	map.add_meter (meterB, BBT_Time (4, 1, 0), 288e3, MusicTime);

	list<MetricSection*>::iterator i = map._metrics.begin();
	CPPUNIT_ASSERT_EQUAL (samplepos_t (0), (*i)->sample ());
	i = map._metrics.end();
	--i;
	CPPUNIT_ASSERT_EQUAL (samplepos_t (264600), (*i)->sample ());

	/* check the tempo section for expected result (no map) */
	const TempoSection& tsa (map.tempo_section_at_sample (0));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1, tsa.minute_at_pulse (3.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1 / 2.0, tsa.minute_at_pulse (1.5), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1 / 3.0, tsa.minute_at_pulse (1.0), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (3.0, tsa.pulse_at_minute (0.1), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.5, tsa.pulse_at_minute (0.1 / 2.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, tsa.pulse_at_minute (0.1 / 3.0), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, tsa.minute_at_sample (60.0 * sampling_rate), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1, tsa.minute_at_ntpm (240.0, 3.0), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1, tsa.minute_at_pulse (3.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (3.0, tsa.pulse_at_minute (0.1), 1e-17);

	/* do the same via the map */

	/* quarter note */

	/* quarter note - sample */
	CPPUNIT_ASSERT_EQUAL (samplepos_t (264600), map.sample_at_quarter_note (12.0));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (132300), map.sample_at_quarter_note (6.0));
	CPPUNIT_ASSERT_EQUAL (samplepos_t (88200), map.sample_at_quarter_note (4.0));

	/* sample - quarter note */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (3.0 * 4.0, map.quarters_at_sample (264600), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.5 * 4.0, map.quarters_at_sample (132300), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0 * 4.0, map.quarters_at_sample (88200), 1e-17);

	/* pulse - internal minute based interface */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1, map.minute_at_pulse_locked (map._metrics, 3.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (3.0, map.pulse_at_minute_locked (map._metrics, 0.1), 1e-17);

	/* tempo */

	/* tempo - sample */
	CPPUNIT_ASSERT_EQUAL (samplepos_t (264600), map.sample_at_tempo (tempoB));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (240.0, map.tempo_at_sample (264600).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (120.0, map.tempo_at_sample (264600 - 1).note_types_per_minute(), 1e-17);

	/* tempo - quarter note */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (240.0, map.tempo_at_quarter_note (24.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (240.0, map.tempo_at_quarter_note (12.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (120.0, map.tempo_at_quarter_note (6.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (120.0, map.tempo_at_quarter_note (0.0).note_types_per_minute(), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (12.0, map.quarters_at_tempo (tempoB), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, map.quarters_at_tempo (tempoA), 1e-17);

	/* tempo - internal minute interface  */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (240.0, map.tempo_at_minute_locked (map._metrics, 0.1).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1, map.minute_at_tempo_locked (map._metrics, tempoB), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (240.0, map.tempo_at_pulse_locked (map._metrics, 3.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (3.0, map.pulse_at_tempo_locked (map._metrics, tempoB), 1e-17);
}

void
TempoTest::qnDistanceTestConstant ()
{
	int const sampling_rate = 44100;

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

	Tempo tempoA (120.0, 4.0);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, AudioTime);
	/* should have no effect on pulse */
	Tempo tempoB (120.0, 4.0);
	map.add_tempo (tempoB, 2.0, 0, MusicTime);
	/* equivalent to pulse 3.0 @ 120 bpm*/
	Tempo tempoC (240.0, 4.0);
	map.add_tempo (tempoC, 0.0, 6 * sampling_rate, AudioTime);
	Tempo tempoD (90.4, 4.0);
	map.add_tempo (tempoD, 9.0, 0, MusicTime);
	Tempo tempoE (110.6, 4.0);
	map.add_tempo (tempoE, 12.0, 0, MusicTime);
	Tempo tempoF (123.7, 4.0);
	map.add_tempo (tempoF, 15.0, 0, MusicTime);
	Tempo tempoG (111.8, 4.0);
	map.add_tempo (tempoG, 0.0, (samplepos_t) 2 * 60 * sampling_rate, AudioTime);

	Meter meterB (3, 4);
	map.add_meter (meterB, BBT_Time (4, 1, 0), 288e3, MusicTime);

	list<MetricSection*>::iterator i = map._metrics.begin();
	CPPUNIT_ASSERT_EQUAL (samplepos_t (0), (*i)->sample ());
	i = map._metrics.end();
	--i;
	CPPUNIT_ASSERT_EQUAL ((*i)->sample(), map.samples_between_quarter_notes (0.0, (*i)->pulse() * 4.0));

	--i;
	/* tempoF */
	CPPUNIT_ASSERT_EQUAL ((*i)->sample(), map.samples_between_quarter_notes (0.0, 15.0 * 4.0));
	CPPUNIT_ASSERT_DOUBLES_EQUAL ((*i)->minute(), map.minutes_between_quarter_notes_locked (map._metrics, 0.0, 15.0 * 4.0), 1e-17);

	--i;
	/* tempoE */
	CPPUNIT_ASSERT_EQUAL ((*i)->sample(), map.samples_between_quarter_notes (0.0, 12.0 * 4.0));
	CPPUNIT_ASSERT_DOUBLES_EQUAL ((*i)->minute(), map.minutes_between_quarter_notes_locked (map._metrics, 0.0, 12.0 * 4.0), 1e-17);

	--i;
	CPPUNIT_ASSERT_EQUAL ((*i)->sample(), map.samples_between_quarter_notes (0.0, 9.0 * 4.0));
	CPPUNIT_ASSERT_DOUBLES_EQUAL ((*i)->minute(), map.minutes_between_quarter_notes_locked (map._metrics, 0.0, 9.0 * 4.0), 1e-17);

	--i;
	/* tempoC */
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (6 * sampling_rate), map.samples_between_quarter_notes (0.0, (*i)->pulse() * 4.0));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1, map.minutes_between_quarter_notes_locked (map._metrics, 0.0, (*i)->pulse() * 4.0), 1e-17);

	/* distance from beat 12.0 to 0.0 should be 6.0 seconds */
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (264600), map.samples_between_quarter_notes (0.0, 3.0 * 4.0));
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (-264600), map.samples_between_quarter_notes (3.0 * 4.0, 0.0));
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (396900), map.samples_between_quarter_notes (0.0, 24.0));
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (-396900), map.samples_between_quarter_notes (24.0, 0.0));
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (88200), map.samples_between_quarter_notes (2.0 * 4.0, 3.0 * 4.0));
}
void
TempoTest::qnDistanceTestRamp ()
{
	int const sampling_rate = 44100;

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
	  0 samples                         288e3 samples
	  0 pulses                                              3 pulses
	  |                 |              |                 |                 |             |
	  | 1.1 1.2 1.3 1.4 |  -no music-  | 2.1 2.2 2.3.2.4 | 3.1 3.2 3.3 3.4 | 4.1 4.2 4.3 |

	*/

	Tempo tempoA (120.2, 4.0, 240.5);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, AudioTime);
	Tempo tempoB (240.5, 4.0, 130.1);
	map.add_tempo (tempoB, 3.0, 0, MusicTime);

	Tempo tempoC (130.1, 4.0, 90.3);
	map.add_tempo (tempoC, 0.0, 6 * sampling_rate, AudioTime);
	Tempo tempoD (90.3, 4.0, 110.7);
	map.add_tempo (tempoD, 9.0, 0, MusicTime);
	Tempo tempoE (110.7, 4.0, 123.9);
	map.add_tempo (tempoE, 12.0, 0, MusicTime);
	Tempo tempoF (123.9, 4.0, 111.8);
	map.add_tempo (tempoF, 15.0, 0, MusicTime);
	Tempo tempoG (111.8, 4.0);
	map.add_tempo (tempoG, 0.0, (samplepos_t) 2 * 60 * sampling_rate, AudioTime);
	Meter meterB (3, 4);
	map.add_meter (meterB, BBT_Time (2, 1, 0), 288e3, AudioTime);
	map.recompute_map (map._metrics, 1);

	list<MetricSection*>::iterator i = map._metrics.begin();
	CPPUNIT_ASSERT_EQUAL (samplepos_t (0), (*i)->sample ());
	i = map._metrics.end();
	--i;
	/* tempoG */
	CPPUNIT_ASSERT_EQUAL ((*i)->sample(), map.samples_between_quarter_notes (0.0, (*i)->pulse() * 4.0));
	CPPUNIT_ASSERT_DOUBLES_EQUAL ((*i)->minute(), map.minutes_between_quarter_notes_locked (map._metrics, 0.0, (*i)->pulse() * 4.0), 1e-17);

	--i;
	CPPUNIT_ASSERT_EQUAL ((*i)->sample(), map.samples_between_quarter_notes (0.0, 60.0));
	CPPUNIT_ASSERT_DOUBLES_EQUAL ((*i)->minute(), map.minutes_between_quarter_notes_locked (map._metrics, 0.0, 60.0), 1e-17);

	--i;
	/* tempoE */
	CPPUNIT_ASSERT_EQUAL ((*i)->sample(), map.samples_between_quarter_notes (0.0, 48.0));
	CPPUNIT_ASSERT_DOUBLES_EQUAL ((*i)->minute(), map.minutes_between_quarter_notes_locked (map._metrics, 0.0, 48.0), 1e-17);

	--i;
	CPPUNIT_ASSERT_EQUAL ((*i)->sample(), map.samples_between_quarter_notes (0.0, 36.0));
	CPPUNIT_ASSERT_DOUBLES_EQUAL ((*i)->minute(), map.minutes_between_quarter_notes_locked (map._metrics, 0.0, 36.0), 1e-17);

	--i;
	/* tempoC */
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (6 * sampling_rate), map.samples_between_quarter_notes (0.0, (*i)->pulse() * 4.0));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.1, map.minutes_between_quarter_notes_locked (map._metrics, 0.0, (*i)->pulse() * 4.0), 1e-17);

}

void
TempoTest::rampTest48 ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meterA (4, 4);
	Tempo tempoA (77.0, 4.0, 217.0);
	Tempo tempoB (217.0, 4.0);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, AudioTime);
	map.add_tempo (tempoB, 0.0, (samplepos_t) 60 * sampling_rate, AudioTime);
	map.replace_meter (map.first_meter(), meterA, BBT_Time (1, 1, 0), 0, AudioTime);

	/*

	  77bpm                                                 217bpm
	  0 samples                                              60 * sample rate samples
	  |                 |                 |                 |             |
	  |                                                    *|
	  |                                                  *  |
	  |                                                *    |
	  |                                             *       |
	  |                                          *          |
	  |                                      *              |
	  |                                 *                   |
	  |                           *  |                      |
	  |                  *           |                      |
	  |     *            |           |                      |
	  -------------------|-----------|-----------------------
                             20 seconds  125.0 bpm / note_type
	*/

	TempoSection& tA = map.first_tempo();
	const TempoSection& tB = map.tempo_section_at_sample ((samplepos_t) 60 * sampling_rate);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, tA.minute_at_ntpm (217.0, 300.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (217.0, tA.tempo_at_minute (1.0).note_types_per_minute(), 1e-17);

	/* note 1e-14 here. pulse is two derivatives away from time */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB.pulse(), tA.pulse_at_minute (1.0), 1e-14);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, tA.minute_at_pulse (tB.pulse()), 1e-15);

	/* note 1e-17 here. tempo is one derivative away from pulse, so we can get the same stuff with more precision */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB.pulse(), tA.pulse_at_ntpm (217.0, 1.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, tA.minute_at_ntpm (217.0, tB.pulse()), 1e-17);

	/* self-check tempo at pulse @ 125 bpm. */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (125.0, tA.tempo_at_pulse (tA.pulse_at_ntpm (125.0, 0)).note_types_per_minute(), 1e-17);

	/* check that tB's pulse is what tA thinks it should be */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB.pulse(), tA.pulse_at_ntpm (217.0, 0), 1e-17);

	/* check that the tempo at the halfway mark (in pulses) is half the tempo delta.*/
	CPPUNIT_ASSERT_DOUBLES_EQUAL (147.0, tA.tempo_at_pulse (tB.pulse() / 2.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL ((tB.pulse() - tA.pulse()) / 2.0, tA.pulse_at_ntpm (147.0, 0), 1e-17);

	/* self-check sample at pulse 20 seconds in. */
	const double target = 20.0 / 60.0;
	const double result = tA.minute_at_pulse (tA.pulse_at_minute (target));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (target, result, 1e-14);
}

void
TempoTest::rampTest44 ()
{
	int const sampling_rate = 44100;

	TempoMap map (sampling_rate);
	Meter meterA (4, 4);
	Tempo tempoA (77.0, 4.0, 217.0);
	Tempo tempoB (217.0, 4.0);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, AudioTime);
	map.add_tempo (tempoB, 0.0, (samplepos_t) 60 * sampling_rate, AudioTime);
	map.replace_meter (map.first_meter(), meterA, BBT_Time (1, 1, 0), 0, AudioTime);

	/*

	  77bpm                                                 217bpm
	  0 samples                                              60 * sample rate samples
	  |                 |                 |                 |             |
	  |                                                    *|
	  |                                                  *  |
	  |                                                *    |
	  |                                             *       |
	  |                                          *          |
	  |                                      *              |
	  |                                 *                   |
	  |                           *  |                      |
	  |                  *           |                      |
	  |     *            |           |                      |
	  -------------------|-----------|-----------------------
                             20 seconds  125.0 bpm / note_type
	*/

	TempoSection& tA = map.first_tempo();
	const TempoSection& tB = map.tempo_section_at_sample ((samplepos_t) 60 * sampling_rate);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, tA.minute_at_ntpm (217.0, 300.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (217.0, tA.tempo_at_minute (1.0).note_types_per_minute(), 1e-17);

	/* note 1e-14 here. pulse is two derivatives away from time */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB.pulse(), tA.pulse_at_minute (1.0), 1e-14);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, tA.minute_at_pulse (tB.pulse()), 1e-15);

	/* note 1e-17 here. tempo is one derivative away from pulse, so we can get the same stuff with more precision */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB.pulse(), tA.pulse_at_ntpm (217.0, 1.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, tA.minute_at_ntpm (217.0, tB.pulse()), 1e-17);

	/* self-check tempo at pulse @ 125 bpm. */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (125.0, tA.tempo_at_pulse (tA.pulse_at_ntpm (125.0, 0)).note_types_per_minute(), 1e-17);

	/* check that tB's pulse is what tA thinks it should be */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB.pulse(), tA.pulse_at_ntpm (217.0, 0), 1e-17);

	/* check that the tempo at the halfway mark (in pulses) is half the tempo delta.*/
	CPPUNIT_ASSERT_DOUBLES_EQUAL (147.0, tA.tempo_at_pulse (tB.pulse() / 2.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL ((tB.pulse() - tA.pulse()) / 2.0, tA.pulse_at_ntpm (147.0, 0), 1e-17);

	/* self-check sample at pulse 20 seconds in. */
	const double target = 20.0 / 60.0;
	const double result = tA.minute_at_pulse (tA.pulse_at_minute (target));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (target, result, 1e-14);
}

void
TempoTest::tempoAtPulseTest ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meterA (4, 8);
	Tempo tempoA (80.0, 8.0, 160.0);
	Tempo tempoB (160.0, 3.0, 123.0);
	Tempo tempoC (123.0, 4.0);

	map.replace_meter (map.first_meter(), meterA, BBT_Time (1, 1, 0), 0, AudioTime);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, AudioTime);

	map.add_tempo (tempoB, 20.0, 0, MusicTime);
	map.add_tempo (tempoC, 30.0, 0, MusicTime);

	TempoSection* tA = 0;
	TempoSection* tB = 0;
	TempoSection* tC = 0;

	list<MetricSection*>::iterator i;

	for (i = map._metrics.begin(); i != map._metrics.end(); ++i) {
		TempoSection* t;
		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
			if (!tA) {
				tA = t;
				continue;
			}
			if (!tB) {
				tB = t;
				continue;
			}
			if (!tC) {
				tC = t;
				continue;
			}
		}
	}

	CPPUNIT_ASSERT_DOUBLES_EQUAL (160.0, tA->tempo_at_pulse (20.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (123.0, tB->tempo_at_pulse (30.0).note_types_per_minute(), 1e-17);

	/* check that the tempo at the halfway mark (in pulses) is half the tempo delta.*/
	CPPUNIT_ASSERT_DOUBLES_EQUAL (((80.0 - 160.0)  / 2.0) + 160.0, tA->tempo_at_pulse (10.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (20.0 / 2.0, tA->pulse_at_ntpm (120.0, 0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (((160.0 - 123.0)  / 2.0) + 123.0, tB->tempo_at_pulse (25.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (((20.0 - 30.0)  / 2.0) + 30.0, tB->pulse_at_ntpm (141.5, 0), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB->minute(), tA->minute_at_pulse (20.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tC->minute(), tB->minute_at_pulse (30.0), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB->minute(), tA->minute_at_ntpm (160.0, 20.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tC->minute(), tB->minute_at_ntpm (123.0, 30.0), 1e-17);

	/* self-check tempo at pulse @ 125 bpm. */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (125.0, tA->tempo_at_pulse (tA->pulse_at_ntpm (125.0, 0)).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (160.0, tA->tempo_at_pulse (20.0).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (123.0, tB->tempo_at_pulse (30.0).note_types_per_minute(), 1e-17);
	/* test minute based measurements */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB->minute(), tA->minute_at_pulse (20.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tC->minute(), tB->minute_at_pulse (30.0), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB->minute(), tA->minute_at_ntpm (160.0, 20.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tC->minute(), tB->minute_at_ntpm (123.0, 30.0), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (160.0, tA->tempo_at_minute (tB->minute()).note_types_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (123.0, tB->tempo_at_minute (tC->minute()).note_types_per_minute(), 1e-17);
}

void
TempoTest::tempoFundamentalsTest ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meterA (4, 8);
	Tempo tempoA (120.0, 4.0);
	Tempo tempoB (120.0, 8.0);
	Tempo tempoC (120.0, 2.0);
	Tempo tempoD (160.0, 2.0);
	Tempo tempoE (123.0, 3.0);

	map.replace_meter (map.first_meter(), meterA, BBT_Time (1, 1, 0), 0, AudioTime);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, AudioTime);

	map.add_tempo (tempoB, 20.0, 0, MusicTime);
	map.add_tempo (tempoC, 30.0, 0, MusicTime);

	map.add_tempo (tempoD, 40.0, 0, MusicTime);
	map.add_tempo (tempoE, 50.0, 0, MusicTime);

	TempoSection* tA = 0;
	TempoSection* tB = 0;
	TempoSection* tC = 0;
	TempoSection* tD = 0;
	TempoSection* tE = 0;
	list<MetricSection*>::iterator i;

	for (i = map._metrics.begin(); i != map._metrics.end(); ++i) {
		TempoSection* t;
		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
			if (!tA) {
				tA = t;
				continue;
			}
			if (!tB) {
				tB = t;
				continue;
			}
			if (!tC) {
				tC = t;
				continue;
			}
			if (!tD) {
				tD = t;
				continue;
			}
			if (!tE) {
				tE = t;
				continue;
			}
		}
	}

	CPPUNIT_ASSERT_DOUBLES_EQUAL (24000.0, tA->samples_per_quarter_note (sampling_rate), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (24000.0, tA->samples_per_note_type (sampling_rate), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (120.0, tA->quarter_notes_per_minute (), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (30.0, tA->pulses_per_minute (), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (48000.0, tB->samples_per_quarter_note (sampling_rate), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (24000.0, tB->samples_per_note_type (sampling_rate), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (60.0, tB->quarter_notes_per_minute (), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (15.0, tB->pulses_per_minute (), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (12000.0, tC->samples_per_quarter_note (sampling_rate), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (24000.0, tC->samples_per_note_type (sampling_rate), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (240.0, tC->quarter_notes_per_minute (), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (60.0, tC->pulses_per_minute (), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (9000.0, tD->samples_per_quarter_note (sampling_rate), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (18000.0, tD->samples_per_note_type (sampling_rate), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (320.0, tD->quarter_notes_per_minute (), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (80.0, tD->pulses_per_minute (), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (17560.975609756097, tE->samples_per_quarter_note (sampling_rate), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (23414.634146341465, tE->samples_per_note_type (sampling_rate), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (164.0, tE->quarter_notes_per_minute (), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (41.0, tE->pulses_per_minute (), 1e-17);
}
