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
  nframes64_t _period_size;

  double       transport_speed;
  nframes64_t _transport_frame;
  nframes64_t _frame_time;
  TempoMap    _tempo_map;

  Tempo     tempo;
  Meter     meter;
  BBT_Time  zero;

  public:
        TestSlaveSessionProxy() : 
           transport_speed  (1.0), 
          _transport_frame  (0), 
          _frame_time       (1000000),
          _tempo_map        (FRAME_RATE),
          tempo             (120),
          meter             (4.0, 4.0)
        {
          _tempo_map.add_tempo (tempo, zero);
          _tempo_map.add_meter (meter, zero);          
        }

        // Controlling the mock object
        void set_period_size (nframes64_t a_size) { _period_size = a_size; }
        void next_period ()                       { 
          _transport_frame += _period_size; 
          _frame_time += _period_size;
        }

        // Implementation
  	TempoMap&   tempo_map ()                { return _tempo_map; }
	nframes_t   frame_rate ()               const { return FRAME_RATE; }
	nframes64_t audible_frame ()            const { return _transport_frame; }
	nframes64_t transport_frame ()          const { return _transport_frame; }
	nframes_t   frames_since_cycle_start () const { return 0; }
	nframes64_t frame_time ()               const { return _frame_time; }

	void request_locate (nframes64_t frame, bool with_roll = false) { 
          _transport_frame = frame; 
        }

        void request_transport_speed (const double speed) { transport_speed = speed; }
};

class MIDIClock_SlaveTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(MIDIClock_SlaveTest);
    CPPUNIT_TEST(testStepResponse);
    CPPUNIT_TEST_SUITE_END();
    
    ISlaveSessionProxy *session_proxy;
    MIDIClock_Slave    *slave;
    
    public:
       	
        void setUp() {
          session_proxy = new TestSlaveSessionProxy ();
          slave = new MIDIClock_Slave (session_proxy);
        }
        
        void tearDown() {
        }

        void testStepResponse();

};

} // namespace ARDOUR
