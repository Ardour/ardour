/*
 * Copyright (C) 2010-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libpbd_openuri_h__
#define __libpbd_openuri_h__

#include <string>

#include "pbd/libpbd_visibility.h"

namespace PBD {
        LIBPBD_API bool open_uri (const char*);
        LIBPBD_API bool open_uri (const std::string&);
        LIBPBD_API bool open_folder (const std::string&);
}

#endif
