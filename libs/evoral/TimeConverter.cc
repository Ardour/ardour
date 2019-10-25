/*
 * Copyright (C) 2014-2015 David Robillard <d@drobilla.net>
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

#include <stdint.h>

#include "temporal/beats.h"
#include "temporal/types.h"

#include "evoral/TimeConverter.h"
#include "evoral/types.h"

namespace Evoral {

template<typename A, typename B>
TimeConverter<A,B>::~TimeConverter()
{}

template<typename A, typename B>
B
IdentityConverter<A,B>::to(A a) const
{
	return static_cast<B>(a);
}

template<typename A, typename B>
A
IdentityConverter<A,B>::from(B b) const
{
	return static_cast<A>(b);
}

template class IdentityConverter<double, Temporal::samplepos_t>;
template class TimeConverter<double, Temporal::samplepos_t>;
template class TimeConverter<Temporal::Beats, Temporal::samplepos_t>;

} // namespace Evoral
