/*
    Copyright (C) 2009 Paul Davis 
    From an idea by Carl Hetherington.

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

#ifndef __pbd_boost_debug_h__
#define __pbd_boost_debug_h__

#include <ostream>

#include "pbd/libpbd_visibility.h"

LIBPBD_API void boost_debug_shared_ptr_mark_interesting (void* ptr, const char* type);
LIBPBD_API void boost_debug_list_ptrs ();
LIBPBD_API void boost_debug_shared_ptr_show_live_debugging (bool yn);

#endif /* __pbd_boost_debug_h__ */
