/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __ardour_export_analysis_h__
#define __ardour_export_analysis_h__

#include <map>
#include <cstring>
#include <boost/shared_ptr.hpp>

#include "ardour/types.h"

namespace ARDOUR {
	struct ExportAnalysis {
	public:
		ExportAnalysis ()
			: loudness (0)
			, loudness_range (0)
			, have_loudness (false)
		{
			memset (_peaks, 0, sizeof(_peaks));
			memset (_spectrum, 0, sizeof(_spectrum));
		}

		ExportAnalysis (const ExportAnalysis& other)
			: loudness (other.loudness)
			, loudness_range (other.loudness_range)
			, have_loudness (other.have_loudness)
		{
			memcpy (_peaks, other._peaks, sizeof(_peaks));
			memcpy (_spectrum, other._spectrum, sizeof(_spectrum));
		}

		float loudness;
		float loudness_range;
		bool have_loudness;
		PeakData _peaks[800];
		float _spectrum[800][256];
	};

	typedef boost::shared_ptr<ExportAnalysis> ExportAnalysisPtr;
	typedef std::map<std::string, ExportAnalysisPtr> AnalysisResults;

} // namespace ARDOUR
#endif
