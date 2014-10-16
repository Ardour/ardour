/*
    Copyright (C) 2002-2009 Paul Davis

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

#ifndef __pbd_locale_guard__
#define __pbd_locale_guard__

#include "pbd/libpbd_visibility.h"

namespace PBD {

struct LIBPBD_API LocaleGuard {
    LocaleGuard (const char*);
    ~LocaleGuard ();
    const char* old;
};

}

#endif /* __pbd_locale_guard__ */
