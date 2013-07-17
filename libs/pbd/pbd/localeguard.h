/*
    Copyright (C) 1999-2010 Paul Davis 

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

#ifndef __pbd_localeguard_h__
#define __pbd_localeguard_h__

#include <string>

namespace PBD {

struct LocaleGuard {
    LocaleGuard (const char*);
    ~LocaleGuard ();
    const char* old;

	/* JE - temporary !!!! */static std::string current;
};

}; // namespace

#endif /* __pbd_localeguard_h__ */
