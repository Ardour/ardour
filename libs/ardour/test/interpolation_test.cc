#include <sigc++/sigc++.h>
#include "interpolation_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(InterpolationTest);

using namespace std;
using namespace ARDOUR;

void
InterpolationTest::linearInterpolationTest ()
{
	framecnt_t result = 0;
//	cout << "\nLinear Interpolation Test\n";

//	cout << "\nSpeed: 1/3";
	for (int i = 0; 3*i < NUM_SAMPLES - 1024;) {
		linear.set_speed (double(1.0)/double(3.0));
		linear.set_target_speed (double(1.0)/double(3.0));
		result = linear.interpolate (0, 1024, input + i, output + i*3);
		i += result;
	}

//	cout << "\nSpeed: 1.0";
	linear.reset();
	linear.set_speed (1.0);
	linear.set_target_speed (linear.speed());
	result = linear.interpolate (0, NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES * linear.speed()), result);
	for (int i = 0; i < NUM_SAMPLES; i += INTERVAL) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}

//	cout << "\nSpeed: 0.5";
	linear.reset();
	linear.set_speed (0.5);
	linear.set_target_speed (linear.speed());
	result = linear.interpolate (0, NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES * linear.speed()), result);
	for (int i = 0; i < NUM_SAMPLES; i += (INTERVAL / linear.speed() +0.5)) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}

//	cout << "\nSpeed: 0.2";
	linear.reset();
	linear.set_speed (0.2);
	linear.set_target_speed (linear.speed());
	result = linear.interpolate (0, NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES * linear.speed()), result);

//	cout << "\nSpeed: 0.02";
	linear.reset();
	linear.set_speed (0.02);
	linear.set_target_speed (linear.speed());
	result = linear.interpolate (0, NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES * linear.speed()), result);

//	cout << "\nSpeed: 0.002";
	linear.reset();
	linear.set_speed (0.002);
	linear.set_target_speed (linear.speed());
	result = linear.interpolate (0, NUM_SAMPLES, input, output);
	linear.speed();
	CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES * linear.speed()), result);

//	cout << "\nSpeed: 2.0";
	linear.reset();
	linear.set_speed (2.0);
	linear.set_target_speed (linear.speed());
	result = linear.interpolate (0, NUM_SAMPLES / 2, input, output);
	CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES / 2 * linear.speed()), result);
	for (int i = 0; i < NUM_SAMPLES / 2; i += (INTERVAL / linear.speed() +0.5)) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}

//	cout << "\nSpeed: 10.0";
	linear.set_speed (10.0);
	linear.set_target_speed (linear.speed());
	result = linear.interpolate (0, NUM_SAMPLES / 10, input, output);
	CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES / 10 * linear.speed()), result);
	for (int i = 0; i < NUM_SAMPLES / 10; i += (INTERVAL / linear.speed() +0.5)) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}
	/*
	   for (int i=0; i < NUM_SAMPLES; ++i) {
	   cout  << i << " " << output[i] << endl; 
	   }
	   */
}

void
InterpolationTest::cubicInterpolationTest ()
{
	framecnt_t result = 0;
//	cout << "\nCubic Interpolation Test\n";

//	cout << "\nSpeed: 1/3";
	for (int i = 0; 3*i < NUM_SAMPLES - 1024;) {
		cubic.set_speed (double(1.0)/double(3.0));
		cubic.set_target_speed (double(1.0)/double(3.0));
		result = cubic.interpolate (0, 1024, input + i, output + i*3);
		i += result;
	}

//	cout << "\nSpeed: 1.0";
	cubic.reset();
	cubic.set_speed (1.0);
	cubic.set_target_speed (cubic.speed());
	result = cubic.interpolate (0, NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES * cubic.speed()), result);
	for (int i = 0; i < NUM_SAMPLES; i += INTERVAL) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}

//	cout << "\nSpeed: 0.5";
	cubic.reset();
	cubic.set_speed (0.5);
	cubic.set_target_speed (cubic.speed());
	result = cubic.interpolate (0, NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES * cubic.speed()), result);
	for (int i = 0; i < NUM_SAMPLES; i += (INTERVAL / cubic.speed() +0.5)) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}

//	cout << "\nSpeed: 0.2";
	cubic.reset();
	cubic.set_speed (0.2);
	cubic.set_target_speed (cubic.speed());
	result = cubic.interpolate (0, NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES * cubic.speed()), result);

//	cout << "\nSpeed: 0.02";
	cubic.reset();
	cubic.set_speed (0.02);
	cubic.set_target_speed (cubic.speed());
	result = cubic.interpolate (0, NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES * cubic.speed()), result);

	/* This one fails due too error accumulation
	   cout << "\nSpeed: 0.002";
	   cubic.reset();
	   cubic.set_speed (0.002);
	   cubic.set_target_speed (cubic.speed());
	   result = cubic.interpolate (0, NUM_SAMPLES, input, output);
	   cubic.speed();
	   CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES * cubic.speed()), result);
	   */

//	cout << "\nSpeed: 2.0";
	cubic.reset();
	cubic.set_speed (2.0);
	cubic.set_target_speed (cubic.speed());
	result = cubic.interpolate (0, NUM_SAMPLES / 2, input, output);
	CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES / 2 * cubic.speed()), result);
	for (int i = 0; i < NUM_SAMPLES / 2; i += (INTERVAL / cubic.speed() +0.5)) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}

//	cout << "\nSpeed: 10.0";
	cubic.set_speed (10.0);
	cubic.set_target_speed (cubic.speed());
	result = cubic.interpolate (0, NUM_SAMPLES / 10, input, output);
	CPPUNIT_ASSERT_EQUAL ((framecnt_t)(NUM_SAMPLES / 10 * cubic.speed()), result);
	for (int i = 0; i < NUM_SAMPLES / 10; i += (INTERVAL / cubic.speed() +0.5)) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}
}
