/*
    Copyright (C) 2002 Paul Barton-Davis 

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

    $Id$
*/

#ifndef __lib_pbd_foreach_h__
#define __lib_pbd_foreach_h__

template<class Iter, class T> void foreach (Iter first, Iter last, void (T::*method)()) {
	for (; first != last; ++first) {
		((*first).*method)();
	}
}

template<class Iter, class T, class A> void foreach (Iter first, Iter last, void (T::*method)(A a), A arg) {
	for (; first != last; ++first) {
		((*first).*method)(arg);
	}
}

template<class Iter, class T, class A1, class A2> void foreach (Iter first, Iter last, void (T::*method)(A1, A2), A1 arg1, A2 arg2) {
	for (; first != last; ++first) {
		((*first).*method)(arg1, arg2);
	}
}

#endif /* __lib_pbd_foreach_h__ */
