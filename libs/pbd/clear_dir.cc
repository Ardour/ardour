/*
    Copyright (C) 2012 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef COMPILER_MSVC
#include <io.h>      // Microsoft's nearest equivalent to <unistd.h>
using PBD::readdir;
using PBD::opendir;
using PBD::closedir;
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include <string>
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
