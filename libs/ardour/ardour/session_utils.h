
#ifndef __ardour_session_utils_h__
#define __ardour_session_utils_h__

#include <string>

namespace ARDOUR {

int find_session (std::string str, std::string& path, std::string& snapshot, bool& isnew);

};

#endif
