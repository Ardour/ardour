#include <sigc++/sigc++.h>
#include "interpolation-test.h"

CPPUNIT_TEST_SUITE_REGISTRATION( InterpolationTest );

void
InterpolationTest::linearInterpolationTest ()
{
	std::cout << "\nLinear Interpolation Test\n";
	std::cout << "\nSpeed: 1.0";
	linear.set_speed (1.0);
	nframes_t result = linear.interpolate (NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)NUM_SAMPLES - 1, result);

	std::cout << "\nSpeed: 0.5";
	linear.set_speed (0.5);
	result = linear.interpolate (NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)NUM_SAMPLES / 2 - 1, result);

	std::cout << "\nSpeed: 0.2";
	linear.set_speed (0.2);
	result = linear.interpolate (NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)NUM_SAMPLES / 5 - 2, result);

	std::cout << "\nSpeed: 0.02";
	linear.set_speed (0.02);
	result = linear.interpolate (NUM_SAMPLES, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)NUM_SAMPLES / 50 - 2, result);
	
	std::cout << "\nSpeed: 2.0";
	linear.set_speed (2.0);
	result = linear.interpolate (NUM_SAMPLES / 2, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)NUM_SAMPLES - 2, result);

/*
    for (int i=0; i < NUM_SAMPLES / 5; ++i) {
        std::cout << "input[" << i << "] = " << input[i] << "  output[" << i << "] = " << output[i] << std::endl;	
    }
*/
	std::cout << "\nSpeed: 10.0";
	linear.set_speed (10.0);
	result = linear.interpolate (NUM_SAMPLES / 10, input, output);
	CPPUNIT_ASSERT_EQUAL ((uint32_t)NUM_SAMPLES - 10, result);
}


