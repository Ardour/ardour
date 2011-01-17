#ifndef __ardour_panner_manager_h__
#define __ardour_panner_manager_h__

#include <dlfcn.h>
#include "ardour/panner.h"
#include "ardour/session_handle.h"

namespace ARDOUR {

struct PannerInfo {
    PanPluginDescriptor descriptor;
    void* module;
    
    PannerInfo (PanPluginDescriptor& d, void* handle) 
            : descriptor (d)
            , module (handle)
    {}
    
    ~PannerInfo () { 
            dlclose (module);
    }
};
        
class PannerManager : public ARDOUR::SessionHandlePtr
{
  public:
    ~PannerManager ();
    static PannerManager& instance ();
    
    void discover_panners ();
    std::list<PannerInfo*> panner_info;

    PannerInfo* select_panner (ChanCount in, ChanCount out);
    
  private:
    PannerManager();
    static PannerManager* _instance;

    PannerInfo* get_descriptor (std::string path);
    int panner_discover (std::string path);
};

} // namespace

#endif /* __ardour_panner_manager_h__ */
