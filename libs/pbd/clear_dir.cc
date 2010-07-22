#include <string>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include <glibmm/miscutils.h>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/clear_dir.h"

#include "i18n.h"

using namespace PBD;
using namespace std;

int
PBD::clear_directory (const string& dir, size_t* size, vector<string>* paths)
{
	struct dirent* dentry;
	struct stat statbuf;
	DIR* dead;
        int ret = 0;

        if ((dead = ::opendir (dir.c_str())) == 0) {
                return -1;
        }
        
        while ((dentry = ::readdir (dead)) != 0) {
                
                /* avoid '.' and '..' */
                
                if ((dentry->d_name[0] == '.' && dentry->d_name[1] == '\0') ||
                    (dentry->d_name[2] == '\0' && dentry->d_name[0] == '.' && dentry->d_name[1] == '.')) {
                        continue;
                }
                
                string fullpath = Glib::build_filename (dir, dentry->d_name);

                if (::stat (fullpath.c_str(), &statbuf)) {
                        continue;
                }
                
                if (!S_ISREG (statbuf.st_mode)) {
                        continue;
                }
                
                if (::unlink (fullpath.c_str())) {
                        error << string_compose (_("cannot remove file %1 (%2)"), fullpath, strerror (errno))
                              << endmsg;
                        ret = 1;
                }

                if (paths) {
                        paths->push_back (dentry->d_name);
                }

                if (size) {
                        *size += statbuf.st_size;
                }
        }
        
        ::closedir (dead);

        return ret;
}
