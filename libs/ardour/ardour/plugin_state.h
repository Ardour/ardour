#ifndef __ardour_plugin_state_h__
#define __ardour_plugin_state_h__

#include <map>

namespace ARDOUR {

struct PluginState {
    std::map<uint32_t,float> parameters;
};

} 

#endif /* __ardour_plugin_state_h__ */
