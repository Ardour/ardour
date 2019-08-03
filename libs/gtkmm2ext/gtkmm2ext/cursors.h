/*
 * Copyright (C) 2014-2015 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __gtkmm2ext_cursor_info_h___
#define __gtkmm2ext_cursor_info_h___

#include <string>
#include <map>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API CursorInfo
{
    public:
        static CursorInfo* lookup_cursor_info (const std::string& image_name);
        static int load_cursor_info (const std::string& path);
        static void drop_cursor_info ();

        std::string name;
        int x;
        int y;

    private:
        CursorInfo (const std::string& image_name, int hotspot_x, int hotspot_y);

        typedef std::map<std::string,CursorInfo*> Infos;
        static Infos infos;
};

} /* namespace */

#endif /* __gtkmm2ext_cursor_info_h___ */
