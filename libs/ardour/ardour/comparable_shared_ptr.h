/*
    Copyright (C) 2011 Paul Davis
    Author: Sakari Bergen

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

#ifndef __ardour_comparable_shared_ptr_h__
#define __ardour_comparable_shared_ptr_h__

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

template<typename T>
class /*LIBARDOUR_API*/ ComparableSharedPtr : public boost::shared_ptr<T>
                          , public boost::less_than_comparable<ComparableSharedPtr<T> >
{
  public:
	ComparableSharedPtr () {}
	template<class Y>
	explicit ComparableSharedPtr (Y * p) : boost::shared_ptr<T> (p) {}

	template<class Y, class D>
	ComparableSharedPtr (Y * p, D d) : boost::shared_ptr<T> (p, d) {}

	template<class Y, class D, class A>
	ComparableSharedPtr (Y * p, D d, A a) : boost::shared_ptr<T> (p, d, a) {}

	ComparableSharedPtr (ComparableSharedPtr const & r) : boost::shared_ptr<T> (r) {}

	template<class Y>
	ComparableSharedPtr(ComparableSharedPtr<Y> const & r) : boost::shared_ptr<T> (r) {}

	template<class Y>
	ComparableSharedPtr(ComparableSharedPtr<Y> const & r, T * p) : boost::shared_ptr<T> (r, p) {}

	template<class Y>
	bool operator< (ComparableSharedPtr<Y> const & other) const { return **this < *other; }
};


} // namespace ARDOUR

#endif // __ardour_comparable_shared_ptr_h__
