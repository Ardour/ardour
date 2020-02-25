/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libardour_boost_debug_h__
#define __libardour_boost_debug_h__

#include "pbd/boost_debug.h"

/* these defines are intended to be switched on as-needed. They will not work
   unless the program was configured with --boost-sp-debug
*/

//#define BOOST_MARK_ROUTE(p)  boost_debug_shared_ptr_mark_interesting ((p).get(),"Route")
//#define BOOST_MARK_TRACK(p)  boost_debug_shared_ptr_mark_interesting ((p).get(),"Track")
//#define BOOST_MARK_VCA(p)    boost_debug_shared_ptr_mark_interesting ((p).get(),"ControlMaster")
//#define BOOST_MARK_REGION(p) boost_debug_shared_ptr_mark_interesting ((p).get(), "Region")
//#define BOOST_MARK_SOURCE(p) boost_debug_shared_ptr_mark_interesting ((p).get(), "Source")
//#define BOOST_MARK_TMM(p)    boost_debug_shared_ptr_mark_interesting ((p).get(), "TransportMaster")

#define BOOST_MARK_ROUTE(p)
#define BOOST_MARK_TRACK(p)
#define BOOST_MARK_VCA(p)
#define BOOST_MARK_REGION(p)
#define BOOST_MARK_SOURCE(p)
#define BOOST_MARK_TMM(p)

#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
#define BOOST_SHOW_POINTERS() boost_debug_list_ptrs()
#else
#define BOOST_SHOW_POINTERS()
#endif

#endif /* __libardour_boost_debug_h__ */
