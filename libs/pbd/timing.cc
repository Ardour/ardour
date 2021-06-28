/*
 * Copyright (C) 2014-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015-2018 John Emmas <john@creativepost.co.uk>
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

#include "pbd/timing.h"

#include <sstream>
#include <limits>
#include <algorithm>

namespace PBD {

bool
get_min_max_avg_total (const std::vector<microseconds_t>& values, microseconds_t& min, microseconds_t& max, microseconds_t& avg, microseconds_t& total)
{
	if (values.empty()) {
		return false;
	}

	total = 0;
	min = std::numeric_limits<microseconds_t>::max();
	max = 0; avg = 0;

	for (std::vector<microseconds_t>::const_iterator ci = values.begin(); ci != values.end(); ++ci) {
		total += *ci;
		min = std::min (min, *ci);
		max = std::max (max, *ci);
	}

	avg = total / values.size();
	return true;
}

std::string
timing_summary (const std::vector<microseconds_t>& values)
{
	std::ostringstream oss;

	microseconds_t min, max, avg, total;

	if (get_min_max_avg_total (values, min, max, avg, total)) {
		oss << "Count: " << values.size()
		    << " Min: " << min
		    << " Max: " << max
		    << " Total: " << total
		    << " Avg: " << avg << " (" << avg / 1000 << " msecs)"
		    << std::endl;
	}
	return oss.str();
}

} // namespace PBD
