#include <sigc++/sigc++.h>
#include "interpolation-test.h"

CPPUNIT_TEST_SUITE_REGISTRATION( InterpolationTest );

using namespace std;
using namespace ARDOUR;

void
InterpolationTest::linearInterpolationTest ()
{
        nframes_t result = 0;
         cout << "\nLinear Interpolation Test\n";
         
         cout << "\nSpeed: 1/3";
         for (int i = 0; i < NUM_SAMPLES - 1024;) {
             linear.set_speed (double(1.0)/double(3.0));
             linear.set_target_speed (double(1.0)/double(3.0));
             //printf ("Interpolate: input: %d, output: %d, i: %d\n", input + i, output + i, i);
             result = linear.interpolate (0, 1024, input + i, output + i);
             printf ("Result: %d\n", result);
             //CPPUNIT_ASSERT_EQUAL ((uint32_t)((NUM_SAMPLES - 100) * interpolation.speed()), result);
             i += result;
         }
         
         cout << "\nSpeed: 1.0";
         linear.reset();
         linear.set_speed (1.0);
         linear.set_target_speed (linear.speed());
         result = linear.interpolate (0, NUM_SAMPLES, input, output);
         CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * linear.speed()), result);
         for (int i = 0; i < NUM_SAMPLES; i += INTERVAL) {
                 CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
         }
         
         cout << "\nSpeed: 0.5";
         linear.reset();
         linear.set_speed (0.5);
         linear.set_target_speed (linear.speed());
         result = linear.interpolate (0, NUM_SAMPLES, input, output);
         CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * linear.speed()), result);
         for (int i = 0; i < NUM_SAMPLES; i += (INTERVAL / linear.speed() +0.5)) {
                 CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
         }
         
         cout << "\nSpeed: 0.2";
         linear.reset();
         linear.set_speed (0.2);
         linear.set_target_speed (linear.speed());
         result = linear.interpolate (0, NUM_SAMPLES, input, output);
         CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * linear.speed()), result);

         cout << "\nSpeed: 0.02";
         linear.reset();
         linear.set_speed (0.02);
         linear.set_target_speed (linear.speed());
         result = linear.interpolate (0, NUM_SAMPLES, input, output);
         CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES * linear.speed()), result);
         
         cout << "\nSpeed: 0.002";
         linear.reset();
         linear.set_speed (0.002);
         linear.set_target_speed (linear.speed());
         result = linear.interpolate (0, NUM_SAMPLES, input, output);
         linear.speed();
         printf("BOOM!: expexted: %d, result = %d\n", (nframes_t)(NUM_SAMPLES * linear.speed()), result);
         CPPUNIT_ASSERT_EQUAL ((nframes_t)(NUM_SAMPLES * linear.speed()), result);
         
         cout << "\nSpeed: 2.0";
         linear.reset();
         linear.set_speed (2.0);
         linear.set_target_speed (linear.speed());
         result = linear.interpolate (0, NUM_SAMPLES / 2, input, output);
         CPPUNIT_ASSERT_EQUAL ((uint32_t)(NUM_SAMPLES / 2 * linear.speed()), result);
         for (int i = 0; i < NUM_SAMPLES / 2; i += (INTERVAL / linear.speed() +0.5)) {
                 CPPUNIT_ASSERT_EQUAL (1.0f, output[i]);
         }

         cout << "\nSpeed: 10.0";
         linear.set_speed (10.0);
         linear.set_target_speed (linear.speed());
         result = linear.interpolate (0, NUM_SAMPLES / 10, input, output);
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

void
InterpolationTest::libSamplerateInterpolationTest ()
{
    nframes_t result;
    
    cout << "\nLibSamplerate Interpolation Test\n";
/*
    cout << "\nSpeed: 1.0";
    interpolation.set_speed (1.0);
    for (int i = 0; i < NUM_SAMPLES;) {
        interpolation.set_speed (1.0);
        result = interpolation.interpolate (0, INTERVAL/10, input + i, output + i);
        CPPUNIT_ASSERT_EQUAL ((uint32_t)(INTERVAL/10 * interpolation.speed()), result);
        i += result;
    }
    
    for (int i = 0; i < NUM_SAMPLES; i += INTERVAL) {
        CPPUNIT_ASSERT_EQUAL (1.0f, output[i+1]);
    }
*/
    
    cout << "\nSpeed: 0.5";
    for (int i = 0; i < NUM_SAMPLES;) {
        interpolation.set_speed (0.5);
        //printf ("Interpolate: input: %d, output: %d, i: %d\n", input + i, output + i, i);
        result = interpolation.interpolate (0, NUM_SAMPLES - 100, input + i, output + i);
        printf ("Result: %d\n", result);
        //CPPUNIT_ASSERT_EQUAL ((uint32_t)((NUM_SAMPLES - 100) * interpolation.speed()), result);
        //i += result;
        break;
    }

    for (int i=0; i < NUM_SAMPLES; ++i) {
        cout << "input[" << i << "] = " << input[i] << "  output[" << i << "] = " << output[i] << endl;    
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


