/*
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2007-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2014 John Emmas <john@creativepost.co.uk>
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

#ifndef __ardour_gtk_doi_h__
#define __ardour_gtk_doi_h__

#ifdef interface
#undef interface
#endif

#include <gdk/gdkevents.h>
#include <glibmm/main.h>

#include "gtkmm2ext/visibility.h"

/* XXX g++ 2.95 can't compile this as pair of member function templates */

template<typename T> /*LIBGTKMM2EXT_API*/ gint idle_delete (T *obj) { delete obj; return FALSE; }
template<typename T> /*LIBGTKMM2EXT_API*/ void delete_when_idle (T *obj) {
	Glib::signal_idle().connect (sigc::bind (sigc::ptr_fun (idle_delete<T>), obj), Glib::PRIORITY_HIGH_IDLE);
}
template<typename T> /*LIBGTKMM2EXT_API*/ gint delete_on_unmap (GdkEventAny *ignored, T *obj) {
	Glib::signal_idle().connect (sigc::bind (sigc::ptr_fun (idle_delete<T>), obj), Glib::PRIORITY_HIGH_IDLE);
	return FALSE;
}

#endif /* __ardour_gtk_doi_h__ */
