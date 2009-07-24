#include <math.h>
#include <samplerate.h>

#include "ardour/types.h"

#ifndef __interpolation_h__
#define __interpolation_h__

namespace ARDOUR {

class Interpolation {
 protected:
     double   _speed, _target_speed;

     // the idea is that when the speed is not 1.0, we have to 
     // interpolate between samples and then we have to store where we thought we were. 
     // rather than being at sample N or N+1, we were at N+0.8792922
     std::vector<double> phase;

             
 public:
     Interpolation ()  { _speed = 1.0; _target_speed = 1.0; }
     ~Interpolation () { phase.clear(); }
 
     void set_speed (double new_speed)          { _speed = new_speed; _target_speed = new_speed; }
     void set_target_speed (double new_speed)   { _target_speed = new_speed; }

     double target_speed()          const { return _target_speed; }
     double speed()                 const { return _speed; }
     
     void add_channel_to (int input_buffer_size, int output_buffer_size) { phase.push_back (0.0); }
     void remove_channel_from () { phase.pop_back (); }

     void reset () {
         for (size_t i = 0; i < phase.size(); i++) {
              phase[i] = 0.0;
          }
     }
};

class LinearInterpolation : public Interpolation {
 protected:
    
 public:
     nframes_t interpolate (int channel, nframes_t nframes, Sample* input, Sample* output);
};

class CubicInterpolation : public Interpolation {
 protected:
    // shamelessly ripped from Steve Harris' swh-plugins (ladspa-util.h)
    static inline float cube_interp(const float fr, const float inm1, const float
                                    in, const float inp1, const float inp2)
    {
        return in + 0.5f * fr * (inp1 - inm1 +
         fr * (4.0f * inp1 + 2.0f * inm1 - 5.0f * in - inp2 +
         fr * (3.0f * (in - inp1) - inm1 + inp2)));
    }
    
 public:
     nframes_t interpolate (int channel, nframes_t nframes, Sample* input, Sample* output);
};
 
} // namespace ARDOUR

#endif
