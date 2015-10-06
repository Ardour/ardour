#include <iostream>

#include "ardour/dsp_load_calculator.h"

#include "dsp_load_calculator_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (DSPLoadCalculatorTest);


#if defined(PLATFORM_WINDOWS) && defined(COMPILER_MINGW)
/* cppunit-1.13.2  uses assertion_traits<double>
 *    sprintf( , "%.*g", precision, x)
 * to format a double. The actual comparison is performed on a string.
 * This is problematic with mingw/windows|wine, "%.*g" formatting fails.
 *
 * This quick hack compares float, however float compatisons are at most Y.MMMM+eXX,
 * the max precision needs to be limited. to the last mantissa digit.
 *
 * Anyway, actual maths is verified with Linux and OSX unit-tests,
 * and this needs to go to https://sourceforge.net/p/cppunit/bugs/
 */
#include <math.h>
#define CPPUNIT_ASSERT_DOUBLES_EQUAL(A,B,P) CPPUNIT_ASSERT_EQUAL((float)rint ((A) / (P)),(float)rint ((B) / (P)))
#endif

using namespace std;
using namespace ARDOUR;

void
DSPLoadCalculatorTest::basicTest ()
{
	DSPLoadCalculator dsp_calc;

	dsp_calc.set_max_time(48000, 512);
	int64_t dsp_100_pc_48k_us = 10666;

	CPPUNIT_ASSERT(dsp_calc.get_max_time_us() == dsp_100_pc_48k_us);

	// test equivalent of 10% load
	dsp_calc.set_start_timestamp_us(0);
	dsp_calc.set_stop_timestamp_us(dsp_100_pc_48k_us/10);
	CPPUNIT_ASSERT(dsp_calc.get_dsp_load() <= 0.1f);

	// test equivalent of 50% load and check that the load jumps to 50 percent
	dsp_calc.set_start_timestamp_us(0);
	dsp_calc.set_stop_timestamp_us(dsp_100_pc_48k_us/2);
	CPPUNIT_ASSERT(dsp_calc.get_dsp_load() <= 0.5f);

	// test equivalent of 100% load
	dsp_calc.set_start_timestamp_us(0);
	dsp_calc.set_stop_timestamp_us(dsp_100_pc_48k_us);
	CPPUNIT_ASSERT(dsp_calc.elapsed_time_us() == dsp_100_pc_48k_us);
	CPPUNIT_ASSERT(dsp_calc.get_dsp_load() <= 1.0f);

	// test setting the equivalent of 100% twice doesn't lead to a dsp value > 1.0
	dsp_calc.set_start_timestamp_us(dsp_100_pc_48k_us);
	dsp_calc.set_stop_timestamp_us(dsp_100_pc_48k_us * 2);
	CPPUNIT_ASSERT(dsp_calc.elapsed_time_us() == dsp_100_pc_48k_us);
	CPPUNIT_ASSERT(dsp_calc.get_dsp_load() <= 1.0f);

	// test setting the equivalent of 200% clamps the value to 1.0
	dsp_calc.set_start_timestamp_us(dsp_100_pc_48k_us);
	dsp_calc.set_stop_timestamp_us(dsp_100_pc_48k_us * 3);
	CPPUNIT_ASSERT(dsp_calc.get_dsp_load() == 1.0f);

	// test setting the an stop timestamp before the start timestamp is ignored
	// and the previous dsp value is returned
	dsp_calc.set_start_timestamp_us(dsp_100_pc_48k_us * 2);
	dsp_calc.set_stop_timestamp_us(dsp_100_pc_48k_us);
	CPPUNIT_ASSERT(dsp_calc.get_dsp_load() == 1.0f);

	float dsp_load = dsp_calc.get_dsp_load();

	// test setting the equivalent of beyond the max_timer_error_us is ignored and
	// the previous dsp value is returned
	dsp_calc.set_start_timestamp_us (0);
	dsp_calc.set_stop_timestamp_us (dsp_100_pc_48k_us*10);
	CPPUNIT_ASSERT(dsp_calc.elapsed_time_us() > dsp_calc.max_timer_error_us());
	CPPUNIT_ASSERT(dsp_calc.get_dsp_load() == dsp_load);

	// test the rate of rolloff of the LPF from 100% with load at constant 50%
	// over the equivalent of 1 second
	for (int i = 0; i < 1e6 / dsp_100_pc_48k_us; ++i) {
		dsp_calc.set_start_timestamp_us(0);
		dsp_calc.set_stop_timestamp_us(dsp_100_pc_48k_us / 2);
		CPPUNIT_ASSERT(dsp_calc.elapsed_time_us() == 5333);
		CPPUNIT_ASSERT(dsp_calc.get_dsp_load() <= 1.0);
		CPPUNIT_ASSERT(dsp_calc.get_dsp_load() >= 0.5);
#if 0
		std::cout << "DSP 50% load value = " << dsp_calc.get_dsp_load() << std::endl;
#endif
	}

	// test that the LPF is still working after one second of values
	// TODO need to work out what is required in terms of responsiveness etc
	CPPUNIT_ASSERT(dsp_calc.get_dsp_load() > 0.5f);

	// compare 96k to 48k
	DSPLoadCalculator dsp_calc_96k;
	dsp_calc_96k.set_max_time(96000, 512);
	int64_t dsp_100_pc_96k_us = 5333;

	// reset both to 100%
	dsp_calc.set_start_timestamp_us(dsp_100_pc_48k_us);
	dsp_calc.set_stop_timestamp_us(dsp_100_pc_48k_us * 2);
	CPPUNIT_ASSERT(dsp_calc.elapsed_time_us() == dsp_100_pc_48k_us);
	CPPUNIT_ASSERT(dsp_calc.get_dsp_load() <= 1.0f);
	dsp_calc_96k.set_start_timestamp_us(dsp_100_pc_96k_us);
	dsp_calc_96k.set_stop_timestamp_us(dsp_100_pc_96k_us * 2);
	CPPUNIT_ASSERT(dsp_calc_96k.elapsed_time_us() == dsp_100_pc_96k_us);
	CPPUNIT_ASSERT(dsp_calc_96k.get_dsp_load() <= 1.0f);

	// test the rate of rolloff of the LPF from 100% with load at constant 50%
	// over the equivalent of 1 second for 48k and 96k and test for ~equality
	for (int i = 0; i < 1e6 / dsp_100_pc_96k_us; ++i) {
		dsp_calc_96k.set_start_timestamp_us(0);
		dsp_calc_96k.set_stop_timestamp_us(dsp_100_pc_96k_us / 2);
		if (i % 2 == 0) {
			dsp_calc.set_start_timestamp_us(0);
			dsp_calc.set_stop_timestamp_us(dsp_100_pc_48k_us / 2);
#if 0
			std::cout << "DSP 50% load value 48k = " << dsp_calc.get_dsp_load()
			          << std::endl;
			std::cout << "DSP 50% load value 96k = " << dsp_calc_96k.get_dsp_load()
			          << std::endl;
#endif
			CPPUNIT_ASSERT_DOUBLES_EQUAL(dsp_calc.get_dsp_load(),
			                             dsp_calc_96k.get_dsp_load(), 0.001);
		}
	}

}
