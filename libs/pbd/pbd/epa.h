#ifndef __libpbd_epa_h__
#define __libpbd_epa_h__

#include <map>
#include <string>

namespace PBD {

class EnvironmentalProtectionAgency {
  public:
    EnvironmentalProtectionAgency ();
    ~EnvironmentalProtectionAgency ();

    void restore ();
    void save ();

    static EnvironmentalProtectionAgency* get_global_epa () { return _global_epa; }
    static void set_global_epa (EnvironmentalProtectionAgency* epa) { _global_epa = epa; }

  private:
    std::map<std::string,std::string> e;
    static EnvironmentalProtectionAgency* _global_epa;
};

}

#endif /* __libpbd_epa_h__ */
