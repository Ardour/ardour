/*
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <sstream>

#include "pbd/gstdio_compat.h"
#include "pbd/error.h"
#include "pbd/compose.h"

#include "gtkmm2ext/cursors.h"

#include "pbd/i18n.h"

using namespace Gtkmm2ext;

CursorInfo::Infos CursorInfo::infos;

CursorInfo::CursorInfo (const std::string& n, int hotspot_x, int hotspot_y)
        : name (n)
        , x (hotspot_x)
        , y (hotspot_y)
{
}

int
CursorInfo::load_cursor_info (const std::string& path)
{
	gchar *buf = NULL;
	if (!g_file_get_contents (path.c_str(), &buf, NULL, NULL))  {
		return -1;
	}
	std::stringstream infofile (buf);
	g_free (buf);

        std::string name;
        int x;
        int y;
	bool parse_ok;
	int line_number = 1;

        do {
		parse_ok = false;
		infofile >> name;
                if (!infofile) {
			/* failing here is OK ... EOF */
			parse_ok = true;
                        break;
                }
		infofile >> x;
                if (!infofile) {
                        break;
                }
		infofile >> y;
                if (!infofile) {
                        break;
                }

                parse_ok = true;
		line_number++;

                infos[name] = new CursorInfo (name, x, y);

        } while (true);

	if (!parse_ok) {
		PBD::error << string_compose (_("cursor hotspots info file %1 has an error on line %2"), path, line_number) << endmsg;
		infos.clear ();
		return -1;
	}

        return 0;
}

void
CursorInfo::drop_cursor_info ()
{
        infos.clear ();
}

CursorInfo*
CursorInfo::lookup_cursor_info (const std::string& name)
{
        Infos::iterator i = infos.find (name);

        if (i == infos.end()) {
                return 0;
        }
        return i->second;
}
