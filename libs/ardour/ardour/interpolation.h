#include <math.h>
//#include "ardour/types.h"

typedef float Sample;
#define nframes_t uint32_t

// 40.24 fixpoint math
#define FIXPOINT_ONE 0x1000000

class Interpolation {
    protected:
    /// speed in fixed point math
    uint64_t      phi;
    
    /// target speed in fixed point math
    uint64_t      target_phi;
    
    uint64_t      last_phase;

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
        Interpolation () : phi (FIXPOINT_ONE), target_phi (FIXPOINT_ONE), last_phase (0) {}
    
        void set_speed (double new_speed) {
            target_phi = (uint64_t) (FIXPOINT_ONE * fabs(new_speed));
            phi = target_phi;
        }
        
        uint64_t get_phi () const { return phi; }
        uint64_t get_target_phi () const { return target_phi; }
        uint64_t get_last_phase () const { return last_phase; }
 
        virtual nframes_t interpolate (nframes_t nframes, Sample* input, Sample* output) = 0;
};

class LinearInterpolation : public Interpolation {
    public:
        nframes_t interpolate (nframes_t nframes, Sample* input, Sample* output);
};
