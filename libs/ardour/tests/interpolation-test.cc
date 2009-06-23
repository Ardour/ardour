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
	interpolation.set_speed (1.0);
	nframes_t result = interpolation.interpolate (0, NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * interpolation.speed()), result);	
/*
*/
	for (int i=0; i < NUM_SAMPLES; ++i) {
        cout << "input[" << i << "] = " << input[i] << "  output[" << i << "] = " << output[i] << endl;	
    }
	for (int i = 0; i < NUM_SAMPLES; i += INTERVAL) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i+1]);
	}
	
	cout << "\nSpeed: 0.5";
	interpolation.set_speed (0.5);
	result = interpolation.interpolate (0, NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * interpolation.speed()), result);
	for (int i = 0; i < NUM_SAMPLES; i += (INTERVAL / interpolation.speed() +0.5)) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}
	
	cout << "\nSpeed: 0.2";
	interpolation.set_speed (0.2);
	result = interpolation.interpolate (0, NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * interpolation.speed()), result);

	cout << "\nSpeed: 0.02";
	interpolation.set_speed (0.02);
	result = interpolation.interpolate (0, NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * interpolation.speed()), result);
	
	cout << "\nSpeed: 0.002";
	interpolation.set_speed (0.002);
	result = interpolation.interpolate (0, NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * interpolation.speed()), result);
	
	cout << "\nSpeed: 2.0";
	interpolation.set_speed (2.0);
	result = interpolation.interpolate (0, NUM_SAMPLES / 2, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES / 2 * interpolation.speed()), result);
	for (int i = 0; i < NUM_SAMPLES / 2; i += (INTERVAL / interpolation.speed() +0.5)) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}

	cout << "\nSpeed: 10.0";
	interpolation.set_speed (10.0);
	result = interpolation.interpolate (0, NUM_SAMPLES / 10, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES / 10 * interpolation.speed()), result);
	for (int i = 0; i < NUM_SAMPLES / 10; i += (INTERVAL / interpolation.speed() +0.5)) {
		CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
	}
	/*
	for (int i=0; i < NUM_SAMPLES; ++i) {
        cout << "input[" << i << "] = " << input[i] << "  output[" << i << "] = " << output[i] << endl;	
    }
    */		
}


