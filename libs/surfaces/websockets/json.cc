/*
 * Copyright (C) 2020 Luciano Iam <oss@lucianoiam.com>
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
 * You should have received a copy of the/GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iomanip>
#include <sstream>

#include "json.h"

using namespace ArdourSurface;

/* adapted from https://stackoverflow.com/questions/7724448/simple-json-string-escape-for-c
   CC BY-SA 4.0 license */
std::string
WebSocketsJSON::escape (const std::string &s) {
    std::ostringstream o;

    for (std::string::const_iterator it = s.begin(); it != s.end(); ++it) {
        if (*it == '"' || *it == '\\' || ('\x00' <= *it && *it <= '\x1f')) {
            o << "\\u" << std::hex << std::setw (4) << std::setfill ('0') << static_cast<int>(*it);
        } else {
            o << *it;
        }
    }
    
    return o.str ();
}
