#include <pbd/shortpath.h>

using namespace Glib;
using namespace std;

ustring
short_path (const Glib::ustring& path, ustring::size_type target_characters)
{
	ustring::size_type last_sep;
	ustring::size_type len = path.length();
	const char separator = '/';

	if (len <= target_characters) {
		return path;
	}

	if ((last_sep = path.find_last_of (separator)) == ustring::npos) {

		/* just a filename, but its too long anyway */

		if (target_characters > 3) {
			return path.substr (0, target_characters - 3) + ustring ("...");
		} else {
			/* stupid caller, just hand back the whole thing */
			return path;
		}
	}

	if (len - last_sep >= target_characters) {

		/* even the filename itself is too long */

		if (target_characters > 3) {
			return path.substr (last_sep+1, target_characters - 3) + ustring ("...");
		} else {
			/* stupid caller, just hand back the whole thing */
			return path;
		}
	}
	
	uint32_t so_far = (len - last_sep);
	uint32_t space_for = target_characters - so_far;

	if (space_for >= 3) {
		ustring res = "...";
		res += path.substr (last_sep - space_for);
		return res;
	} else {
		/* remove part of the end */
		ustring res = "...";
		res += path.substr (last_sep - space_for, len - last_sep + space_for - 3);
		res += "...";
		return res;
		
	}
}
