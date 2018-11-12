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
  samplecnt_t _period_size;

  double       transport_speed;
  samplepos_t _transport_sample;
  samplepos_t _sample_time;
  TempoMap    *_tempo_map;

  Tempo     tempo;
  Meter     meter;

  public:
        TestSlaveSessionProxy() :
           transport_speed  (1.0),
          _transport_sample  (0),
          _sample_time       (1000000),
          _tempo_map        (0),
          tempo             (120, 4.0),
          meter             (4.0, 4.0)
        {
          _tempo_map = new TempoMap (FRAME_RATE);
          _tempo_map->add_tempo (tempo, 0.0, 0, AudioTime);
          _tempo_map->add_meter (meter, Timecode::BBT_Time(1, 1, 0), 0, AudioTime);
        }

        // Controlling the mock object
        void        set_period_size (samplecnt_t a_size) { _period_size = a_size; }
        samplecnt_t period_size () const                 { return _period_size; }
        void next_period ()                       {
          _transport_sample += double(_period_size) * double(transport_speed);
          _sample_time += _period_size;
        }

        // Implementation
  	TempoMap&  tempo_map ()                const { return *_tempo_map; }
	samplecnt_t sample_rate ()               const { return FRAME_RATE; }
	samplepos_t audible_sample ()            const { return _transport_sample; }
	samplepos_t transport_sample ()          const { return _transport_sample; }
	pframes_t  samples_since_cycle_start () const { return 0; }
	samplepos_t sample_time ()               const { return _sample_time; }

	void request_locate (samplepos_t sample, bool with_roll = false) {
          _transport_sample = sample;
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
