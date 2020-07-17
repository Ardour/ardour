/* COMPILE: c++ -o test -I../pbd -I. test.cc */

#include <iostream>
#include <iomanip>

#include "temporal/tempo.h"

using namespace std;
using namespace Temporal;

int
main (int argc, char* argv[])
{
	double bpm;

	if (argc < 2) {
		cerr << "Usage: " << argv[0] << " BPM\n";
		return -1;
	}

	if (sscanf (argv[1], "%lf", &bpm) != 1) {
		cerr << "Cannot parse " << argv[1] << " as floating point value\n";
		return -2;
	}

	TempoValue tempo (bpm);
	uint64_t numerator_seconds = 1;
	uint64_t denominator_seconds = 2;
	Temporal::Beats fb = tempo.seconds_as_beats (numerator_seconds, denominator_seconds);

	/* compute 20 minutes of beats:ticks */
	Temporal::Beats b20 = tempo.seconds_as_beats ((20 * 60), 1);

	double d20 = 20.0 * bpm;
	double d20r = 20.0 * tempo.actual_bpm_for_display_only ();

	cout << "bpm " << setprecision (12) << bpm << " => " << tempo << " ticks/second = " << tempo.ticks_per_second ()
	     << " tps " << tempo.ticks_per_second ()
	     << " bpm " << tempo.actual_bpm_for_display_only ()
	     << ' ' << numerator_seconds << '/' << denominator_seconds << " sec = " << fb
	     << " 20 mins = " << b20
	     << " 20 mins " << d20 << " computed " << d20r
	     << " b-as-sc " << tempo.beats_as_superclocks (Beats (1,0))
	     << " scpb " << tempo.superclocks_per_beat ()
	     << endl;

	for (uint32_t tn = 0; tn < Temporal::ticks_per_beat; ++tn) {
		Temporal::Beats b (1, tn);

		double sec = tempo.beats_as_float_seconds_avoid_me (b);
		double csec = (60. / bpm) * (1 + (tn / 1920.));

		if (fabs (sec - csec) > 0.00000001) {
			cout << b << " sec " << sec << " csec " << csec << " err " << sec - csec << endl;
		}
	}

	int rate[] = { 16000, 22050, 24000, 32000, 33075, 44100, 48000, 88200, 96000, 0 };

	for (int s = 0; rate[s] != 0; ++s) {

		cout << "Checking with SR = " << rate[s] << endl;

		double tempos[] = {
			1,
			10,
			30,
			60,
			120,
			240,
			1200,
			33,
			47,
			91 + 4./7,
			100./3.,
			100./7.,
			100./5.,
			100./9.,
			M_PI,
			M_PI * 20.,
			0.
		};


		for (int t = 0; tempos[t] != 0; ++t) {

			tempo = TempoValue (tempos[t]);
			cout << "\tChecking tempo " << tempo.given_bpm_for_display_only () << endl;

			for (uint32_t tn = 0; tn < Temporal::ticks_per_beat; ++tn) {
				Temporal::Beats b (1, tn);

				superclock_t sc = tempo.beats_as_superclocks (b);
				Temporal::Beats b2 = tempo.superclocks_as_beats (sc);

				if (b2 != b) {
					cout << "\t\tb2 " << b2 << " != b " << b << endl;
				}

				uint64_t samples = superclock_to_samples (sc, rate[s]);
				superclock_t sc2 = samples_to_superclock (samples, rate[s]);

				Temporal::Beats b3 = tempo.superclocks_as_beats (sc2);

				if (b3 != b) {
					cout << "\t\tb3 " << b3 << " != b " << b << endl;
					break;
				}
			}

			cout << "Now checking sample positions\n";

			for (int p = 0; p < rate[s]; ++p) {
				superclock_t sc = samples_to_superclock (p, rate[s]);
				Temporal::Beats b = tempo.superclocks_as_beats (sc);
				superclock_t sc2 = tempo.beats_as_superclocks (b);
				uint64_t sm = superclock_to_samples (sc2, rate[s]);

				if (p != sm) {
					cout << "\t\tsm " << sm << " != " << p << endl;
					break;
				}
			}
		}
	}

	return 0;
}
