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
    
 public:
     nframes_t interpolate (int channel, nframes_t nframes, Sample* input, Sample* output);
};

class CubicInterpolation : public Interpolation {
 protected:
    // shamelessly ripped from Steve Harris' swh-plugins
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
 

/**
 * @class SplineInterpolation
 * 
 * @brief interpolates using cubic spline interpolation over an input period
 * 
 * Splines are piecewise cubic functions between each samples,
 * where the cubic polynomials' values, first and second derivatives are equal
 * on each sample point.
 * 
 * Those conditions are equivalent of solving the linear system of equations
 * defined by the matrix equation (all indices are zero-based):
 *  A * M = d
 *
 * where A has (n-2) rows and (n-2) columns
 *
 *  [ 4 1 0 0 ... 0 0 0 0 ]   [ M[1]   ]   [ 6*y[0] - 12*y[1] + 6*y[2] ]
 *  [ 1 4 1 0 ... 0 0 0 0 ]   [ M[2]   ]   [ 6*y[1] - 12*y[2] + 6*y[3] ]
 *  [ 0 1 4 1 ... 0 0 0 0 ]   [ M[3]   ]   [ 6*y[2] - 12*y[3] + 6*y[4] ]
 *  [ 0 0 1 4 ... 0 0 0 0 ]   [ M[4]   ]   [ 6*y[3] - 12*y[4] + 6*y[5] ]
 *            ...           *            =            ...            
 *  [ 0 0 0 0 ... 4 1 0 0 ]   [ M[n-5] ]   [ 6*y[n-6]- 12*y[n-5] + 6*y[n-4] ]
 *  [ 0 0 0 0 ... 1 4 1 0 ]   [ M[n-4] ]   [ 6*y[n-5]- 12*y[n-4] + 6*y[n-3] ]
 *  [ 0 0 0 0 ... 0 1 4 1 ]   [ M[n-3] ]   [ 6*y[n-4]- 12*y[n-3] + 6*y[n-2] ]
 *  [ 0 0 0 0 ... 0 0 1 4 ]   [ M[n-2] ]   [ 6*y[n-3]- 12*y[n-2] + 6*y[n-1] ]
 *
 *  For our purpose we use natural splines which means the boundary coefficients
 *  M[0] = M[n-1] = 0
 *
 *  The interpolation polynomial in the i-th interval then has the form
 *  p_i(x) = a3 (x - i)^3 + a2 (x - i)^2 + a1 (x - i) + a0
 *         = ((a3 * (x - i) + a2) * (x - i) + a1) * (x - i) + a0
 *     where
 *  a3 = (M[i+1] - M[i]) / 6
 *  a2 = M[i] / 2 
 *  a1 = y[i+1] - y[i] - M[i+1]/6 - M[i]/3
 *  a0 = y[i] 
 *
 *  We solve the system by LU-factoring the matrix A:
 *  A = L * U:
 *
 *  [ 4 1 0 0 ... 0 0 0 0 ]   [ 1    0    0    0   ... 0      0      0      0 ]   [ m[0] 1    0    0   ... 0      0      0      ]
 *  [ 1 4 1 0 ... 0 0 0 0 ]   [ l[0] 1    0    0   ... 0      0      0      0 ]   [ 0    m[1] 1    0   ... 0      0      0      ]
 *  [ 0 1 4 1 ... 0 0 0 0 ]   [ 0    l[1] 1    0   ... 0      0      0      0 ]   [ 0    0    m[2] 1   ... 0      0      0      ]
 *  [ 0 0 1 4 ... 0 0 0 0 ]   [ 0    0    l[2] 1   ... 0      0      0      0 ]                        ...                
 *            ...           =                     ...                          *  [ 0    0    0    0   ... 0      0      0      ]
 *  [ 0 0 0 0 ... 4 1 0 0 ]   [ 0    0    0    0   ... 1      0      0      0 ]   [ 0    0    0    0   ... 1      0      0      ]
 *  [ 0 0 0 0 ... 1 4 1 0 ]   [ 0    0    0    0   ... l[n-6] 1      0      0 ]   [ 0    0    0    0   ... m[n-5] 1      0      ]
 *  [ 0 0 0 0 ... 0 1 4 1 ]   [ 0    0    0    0   ... 0      l[n-5] 1      0 ]   [ 0    0    0    0   ... 0      m[n-4] 1      ]
 *  [ 0 0 0 0 ... 0 0 1 4 ]   [ 0    0    0    0   ... 0      0      l[n-4] 1 ]   [ 0    0    0    0   ... 0      0      m[n-3] ]
 *
 *  where the l[i] and m[i] can be precomputed.
 * 
 *  Then we solve the system A * M = L(UM) = d by first solving the system
 *    L * t = d 
 *    
 *    [ 1    0    0    0   ... 0      0      0      0 ]    [ t[0]   ]    [ 6*y[0] - 12*y[1] + 6*y[2] ]
 *    [ l[0] 1    0    0   ... 0      0      0      0 ]    [ t[1]   ]    [ 6*y[1] - 12*y[2] + 6*y[3] ]
 *    [ 0    l[1] 1    0   ... 0      0      0      0 ]    [ t[2]   ]    [ 6*y[2] - 12*y[3] + 6*y[4] ]
 *    [ 0    0    l[2] 1   ... 0      0      0      0 ]    [ t[3]   ]    [ 6*y[3] - 12*y[4] + 6*y[5] ]
 *                        ...                           *             =             ...            
 *    [ 0    0    0    0   ... 1      0      0      0 ]    [ t[n-6] ]    [ 6*y[n-6]- 12*y[n-5] + 6*y[n-4] ]
 *    [ 0    0    0    0   ... l[n-6] 1      0      0 ]    [ t[n-5] ]    [ 6*y[n-5]- 12*y[n-4] + 6*y[n-3] ]
 *    [ 0    0    0    0   ... 0      l[n-5] 1      0 ]    [ t[n-4] ]    [ 6*y[n-4]- 12*y[n-3] + 6*y[n-2] ]
 *    [ 0    0    0    0   ... 0      0      l[n-4] 1 ]    [ t[n-3] ]    [ 6*y[n-3]- 12*y[n-2] + 6*y[n-1] ]
 *    
 *    
 *  and then
 *    U * M = t
 *  
 *  [ m[0] 1    0    0   ... 0      0      0      ]   [ M[1]   ]    [ t[0]   ]
 *  [ 0    m[1] 1    0   ... 0      0      0      ]   [ M[2]   ]    [ t[1]   ]
 *  [ 0    0    m[2] 1   ... 0      0      0      ]   [ M[3]   ]    [ t[2]   ]
 *                       ...                          [ M[4]   ]    [ t[3]   ]
 *  [ 0    0    0    0   ... 0      0      0      ] *            =            
 *  [ 0    0    0    0   ... 1      0      0      ]   [ M[n-5] ]    [ t[n-6] ]
 *  [ 0    0    0    0   ... m[n-5] 1      0      ]   [ M[n-4] ]    [ t[n-5] ]
 *  [ 0    0    0    0   ... 0      m[n-4] 1      ]   [ M[n-3] ]    [ t[n-4] ]
 *  [ 0    0    0    0   ... 0      0      m[n-3] ]   [ M[n-2] ]    [ t[n-3] ]
 *  
 */
class SplineInterpolation : public Interpolation {
 protected:
    double _l[19], _m[20];
    
    inline double l(nframes_t i) {  return (i >= 19) ? _l[18] : _l[i]; }
    inline double m(nframes_t i) {  return (i >= 20) ? _m[19] : _m[i]; }
    
 public:
    SplineInterpolation();
    nframes_t interpolate (int channel, nframes_t nframes, Sample* input, Sample* output);
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
        void   set_target_speed (double new_speed)   {}
        double speed ()                        const { return _speed;      }
        
        void add_channel_to (int input_buffer_size, int output_buffer_size);
        void remove_channel_from (); 
 
        nframes_t interpolate (int channel, nframes_t nframes, Sample* input, Sample* output);
        void reset() { reset_state (); }
};

} // namespace ARDOUR

#endif
