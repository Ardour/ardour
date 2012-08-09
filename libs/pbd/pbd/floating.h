/* Taken from
 * http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm
 *
 * Code assumed to be in the public domain.
 */

#ifndef __libpbd__floating_h__
#define __libpbd__floating_h__

namespace PBD {

union Float_t
{
    Float_t (float num = 0.0f) : f(num) {}

    // Portable extraction of components.
    bool    negative() const { return (i >> 31) != 0; }
    int32_t raw_mantissa() const { return i & ((1 << 23) - 1); }
    int32_t raw_exponent() const { return (i >> 23) & 0xFF; }
 
    int32_t i;
    float f;
};
 
/* Note: ULPS = Units in the Last Place */

static inline bool floateq (float a, float b, int max_ulps_diff)
{
    Float_t ua(a);
    Float_t ub(b);
 
    // Different signs means they do not match.
    if (ua.negative() != ub.negative()) {
        // Check for equality to make sure +0==-0
	    if (a == b) {
		    return true;
	    }
	    return false;
    }
 
    // Find the difference in ULPs.
    int ulps_diff = abs (ua.i - ub.i);

    if (ulps_diff <= max_ulps_diff) {
        return true;
    }
 
    return false;
}

} /* namespace */

#endif /* __libpbd__floating_h__ */
