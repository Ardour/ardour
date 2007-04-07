#ifndef __ardour_profile_h__
#define __ardour_profile_h__

#include <boost/dynamic_bitset.hpp>
#include <stdint.h>

namespace ARDOUR {

class RuntimeProfile {
  public:
    enum Element {
	    SmallScreen,
	    LastElement
    };
    
    RuntimeProfile() { bits.resize (LastElement); }
    ~RuntimeProfile() {}

    void set_small_screen() { bits[SmallScreen] = true; }
    bool get_small_screen() const { return bits[SmallScreen]; }

  private:
    boost::dynamic_bitset<uint64_t> bits;
    
};

extern RuntimeProfile* Profile;

}; // namespace ARDOUR

#endif /* __ardour_profile_h__ */
