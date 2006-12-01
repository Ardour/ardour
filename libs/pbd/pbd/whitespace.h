#ifndef __pbd_whitespace_h__
#define __pbd_whitespace_h__

#include <string>

namespace PBD {

// returns the empty string if the entire string is whitespace
// so check length after calling.
extern void strip_whitespace_edges (std::string& str);

} // namespace PBD

#endif // __pbd_whitespace_h__
