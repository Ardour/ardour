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
			, loudness_hist_max (0)
			, have_loudness (false)
			, n_channels (1)
		{
			memset (peaks, 0, sizeof(peaks));
			memset (spectrum, 0, sizeof(spectrum));
			memset (loudness_hist, 0, sizeof(loudness_hist));
			memset (freq, 0, sizeof(freq));
		}

		ExportAnalysis (const ExportAnalysis& other)
			: loudness (other.loudness)
			, loudness_range (other.loudness_range)
			, loudness_hist_max (other.loudness_hist_max)
			, have_loudness (other.have_loudness)
			, n_channels (other.n_channels)
		{
			memcpy (peaks, other.peaks, sizeof(peaks));
			memcpy (spectrum, other.spectrum, sizeof(spectrum));
			memcpy (loudness_hist, other.loudness_hist, sizeof(loudness_hist));
			memcpy (freq, other.freq, sizeof(freq));
		}

		float loudness;
		float loudness_range;
		int loudness_hist[540];
		int loudness_hist_max;
		bool have_loudness;

		uint32_t n_channels;
		uint32_t freq[6]; // y-pos, 50, 100, 500, 1k, 5k, 10k [Hz]

		PeakData peaks[2][800];
		float spectrum[800][200];
	};

	typedef boost::shared_ptr<ExportAnalysis> ExportAnalysisPtr;
	typedef std::map<std::string, ExportAnalysisPtr> AnalysisResults;

} // namespace ARDOUR
#endif
