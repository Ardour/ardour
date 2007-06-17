
#ifndef __ardour_session_utils_h__
#define __ardour_session_utils_h__

#include <string>

namespace ARDOUR {

using std::string;

int find_session (string str, string& path, string& snapshot, bool& isnew);

/**
 * Create a SessionDirectory at the path specified by
 * session_directory_path, this includes all subdirectories.
 *
 * @return true if the session directory was able to be created
 * or if it already existed, false otherwise.
 *
 * @see SessionDirectory
 */
bool create_session_directory (const string& session_directory_path);

};

#endif
