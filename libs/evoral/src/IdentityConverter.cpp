/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 *
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdint.h>
#include "evoral/TimeConverter.hpp"

typedef int64_t framepos_t; /* MUST match libs/ardour/ardour/types.h */

namespace Evoral {

template<typename A, typename B>
B
IdentityConverter<A,B>::to(A a)   const
{
	return static_cast<B>(a);
}

template<typename A, typename B>
A
IdentityConverter<A,B>::from(B b) const
{
	return static_cast<A>(b);
}

template class IdentityConverter<double, framepos_t>;
template class TimeConverter<double, framepos_t>;

} // namespace Evoral
