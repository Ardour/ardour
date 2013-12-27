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

#ifndef EVORAL_CURVE_HPP
#define EVORAL_CURVE_HPP

#include <inttypes.h>
#include <boost/utility.hpp>

#include "evoral/visibility.h"

namespace Evoral {

class ControlList;

class LIBEVORAL_API Curve : public boost::noncopyable
{
public:
	Curve (const ControlList& cl);

	bool rt_safe_get_vector (double x0, double x1, float *arg, int32_t veclen);
	void get_vector (double x0, double x1, float *arg, int32_t veclen);

	void solve ();

	void mark_dirty() const { _dirty = true; }

private:
	double unlocked_eval (double where);
	double multipoint_eval (double x);

	void _get_vector (double x0, double x1, float *arg, int32_t veclen);

	mutable bool       _dirty;
	const ControlList& _list;
};

} // namespace Evoral

extern "C" {
	LIBEVORAL_API void curve_get_vector_from_c (void *arg, double, double, float*, int32_t);
}

#endif // EVORAL_CURVE_HPP

