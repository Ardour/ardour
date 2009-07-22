/* 
 * Copyright(C) 2000-2008 Paul Davis
 * Author: Hans Baier
 * 
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or(at your option) any later
 * version.
 * 
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <cassert>
#include <stdint.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "ardour/interpolation.h"

class InterpolationTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(InterpolationTest);
    //CPPUNIT_TEST(linearInterpolationTest);
    CPPUNIT_TEST(splineInterpolationTest);
    //CPPUNIT_TEST(libSamplerateInterpolationTest);
    CPPUNIT_TEST_SUITE_END();
    
    #define NUM_SAMPLES 1000000
    #define INTERVAL 100
    
    ARDOUR::Sample  input[NUM_SAMPLES];
    ARDOUR::Sample output[NUM_SAMPLES];
    
    ARDOUR::LinearInterpolation linear;
    ARDOUR::SplineInterpolation spline;
    ARDOUR::LibSamplerateInterpolation interpolation;

    public:
       	
        void setUp() {
            for (int i = 0; i < NUM_SAMPLES; ++i) {
                if (i % INTERVAL == 50) {
                    input[i] = 1.0f;
                } else {
                    input[i] = 0.0f;
                }
                output[i] = 0.0f;
            }
            linear.add_channel_to (NUM_SAMPLES, NUM_SAMPLES);
            spline.add_channel_to (NUM_SAMPLES, NUM_SAMPLES);
            interpolation.add_channel_to (NUM_SAMPLES, NUM_SAMPLES);
        }
        
        void tearDown() {
        }

        void linearInterpolationTest();
        void splineInterpolationTest();
        void libSamplerateInterpolationTest();

};
