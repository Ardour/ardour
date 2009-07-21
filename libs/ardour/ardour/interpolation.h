#include <math.h>
#include <samplerate.h>

#include "ardour/types.h"

#ifndef __interpolation_h__
#define __interpolation_h__

namespace ARDOUR {

class Interpolation {
 protected:
         double   _speed, _target_speed;
             
 public:
         Interpolation () { _speed = 1.0; _target_speed = 1.0; }
     
         void set_speed (double new_speed)          { _speed = new_speed; _target_speed = new_speed; }
         void set_target_speed (double new_speed)   { _target_speed = new_speed; }

         double target_speed()          const { return _target_speed; }
         double speed()                 const { return _speed; }
         
	void add_channel_to (int /*input_buffer_size*/, int /*output_buffer_size*/) {}
         void remove_channel_from () {}
  
         void reset () {}
};

// 40.24 fixpoint math
#define FIXPOINT_ONE 0x1000000

class FixedPointLinearInterpolation : public Interpolation {
    protected:
    /// speed in fixed point math
    uint64_t      phi;
    
    /// target speed in fixed point math
    uint64_t      target_phi;
    
    std::vector<uint64_t> last_phase;

    // Fixed point is just an integer with an implied scaling factor. 
    // In 40.24 the scaling factor is 2^24 = 16777216,  
    // so a value of 10*2^24 (in integer space) is equivalent to 10.0. 
    //
    // The advantage is that addition and modulus [like x = (x + y) % 2^40]  
    // have no rounding errors and no drift, and just require a single integer add.
    // (swh)
    
    static const int64_t fractional_part_mask  = 0xFFFFFF;
    static const Sample  binary_scaling_factor = 16777216.0f;
    
    public:
        
        FixedPointLinearInterpolation () : phi (FIXPOINT_ONE), target_phi (FIXPOINT_ONE) {}
    
        void set_speed (double new_speed) {
            target_phi = (uint64_t) (FIXPOINT_ONE * fabs(new_speed));
            phi = target_phi;
        }
        
        uint64_t get_phi() { return phi; }
        uint64_t get_target_phi() { return target_phi; }
        uint64_t get_last_phase() { assert(last_phase.size()); return last_phase[0]; }
        void set_last_phase(uint64_t phase) { assert(last_phase.size()); last_phase[0] = phase; }
        
        void add_channel_to (int input_buffer_size, int output_buffer_size);
        void remove_channel_from ();
         
        nframes_t interpolate (int channel, nframes_t nframes, Sample* input, Sample* output);
        void reset ();
};

 class LinearInterpolation : public Interpolation {
 protected:
    // the idea is that when the speed is not 1.0, we have to 
    // interpolate between samples and then we have to store where we thought we were. 
    // rather than being at sample N or N+1, we were at N+0.8792922
    std::vector<double> phase;
    
 public:
         void add_channel_to (int input_buffer_size, int output_buffer_size);
         void remove_channel_from ();
     
         nframes_t interpolate (int channel, nframes_t nframes, Sample* input, Sample* output);
         void reset ();
 };

class LibSamplerateInterpolation : public Interpolation {
 protected:
    std::vector<SRC_STATE*>  state;
    std::vector<SRC_DATA*>   data;
    
    int        error;
    
    void reset_state ();
    
 public:
        LibSamplerateInterpolation ();
        ~LibSamplerateInterpolation ();
    
        void   set_speed (double new_speed);
        void   set_target_speed (double /*new_speed*/) {}
        double speed ()                        const { return _speed;      }
        
        void add_channel_to (int input_buffer_size, int output_buffer_size);
        void remove_channel_from (); 
 
        nframes_t interpolate (int channel, nframes_t nframes, Sample* input, Sample* output);
        void reset() { reset_state (); }
};

} // namespace ARDOUR

#endif
