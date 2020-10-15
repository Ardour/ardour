/* This file is part of Temporal.
 * Copyright (C) 2009 David Robillard <http://drobilla.net>
 * Copyright (C) 2009 Paul Davis
 *
 * Temporal is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Temporal is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __libtemporal_time_converter_h__
#define __libtemporal_time_converter_h__

#include "temporal/timeline.h"
#include "temporal/visibility.h"

namespace Temporal {

class TempoMap;

/** A bidirectional converter between two different time units.
 *
 * Think of the conversion method names as if they are written in-between
 * the two template parameters (i.e. "A <name> B").
 *
 * _origin_b should be the origin for conversion in the units of B.
 * That is, there is some point in time _origin, such that:
 *
 *    to()   converts a time _origin + a into an offset from _origin_b in units of B.
 *    from() converts a time _origin + b into an offset from _origin_b in units of A.
 */
template<typename A, typename B, typename C>
class LIBTEMPORAL_TEMPLATE_API TimeConverter {
public:
	TimeConverter () : _origin (0) {}
	TimeConverter (B ob) : _origin (ob) {}
	virtual ~TimeConverter();

	/** Convert A time to C time (A to C) */
	virtual C to(A a) const = 0;

	/** Convert C time to A time (A from C) */
	virtual A from(C b) const = 0;

	B origin () const {
		return _origin;
	}

	void set_origin (B o) {
		_origin = o;
	}

protected:
	B _origin;
};

class LIBTEMPORAL_API DistanceMeasure
{
  public:
	DistanceMeasure (timepos_t const & o)
		: _origin (o) {}

	timepos_t operator() (Temporal::timecnt_t const & duration, Temporal::TimeDomain canonical_domain) const;
	timepos_t origin() const { return _origin; }
	void      set_origin (timepos_t const &);

  private:
	timepos_t        _origin;
};


} // namespace Temporal

#endif /* __libtemporal_time_converter_h__ */
