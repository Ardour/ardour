#ifndef __stupid_basename_h__
#define __stupid_basename_h__

#include <string>

namespace PBD
{
	
extern char *basename (const char *);
extern std::string basename (const std::string);
extern std::string basename_nosuffix (const std::string);

};

#endif  // __stupid_basename_h__
