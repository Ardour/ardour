#include <pbd/basename.h>
#include <glibmm/miscutils.h>

using Glib::ustring;

ustring
PBD::basename_nosuffix (ustring str)
{
	ustring base = Glib::path_get_basename (str);

	return base.substr (0, base.find_last_of ('.'));

}
