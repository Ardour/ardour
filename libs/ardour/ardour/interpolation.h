#include <math.h>
#include <samplerate.h>

#include "ardour/types.h"

#ifndef __interpolation_h__
#define __interpolation_h__

namespace ARDOUR {

class Interpolation {
protected:
	double   _speed;
	
	SRC_STATE*            state;
	std::vector<SRC_DATA> data;
	
	int        error;
	
	void reset_state ();
	
public:
        Interpolation ();
        ~Interpolation ();
    
        void   set_speed (double new_speed);
        void   set_target_speed (double new_speed)   {}
        double speed ()                        const { return _speed;      }
        
        void add_channel_to (int input_buffer_size, int output_buffer_size);
        void remove_channel_from (); 
 
        nframes_t interpolate (int channel, nframes_t nframes, Sample* input, Sample* output);
};

} // namespace ARDOUR

#endif