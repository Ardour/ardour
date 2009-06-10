#include <math.h>
#include "ardour/types.h"

#ifndef __interpolation_h__
#define __interpolation_h__

namespace ARDOUR {

class Interpolation {
protected:
	double   _speed, _target_speed;
    	    
public:
        Interpolation () : _speed(0.0L) {}
    
        void set_speed (double new_speed)          { _speed = new_speed; }
        void set_target_speed (double new_speed)   { _target_speed = new_speed; }

        double target_speed()          const { return _target_speed; }
        double speed()                 const { return _speed; }
 
        virtual nframes_t interpolate (nframes_t nframes, Sample* input, Sample* output) = 0;
};

class LinearInterpolation : public Interpolation {
public:
        nframes_t interpolate (nframes_t nframes, Sample* input, Sample* output);
};

} // namespace ARDOUR

#endif