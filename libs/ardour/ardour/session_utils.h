
#ifndef __ardour_session_utils_h__
#define __ardour_session_utils_h__

#include <string>

namespace ARDOUR {

int find_session (std::string str, std::string& path, std::string& snapshot, bool& isnew);

/**
 * Create a SessionDirectory at the path specified by
 * session_directory_path, this includes all subdirectories.
 *
 * @return true if the session directory was able to be created
 * or if it already existed, false otherwise.
 *
 * @see SessionDirectory
 */
bool create_session_directory (const std::string& session_directory_path);

};

#endif
