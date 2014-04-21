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

#ifndef __pbd_clear_dir_h__
#define __pbd_clear_dir_h__

#include <string>
#include <vector>
#include <sys/types.h>

namespace PBD {
        int clear_directory (const std::string&, size_t* = 0, std::vector<std::string>* = 0);
        void remove_directory (const std::string& dir);
}

#endif /* __pbd_clear_dir_h__ */
