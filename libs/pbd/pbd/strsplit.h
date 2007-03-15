#ifndef __pbd_strplit_h__
#define __pbd_strplit_h__

#include <string>
#include <vector>
#include <glibmm/ustring.h>

extern void split (std::string, std::vector<std::string>&, char);
extern void split (Glib::ustring, std::vector<Glib::ustring>&, char);

#endif // __pbd_strplit_h__
