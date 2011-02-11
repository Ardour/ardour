#ifndef __libardour_proxy_controllable_h__
#define __libardour_proxy_controllable_h__

#include <boost/function.hpp>

#include "pbd/controllable.h"

namespace ARDOUR {

/** this class converts a pair of setter/getter functors into a Controllable
    so that it can be used like a regular Controllable, bound to MIDI, OSC etc.
*/

class ProxyControllable : public PBD::Controllable {
  public:
    ProxyControllable (const std::string& name, PBD::Controllable::Flag flags,
                                 boost::function1<void,double> setter,
                                 boost::function0<double> getter)
            : PBD::Controllable (name, flags)
            , _setter (setter)
            , _getter (getter)
    {}
    
    void set_value (double v) { _setter (v); }
    double get_value () const { return _getter (); }

  private:
    boost::function1<void,double> _setter;
    boost::function0<double> _getter;
};

} // namespace

#endif /* __libardour_proxy_controllable_h__ */
