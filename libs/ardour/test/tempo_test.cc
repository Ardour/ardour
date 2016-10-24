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
	map.replace_meter (map.first_meter(), meterA, BBT_Time (1, 1, 0), (framepos_t) 0, AudioTime);

	/*
	  120bpm at bar 1, 240bpm at bar 4

	  120bpm = 24e3 samples per beat
	  240bpm = 12e3 samples per beat
	*/


	/*

	  120bpm                                                240bpm
	  0 beats                                               12 beats
	  0 frames                                              288e3 frames
	  0 pulses                                              3 pulses
	  |                 |                 |                 |             |
	  | 1.1 1.2 1.3 1.4 | 2.1 2.2 2.3.2.4 | 3.1 3.2 3.3 3.4 | 4.1 4.2 4.3 |

	*/

	Tempo tempoA (120);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, TempoSection::Constant, AudioTime);
	Tempo tempoB (240);
	map.add_tempo (tempoB, 3.0, 0, TempoSection::Constant, MusicTime);
	Meter meterB (3, 4);
	map.add_meter (meterB, 12.0, BBT_Time (4, 1, 0), 0, MusicTime);

	list<MetricSection*>::iterator i = map._metrics.begin();
	CPPUNIT_ASSERT_EQUAL (framepos_t (0), (*i)->frame ());

	/* check the tempo section for ecpected result (no map) */
	const TempoSection& tsa (map.tempo_section_at_frame (0));
	CPPUNIT_ASSERT_EQUAL (framepos_t (288e3), tsa.frame_at_pulse (3.0, sampling_rate));
	CPPUNIT_ASSERT_EQUAL (framepos_t (144e3), tsa.frame_at_pulse (1.5, sampling_rate));
	CPPUNIT_ASSERT_EQUAL (framepos_t (96e3), tsa.frame_at_pulse (1.0, sampling_rate));

	CPPUNIT_ASSERT_DOUBLES_EQUAL (3.0, tsa.pulse_at_frame (288e3, sampling_rate), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.5, tsa.pulse_at_frame (144e3, sampling_rate), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, tsa.pulse_at_frame (96e3, sampling_rate), 1e-17);

	/* do the same via the map */
	CPPUNIT_ASSERT_EQUAL (framepos_t (288e3), map.frame_at_pulse (3.0));
	CPPUNIT_ASSERT_EQUAL (framepos_t (144e3), map.frame_at_pulse (1.5));
	CPPUNIT_ASSERT_EQUAL (framepos_t (96e3), map.frame_at_pulse (1.0));

	CPPUNIT_ASSERT_DOUBLES_EQUAL (3.0, map.pulse_at_frame (288e3), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.5, map.pulse_at_frame (144e3), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, map.pulse_at_frame (96e3), 1e-17);

	CPPUNIT_ASSERT_DOUBLES_EQUAL (240.0, map.tempo_at_beat (24.0).beats_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (240.0, map.tempo_at_beat (12.0).beats_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (120.0, map.tempo_at_beat (6.0).beats_per_minute(), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (120.0, map.tempo_at_beat (0.0).beats_per_minute(), 1e-17);

	i = map._metrics.end();
	--i;
	CPPUNIT_ASSERT_EQUAL (framepos_t (288e3), (*i)->frame ());
}

void
TempoTest::rampTest48 ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meterA (4, 4);
	Tempo tempoA (77.0, 4.0);
	Tempo tempoB (217.0, 4.0);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, TempoSection::Ramp, AudioTime);
	map.add_tempo (tempoB, 0.0, (framepos_t) 60 * sampling_rate, TempoSection::Ramp, AudioTime);
	map.replace_meter (map.first_meter(), meterA, BBT_Time (1, 1, 0), (framepos_t) 0, AudioTime);

	/*

	  77bpm                                                 217bpm
	  0 frames                                              60 * sample rate frames
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
	const TempoSection& tB = map.tempo_section_at_frame ((framepos_t) 60 * sampling_rate);

	CPPUNIT_ASSERT_EQUAL ((framepos_t) 60 * sampling_rate, tA.frame_at_tempo (tB.beats_per_minute(), 300.0, sampling_rate));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (217.0, tA.tempo_at_frame ((framepos_t) 60 * sampling_rate, sampling_rate), 1e-17);
	CPPUNIT_ASSERT_EQUAL ((framepos_t) 60 * sampling_rate, tA.frame_at_pulse (tB.pulse(), sampling_rate));
	/* note 1e-14 here. (expm1) */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB.pulse(), tA.pulse_at_frame ((framepos_t) 60 * sampling_rate, sampling_rate), 1e-14);

	/* self-check tempo at pulse @ 125 bpm. */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (125.0, tA.tempo_at_pulse (tA.pulse_at_tempo (125.0, 0, sampling_rate)), 1e-17);

	/* check that tB's pulse is what tA thinks it should be */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB.pulse(), tA.pulse_at_tempo (217.0, 0, sampling_rate), 1e-17);

	/* check that the tempo at the halfway mark (in pulses) is half the tempo delta.*/
	CPPUNIT_ASSERT_DOUBLES_EQUAL (147.0, tA.tempo_at_pulse (tB.pulse() / 2.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL ((tB.pulse() - tA.pulse()) / 2.0, tA.pulse_at_tempo (147.0, 0, sampling_rate), 1e-17);

	/* self-check frame at pulse 20 seconds in. */
	const framepos_t target = 20 * sampling_rate;
	const framepos_t result = tA.frame_at_pulse (tA.pulse_at_frame (target, sampling_rate), sampling_rate);
	CPPUNIT_ASSERT_EQUAL (target, result);
}

void
TempoTest::rampTest44 ()
{
	int const sampling_rate = 44100;

	TempoMap map (sampling_rate);
	Meter meterA (4, 4);
	Tempo tempoA (77.0, 4.0);
	Tempo tempoB (217.0, 4.0);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, TempoSection::Ramp, AudioTime);
	map.add_tempo (tempoB, 0.0, (framepos_t) 60 * sampling_rate, TempoSection::Ramp, AudioTime);
	map.replace_meter (map.first_meter(), meterA, BBT_Time (1, 1, 0), (framepos_t) 0, AudioTime);

	/*

	  77bpm                                                 217bpm
	  0 frames                                              60 * sample rate frames
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
	const TempoSection& tB = map.tempo_section_at_frame ((framepos_t) 60 * sampling_rate);

	CPPUNIT_ASSERT_EQUAL ((framepos_t) 60 * sampling_rate, tA.frame_at_tempo (tB.beats_per_minute(), 300.0, sampling_rate));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (217.0, tA.tempo_at_frame ((framepos_t) 60 * sampling_rate, sampling_rate), 1e-17);
	CPPUNIT_ASSERT_EQUAL ((framepos_t) 60 * sampling_rate, tA.frame_at_pulse (tB.pulse(), sampling_rate));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB.pulse(), tA.pulse_at_frame ((framepos_t) 60 * sampling_rate, sampling_rate), 1e-14);

	/* self-check tempo at pulse @ 125 bpm. */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (125.0, tA.tempo_at_pulse (tA.pulse_at_tempo (125.0, 0, sampling_rate)), 1e-17);

	/* check that tB's pulse is what tA thinks it should be */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (tB.pulse(), tA.pulse_at_tempo (217.0, 0, sampling_rate), 1e-17);

	/* check that the tempo at the halfway mark (in pulses) is half the tempo delta.*/
	CPPUNIT_ASSERT_DOUBLES_EQUAL (147.0, tA.tempo_at_pulse (tB.pulse() / 2.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL ((tB.pulse() - tA.pulse()) / 2.0, tA.pulse_at_tempo (147.0, 0, sampling_rate), 1e-17);

	/* self-check frame at pulse 20 seconds in. */
	const framepos_t target = 20 * sampling_rate;
	const framepos_t result = tA.frame_at_pulse (tA.pulse_at_frame (target, sampling_rate), sampling_rate);
	CPPUNIT_ASSERT_EQUAL (target, result);
}

void
TempoTest::tempoAtPulseTest ()
{
	int const sampling_rate = 48000;

	TempoMap map (sampling_rate);
	Meter meterA (4, 8);
	Tempo tempoA (80.0, 8.0);
	Tempo tempoB (160.0, 3.0);
	Tempo tempoC (123.0, 4.0);

	map.replace_meter (map.first_meter(), meterA, BBT_Time (1, 1, 0), (framepos_t) 0, AudioTime);
	map.replace_tempo (map.first_tempo(), tempoA, 0.0, 0, TempoSection::Ramp, AudioTime);

	map.add_tempo (tempoB, 20.0, 0, TempoSection::Ramp, MusicTime);
	map.add_tempo (tempoC, 30.0, 0, TempoSection::Ramp, MusicTime);

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

	CPPUNIT_ASSERT_EQUAL (160.0, tA->tempo_at_pulse (20.0));
	CPPUNIT_ASSERT_EQUAL (123.0, tB->tempo_at_pulse (30.0));

	/* check that the tempo at the halfway mark (in pulses) is half the tempo delta.*/
	CPPUNIT_ASSERT_DOUBLES_EQUAL (((80.0 - 160.0)  / 2.0) + 160.0, tA->tempo_at_pulse (10.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (20.0 / 2.0, tA->pulse_at_tempo (120, 0, sampling_rate), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (((160.0 - 123.0)  / 2.0) + 123.0, tB->tempo_at_pulse (25.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (((20.0 - 30.0)  / 2.0) + 30.0, tB->pulse_at_tempo (141.5, 0, sampling_rate), 1e-17);

	CPPUNIT_ASSERT_EQUAL (tB->frame(), tA->frame_at_pulse (20.0, sampling_rate));
	CPPUNIT_ASSERT_EQUAL (tC->frame(), tB->frame_at_pulse (30.0, sampling_rate));

	CPPUNIT_ASSERT_EQUAL (tB->frame(), tA->frame_at_tempo (160.0, 20.0, sampling_rate));
	CPPUNIT_ASSERT_EQUAL (tC->frame(), tB->frame_at_tempo (123.0, 30.0, sampling_rate));

	/* self-check tempo at pulse @ 125 bpm. */
	CPPUNIT_ASSERT_DOUBLES_EQUAL (125.0, tA->tempo_at_pulse (tA->pulse_at_tempo (125.0, 0, sampling_rate)), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (160.0, tA->tempo_at_pulse (20.0), 1e-17);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (123.0, tB->tempo_at_pulse (30.0), 1e-17);
}
