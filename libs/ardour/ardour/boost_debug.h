/*
    Copyright (C) 2016 Paul Davis
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

#ifndef __libardour_boost_debug_h__
#define __libardour_boost_debug_h__

#include "pbd/boost_debug.h"

/* these defines are intended to be switched on as-needed. They will not work
   unless the program was configured with --boost-sp-debug
*/

//#define BOOST_MARK_ROUTE(p) boost_debug_shared_ptr_mark_interesting((p).get(),"Route")
//#define BOOST_MARK_TRACK(p) boost_debug_shared_ptr_mark_interesting((p).get(),"Track")
//#define BOOST_MARK_VCA(p) boost_debug_shared_ptr_mark_interesting((p).get(),"ControlMaster")
//#define BOOST_SHOW_POINTERS() boost_debug_list_ptrs()

#define BOOST_MARK_ROUTE(p)
#define BOOST_MARK_TRACK(p)
#define BOOST_MARK_VCA(p)
#define BOOST_SHOW_POINTERS()


#endif /* __libardour_boost_debug_h__ */
