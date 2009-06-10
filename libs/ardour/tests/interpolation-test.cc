#include <sigc++/sigc++.h>
#include "interpolation-test.h"

CPPUNIT_TEST_SUITE_REGISTRATION( InterpolationTest );

using namespace std;
using namespace ARDOUR;

void
InterpolationTest::linearInterpolationTest ()
{
	cout << "\nLinear Interpolation Test\n";
	cout << "\nSpeed: 1.0";
	linear.set_speed (1.0);
	linear.set_target_speed (linear.speed());
	nframes_t result = linear.interpolate (NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * linear.speed()), result);	
	for (int i = 0; i < NUM_SAMPLES; i += INTERVAL) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}
	
	cout << "\nSpeed: 0.5";
	linear.set_speed (0.5);
	linear.set_target_speed (linear.speed());
	result = linear.interpolate (NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * linear.speed()), result);
	for (int i = 0; i < NUM_SAMPLES; i += (INTERVAL / linear.speed() +0.5)) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}
	
	cout << "\nSpeed: 0.2";
	linear.set_speed (0.2);
	linear.set_target_speed (linear.speed());
	result = linear.interpolate (NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * linear.speed()), result);

	cout << "\nSpeed: 0.02";
	linear.set_speed (0.02);
	linear.set_target_speed (linear.speed());
	result = linear.interpolate (NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * linear.speed()), result);
	
	cout << "\nSpeed: 0.002";
	linear.set_speed (0.002);
	linear.set_target_speed (linear.speed());
	result = linear.interpolate (NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * linear.speed()), result);
	
	cout << "\nSpeed: 2.0";
	linear.set_speed (2.0);
	linear.set_target_speed (linear.speed());
	result = linear.interpolate (NUM_SAMPLES / 2, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES / 2 * linear.speed()), result);
	for (int i = 0; i < NUM_SAMPLES / 2; i += (INTERVAL / linear.speed() +0.5)) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}

	cout << "\nSpeed: 10.0";
	linear.set_speed (10.0);
	linear.set_target_speed (linear.speed());
	result = linear.interpolate (NUM_SAMPLES / 10, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES / 10 * linear.speed()), result);
	for (int i = 0; i < NUM_SAMPLES / 10; i += (INTERVAL / linear.speed() +0.5)) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}
	/*
	for (int i=0; i < NUM_SAMPLES; ++i) {
        cout << "input[" << i << "] = " << input[i] << "  output[" << i << "] = " << output[i] << endl;	
    }
    */		
}


