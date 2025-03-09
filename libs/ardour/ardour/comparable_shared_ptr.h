/*
 * Copyright (C) 2011 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014 John Emmas <john@creativepost.co.uk>
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

#pragma once

#include "ardour/libardour_visibility.h"

#include <boost/operators.hpp>

#include <memory>

namespace ARDOUR {

template<typename T>
class /*LIBARDOUR_API*/ ComparableSharedPtr : public std::shared_ptr<T>
                          , public boost::less_than_comparable<ComparableSharedPtr<T> >
{
  public:
	ComparableSharedPtr () {}
	template<class Y>
	explicit ComparableSharedPtr (Y * p) : std::shared_ptr<T> (p) {}

	template<class Y, class D>
	ComparableSharedPtr (Y * p, D d) : std::shared_ptr<T> (p, d) {}

	template<class Y, class D, class A>
	ComparableSharedPtr (Y * p, D d, A a) : std::shared_ptr<T> (p, d, a) {}

	ComparableSharedPtr (ComparableSharedPtr const & r) : std::shared_ptr<T> (r) {}

	ComparableSharedPtr& operator=(ComparableSharedPtr const& r) {
		std::shared_ptr<T>(r).swap(*this);
		return *this;
	}

	template<class Y>
	ComparableSharedPtr(ComparableSharedPtr<Y> const & r) : std::shared_ptr<T> (r) {}

	template<class Y>
	ComparableSharedPtr(ComparableSharedPtr<Y> const & r, T * p) : std::shared_ptr<T> (r, p) {}

	template<class Y>
	bool operator< (ComparableSharedPtr<Y> const & other) const { return **this < *other; }
};


} // namespace ARDOUR

