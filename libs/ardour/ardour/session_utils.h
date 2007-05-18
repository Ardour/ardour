
#ifndef __ardour_session_utils_h__
#define __ardour_session_utils_h__

#include <string>

namespace ARDOUR {

using std::string;

int find_session (string str, string& path, string& snapshot, bool& isnew);

};

#endif
