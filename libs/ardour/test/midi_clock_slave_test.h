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

#include "ardour/tempo.h"
#include "ardour/slave.h"

namespace ARDOUR {

class TestSlaveSessionProxy : public ISlaveSessionProxy {
  #define FRAME_RATE 44100
  framecnt_t _period_size;

  double       transport_speed;
  framepos_t _transport_frame;
  framepos_t _frame_time;
  TempoMap    *_tempo_map;

  Tempo     tempo;
  Meter     meter;

  public:
        TestSlaveSessionProxy() : 
           transport_speed  (1.0), 
          _transport_frame  (0), 
          _frame_time       (1000000),
          _tempo_map        (0),
          tempo             (120),
          meter             (4.0, 4.0)
        {
          _tempo_map = new TempoMap (FRAME_RATE);
          _tempo_map->add_tempo (tempo, Timecode::BBT_Time(1, 1, 0));
          _tempo_map->add_meter (meter, Timecode::BBT_Time(1, 1, 0));          
        }

        // Controlling the mock object
        void        set_period_size (framecnt_t a_size) { _period_size = a_size; }
        framecnt_t period_size () const                 { return _period_size; }
        void next_period ()                       { 
          _transport_frame += double(_period_size) * double(transport_speed); 
          _frame_time += _period_size;
        }

        // Implementation
  	TempoMap&  tempo_map ()                const { return *_tempo_map; }
	framecnt_t frame_rate ()               const { return FRAME_RATE; }
	framepos_t audible_frame ()            const { return _transport_frame; }
	framepos_t transport_frame ()          const { return _transport_frame; }
	pframes_t  frames_since_cycle_start () const { return 0; }
	framepos_t frame_time ()               const { return _frame_time; }

	void request_locate (framepos_t frame, bool with_roll = false) { 
          _transport_frame = frame; 
        }

        void request_transport_speed (const double speed) { transport_speed = speed; }
};

class MIDIClock_SlaveTest : public CppUnit::TestFixture, ARDOUR::MIDIClock_Slave
{
    CPPUNIT_TEST_SUITE(MIDIClock_SlaveTest);
    CPPUNIT_TEST(testStepResponse);
    CPPUNIT_TEST_SUITE_END();

    public:
	MIDIClock_SlaveTest () : MIDIClock_Slave (new TestSlaveSessionProxy) {}
       	
        void setUp() {
        }
        
        void tearDown() {
        }

        void testStepResponse();
};

} // namespace ARDOUR
