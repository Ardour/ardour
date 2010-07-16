#ifndef __pbd_clear_dir_h__
#define __pbd_clear_dir_h__

#include <string>
#include <vector>
#include <sys/types.h>

namespace PBD {
        int clear_directory (const std::string&, size_t* = 0, std::vector<std::string>* = 0);
}

#endif /* __pbd_clear_dir_h__ */
