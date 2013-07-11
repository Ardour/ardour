/*
    Copyright (C) 2002 Paul Davis 

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

#ifndef __ardour_gtk_doi_h__
#define __ardour_gtk_doi_h__

#ifdef interface
#undef interface
#endif

#include <gtkmm.h>

/* XXX g++ 2.95 can't compile this as pair of member function templates */

template<typename T> gint idle_delete (T *obj) { delete obj; return FALSE; }
template<typename T> void delete_when_idle (T *obj) {
	Glib::signal_idle().connect (sigc::bind (sigc::ptr_fun (idle_delete<T>), obj));
}
template<typename T> gint delete_on_unmap (GdkEventAny *ignored, T *obj) {
	Glib::signal_idle().connect (sigc::bind (sigc::ptr_fun (idle_delete<T>), obj));
	return FALSE;
}

#endif /* __ardour_gtk_doi_h__ */
