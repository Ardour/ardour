/*
 * Copyright (C) 2008-2016 David Robillard <d@drobilla.net>
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

#ifndef EVORAL_CURVE_HPP
#define EVORAL_CURVE_HPP

#include <inttypes.h>
#include <boost/utility.hpp>

#include "temporal/timeline.h"

#include "evoral/visibility.h"

namespace Evoral {

class ControlList;

class LIBEVORAL_API Curve : public boost::noncopyable
{
public:
	Curve (const ControlList& cl);

	bool rt_safe_get_vector (Temporal::timepos_t const & x0, Temporal::timepos_t const & x1, float *arg, int32_t veclen) const;
	void get_vector (Temporal::timepos_t const & x0, Temporal::timepos_t const & x1, float *arg, int32_t veclen) const;

	void solve () const;

	void mark_dirty() const { _dirty = true; }

private:
	double multipoint_eval (Temporal::timepos_t const & x) const;

	void _get_vector (Temporal::timepos_t const & x0, Temporal::timepos_t const & x1, float *arg, int32_t veclen) const;

	mutable bool       _dirty;
	const ControlList& _list;
};

} // namespace Evoral

#endif // EVORAL_CURVE_HPP

