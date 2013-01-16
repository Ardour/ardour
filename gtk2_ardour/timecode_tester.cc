/*
    Copyright (C) 2012 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

void
AudioClock::timecode_tester ()
{
#if 0
#define Timecode_SAMPLE_TEST_1
#define Timecode_SAMPLE_TEST_2
#define Timecode_SAMPLE_TEST_3
#define Timecode_SAMPLE_TEST_4
#define Timecode_SAMPLE_TEST_5
#define Timecode_SAMPLE_TEST_6
#define Timecode_SAMPLE_TEST_7

	// Testcode for timecode<->sample conversions (P.S.)
	Timecode::Time timecode1;
	framepos_t sample1;
	framepos_t oldsample = 0;
	Timecode::Time timecode2;
	framecnt_t sample_increment;

	sample_increment = (framecnt_t)rint(_session->frame_rate() / _session->timecode_frames_per_second);

#ifdef Timecode_SAMPLE_TEST_1
	// Test 1: use_offset = false, use_subframes = false
	cout << "use_offset = false, use_subframes = false" << endl;
	for (int i = 0; i < 108003; i++) {
		_session->timecode_to_sample( timecode1, sample1, false /* use_offset */, false /* use_subframes */ );
		_session->sample_to_timecode( sample1, timecode2, false /* use_offset */, false /* use_subframes */ );

		if ((i > 0) && ( ((sample1 - oldsample) != sample_increment) && ((sample1 - oldsample) != (sample_increment + 1)) && ((sample1 - oldsample) != (sample_increment - 1)))) {
			cout << "ERROR: sample increment not right: " << (sample1 - oldsample) << " != " << sample_increment << endl;
			cout << "timecode1: " << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode2: " << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}

		if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames) {
			cout << "ERROR: timecode2 not equal timecode1" << endl;
			cout << "timecode1: " << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode2: " << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}
		oldsample = sample1;
		_session->timecode_increment( timecode1 );
	}

	cout << "sample_increment: " << sample_increment << endl;
	cout << "sample: " << sample1 << " -> ";
	cout << "timecode: " << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
#endif

#ifdef Timecode_SAMPLE_TEST_2
	// Test 2: use_offset = true, use_subframes = false
	cout << "use_offset = true, use_subframes = false" << endl;

	timecode1.hours = 0;
	timecode1.minutes = 0;
	timecode1.seconds = 0;
	timecode1.frames = 0;
	timecode1.subframes = 0;
	sample1 = oldsample = 0;

	_session->sample_to_timecode( sample1, timecode1, true /* use_offset */, false /* use_subframes */ );
	cout << "Starting at sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << endl;

	for (int i = 0; i < 108003; i++) {
		_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, false /* use_subframes */ );
		_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, false /* use_subframes */ );

//     cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
//     cout << "sample: " << sample1 << endl;
//     cout << "sample: " << sample1 << " -> ";
//     cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;

		if ((i > 0) && ( ((sample1 - oldsample) != sample_increment) && ((sample1 - oldsample) != (sample_increment + 1)) && ((sample1 - oldsample) != (sample_increment - 1)))) {
			cout << "ERROR: sample increment not right: " << (sample1 - oldsample) << " != " << sample_increment << endl;
			cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}

		if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames) {
			cout << "ERROR: timecode2 not equal timecode1" << endl;
			cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}
		oldsample = sample1;
		_session->timecode_increment( timecode1 );
	}

	cout << "sample_increment: " << sample_increment << endl;
	cout << "sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
#endif

#ifdef Timecode_SAMPLE_TEST_3
	// Test 3: use_offset = true, use_subframes = false, decrement
	cout << "use_offset = true, use_subframes = false, decrement" << endl;

	_session->sample_to_timecode( sample1, timecode1, true /* use_offset */, false /* use_subframes */ );
	cout << "Starting at sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << endl;

	for (int i = 0; i < 108003; i++) {
		_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, false /* use_subframes */ );
		_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, false /* use_subframes */ );

//     cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
//     cout << "sample: " << sample1 << endl;
//     cout << "sample: " << sample1 << " -> ";
//     cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;

		if ((i > 0) && ( ((oldsample - sample1) != sample_increment) && ((oldsample - sample1) != (sample_increment + 1)) && ((oldsample - sample1) != (sample_increment - 1)))) {
			cout << "ERROR: sample increment not right: " << (oldsample - sample1) << " != " << sample_increment << endl;
			cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}

		if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames) {
			cout << "ERROR: timecode2 not equal timecode1" << endl;
			cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}
		oldsample = sample1;
		_session->timecode_decrement( timecode1 );
	}

	cout << "sample_decrement: " << sample_increment << endl;
	cout << "sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
#endif


#ifdef Timecode_SAMPLE_TEST_4
	// Test 4: use_offset = true, use_subframes = true
	cout << "use_offset = true, use_subframes = true" << endl;

	for (long sub = 5; sub < 80; sub += 5) {
		timecode1.hours = 0;
		timecode1.minutes = 0;
		timecode1.seconds = 0;
		timecode1.frames = 0;
		timecode1.subframes = 0;
		sample1 = oldsample = (sample_increment * sub) / 80;

		_session->sample_to_timecode( sample1, timecode1, true /* use_offset */, true /* use_subframes */ );

		cout << "starting at sample: " << sample1 << " -> ";
		cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << endl;

		for (int i = 0; i < 108003; i++) {
			_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, true /* use_subframes */ );
			_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, true /* use_subframes */ );

			if ((i > 0) && ( ((sample1 - oldsample) != sample_increment) && ((sample1 - oldsample) != (sample_increment + 1)) && ((sample1 - oldsample) != (sample_increment - 1)))) {
				cout << "ERROR: sample increment not right: " << (sample1 - oldsample) << " != " << sample_increment << endl;
				cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
				cout << "sample: " << sample1 << endl;
				cout << "sample: " << sample1 << " -> ";
				cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
				//break;
			}

			if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames || timecode2.subframes != timecode1.subframes) {
				cout << "ERROR: timecode2 not equal timecode1" << endl;
				cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
				cout << "sample: " << sample1 << endl;
				cout << "sample: " << sample1 << " -> ";
				cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
				break;
			}
			oldsample = sample1;
			_session->timecode_increment( timecode1 );
		}

		cout << "sample_increment: " << sample_increment << endl;
		cout << "sample: " << sample1 << " -> ";
		cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;

		for (int i = 0; i < 108003; i++) {
			_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, true /* use_subframes */ );
			_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, true /* use_subframes */ );

			if ((i > 0) && ( ((oldsample - sample1) != sample_increment) && ((oldsample - sample1) != (sample_increment + 1)) && ((oldsample - sample1) != (sample_increment - 1)))) {
				cout << "ERROR: sample increment not right: " << (oldsample - sample1) << " != " << sample_increment << endl;
				cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
				cout << "sample: " << sample1 << endl;
				cout << "sample: " << sample1 << " -> ";
				cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
				//break;
			}

			if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames || timecode2.subframes != timecode1.subframes) {
				cout << "ERROR: timecode2 not equal timecode1" << endl;
				cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
				cout << "sample: " << sample1 << endl;
				cout << "sample: " << sample1 << " -> ";
				cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
				break;
			}
			oldsample = sample1;
			_session->timecode_decrement( timecode1 );
		}

		cout << "sample_decrement: " << sample_increment << endl;
		cout << "sample: " << sample1 << " -> ";
		cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
	}
#endif


#ifdef Timecode_SAMPLE_TEST_5
	// Test 5: use_offset = true, use_subframes = false, increment seconds
	cout << "use_offset = true, use_subframes = false, increment seconds" << endl;

	timecode1.hours = 0;
	timecode1.minutes = 0;
	timecode1.seconds = 0;
	timecode1.frames = 0;
	timecode1.subframes = 0;
	sample1 = oldsample = 0;
	sample_increment = _session->frame_rate();

	_session->sample_to_timecode( sample1, timecode1, true /* use_offset */, false /* use_subframes */ );
	cout << "Starting at sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << endl;

	for (int i = 0; i < 3600; i++) {
		_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, false /* use_subframes */ );
		_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, false /* use_subframes */ );

//     cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
//     cout << "sample: " << sample1 << endl;
//     cout << "sample: " << sample1 << " -> ";
//     cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;

//     if ((i > 0) && ( ((sample1 - oldsample) != sample_increment) && ((sample1 - oldsample) != (sample_increment + 1)) && ((sample1 - oldsample) != (sample_increment - 1))))
//     {
//       cout << "ERROR: sample increment not right: " << (sample1 - oldsample) << " != " << sample_increment << endl;
//       break;
//     }

		if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames) {
			cout << "ERROR: timecode2 not equal timecode1" << endl;
			cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}
		oldsample = sample1;
		_session->timecode_increment_seconds( timecode1 );
	}

	cout << "sample_increment: " << sample_increment << endl;
	cout << "sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
#endif


#ifdef Timecode_SAMPLE_TEST_6
	// Test 6: use_offset = true, use_subframes = false, increment minutes
	cout << "use_offset = true, use_subframes = false, increment minutes" << endl;

	timecode1.hours = 0;
	timecode1.minutes = 0;
	timecode1.seconds = 0;
	timecode1.frames = 0;
	timecode1.subframes = 0;
	sample1 = oldsample = 0;
	sample_increment = _session->frame_rate() * 60;

	_session->sample_to_timecode( sample1, timecode1, true /* use_offset */, false /* use_subframes */ );
	cout << "Starting at sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << endl;

	for (int i = 0; i < 60; i++) {
		_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, false /* use_subframes */ );
		_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, false /* use_subframes */ );

//     cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
//     cout << "sample: " << sample1 << endl;
//     cout << "sample: " << sample1 << " -> ";
//     cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;

//     if ((i > 0) && ( ((sample1 - oldsample) != sample_increment) && ((sample1 - oldsample) != (sample_increment + 1)) && ((sample1 - oldsample) != (sample_increment - 1))))
//     {
//       cout << "ERROR: sample increment not right: " << (sample1 - oldsample) << " != " << sample_increment << endl;
//       break;
//     }

		if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames) {
			cout << "ERROR: timecode2 not equal timecode1" << endl;
			cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}
		oldsample = sample1;
		_session->timecode_increment_minutes( timecode1 );
	}

	cout << "sample_increment: " << sample_increment << endl;
	cout << "sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
#endif

#ifdef Timecode_SAMPLE_TEST_7
	// Test 7: use_offset = true, use_subframes = false, increment hours
	cout << "use_offset = true, use_subframes = false, increment hours" << endl;

	timecode1.hours = 0;
	timecode1.minutes = 0;
	timecode1.seconds = 0;
	timecode1.frames = 0;
	timecode1.subframes = 0;
	sample1 = oldsample = 0;
	sample_increment = _session->frame_rate() * 60 * 60;

	_session->sample_to_timecode( sample1, timecode1, true /* use_offset */, false /* use_subframes */ );
	cout << "Starting at sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << endl;

	for (int i = 0; i < 10; i++) {
		_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, false /* use_subframes */ );
		_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, false /* use_subframes */ );

//     cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
//     cout << "sample: " << sample1 << endl;
//     cout << "sample: " << sample1 << " -> ";
//     cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;

//     if ((i > 0) && ( ((sample1 - oldsample) != sample_increment) && ((sample1 - oldsample) != (sample_increment + 1)) && ((sample1 - oldsample) != (sample_increment - 1))))
//     {
//       cout << "ERROR: sample increment not right: " << (sample1 - oldsample) << " != " << sample_increment << endl;
//       break;
//     }

		if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames) {
			cout << "ERROR: timecode2 not equal timecode1" << endl;
			cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}
		oldsample = sample1;
		_session->timecode_increment_hours( timecode1 );
	}

	cout << "sample_increment: " << sample_increment << endl;
	cout << "sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
#endif

#endif
}
