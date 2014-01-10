#include <sigc++/sigc++.h>
#include "midi_clock_slave_test.h"

using namespace std;
using namespace ARDOUR;

CPPUNIT_TEST_SUITE_REGISTRATION( MIDIClock_SlaveTest );

void
MIDIClock_SlaveTest::testStepResponse ()
{
  double      speed    = 1.0;
  framepos_t position = 0;

  MIDI::Parser* parser = 0;

  TestSlaveSessionProxy *sess = (TestSlaveSessionProxy *) session;
  framecnt_t period_size = 4096;
  sess->set_period_size (period_size);

  bandwidth = 1.0 / 60.0;

  framepos_t start_time = 1000000;
  start (*parser, start_time);

  update_midi_clock (*parser, start_time);
 
  for (framecnt_t i = 1; i<= 100 * period_size; i++) {
    // simulate jitter
    framecnt_t input_delta = framecnt_t (one_ppqn_in_frames + 0.1 * (double(g_random_int()) / double (RAND_MAX)) * one_ppqn_in_frames);
    
    if (i % input_delta == 0) {
      update_midi_clock (*parser, start_time + i);
    }

    if (i % period_size == 0) {
      sess->next_period ();
      speed_and_position (speed, position);
      sess->request_transport_speed (speed);
    }
  }

}


