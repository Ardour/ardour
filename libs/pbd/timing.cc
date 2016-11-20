/*
    Copyright (C) 2014 Tim Mayberry

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

#include "pbd/timing.h"

#include <sstream>
#include <limits>

#ifdef COMPILER_MSVC
#undef min
#undef max
#endif

namespace PBD {

bool
get_min_max_avg_total (const std::vector<uint64_t>& values, uint64_t& min, uint64_t& max, uint64_t& avg, uint64_t& total)
{
	if (values.empty()) {
		return false;
	}

	total = 0;
	min = std::numeric_limits<uint64_t>::max();
	max = 0; avg = 0;

	for (std::vector<uint64_t>::const_iterator ci = values.begin(); ci != values.end(); ++ci) {
		total += *ci;
		min = std::min (min, *ci);
		max = std::max (max, *ci);
	}

	avg = total / values.size();
	return true;
}

std::string
timing_summary (const std::vector<uint64_t>& values)
{
	std::ostringstream oss;

	uint64_t min, max, avg, total;

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
