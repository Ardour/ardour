/*
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
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

#include <stdint.h>

#include "temporal/beats.h"
#include "temporal/types.h"
#include "temporal/time_converter.h"
#include "temporal/tempo.h"

namespace Temporal {

template<typename A, typename B, typename C>
TimeConverter<A,B,C>::~TimeConverter()
{}

template class TimeConverter<double, Temporal::samplepos_t,Temporal::samplecnt_t>;
template class TimeConverter<Temporal::Beats, Temporal::samplepos_t,Temporal::samplecnt_t>;

Temporal::timepos_t
DistanceMeasure::operator() (Temporal::timecnt_t const & duration, Temporal::TimeDomain canonical_domain) const
{
	return timepos_t (TempoMap::fetch()->full_duration_at (_origin, duration, canonical_domain));
}

void
DistanceMeasure::set_origin (timepos_t const & pos)
{
	_origin = pos;
}

} // namespace Temporal
