/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_export_analysis_h__
#define __ardour_export_analysis_h__

#include <map>
#include <set>
#include <cstring>
#include <boost/shared_ptr.hpp>

#include "ardour/types.h"

namespace ARDOUR {
	struct ExportAnalysis {
	public:
		ExportAnalysis ()
			: peak (0)
			, truepeak (0)
			, loudness_range (0)
			, integrated_loudness (0)
			, max_loudness_short (0)
			, max_loudness_momentary (0)
			, loudness_hist_max (0)
			, have_loudness (false)
			, have_lufs_graph (false)
			, have_dbtp (false)
			, norm_gain_factor (1.0)
			, normalized (false)
			, n_channels (1)
			, n_samples (0)
		{
			memset (peaks, 0, sizeof(peaks));
			memset (spectrum, 0, sizeof(spectrum));
			memset (loudness_hist, 0, sizeof(loudness_hist));
			memset (freq, 0, sizeof(freq));
			memset (limiter_pk, 0, sizeof(limiter_pk));
			assert (sizeof(lgraph_i) == sizeof(lgraph_s));
			assert (sizeof(lgraph_i) == sizeof(lgraph_m));
			for (size_t i = 0; i < sizeof(lgraph_i) / sizeof (float); ++i) {
				/* d compare to ebu_r128_proc.cc */
				lgraph_i [i] = -200;
				lgraph_s [i] = -200;
				lgraph_m [i] = -200;
			}
		}

		ExportAnalysis (const ExportAnalysis& other)
			: peak (other.peak)
			, truepeak (other.truepeak)
			, loudness_range (other.loudness_range)
			, integrated_loudness (other.integrated_loudness)
			, max_loudness_short (other.max_loudness_short)
			, max_loudness_momentary (other.max_loudness_momentary)
			, loudness_hist_max (other.loudness_hist_max)
			, have_loudness (other.have_loudness)
			, have_lufs_graph (other.have_lufs_graph)
			, have_dbtp (other.have_dbtp)
			, norm_gain_factor (other.norm_gain_factor)
			, normalized (other.normalized)
			, n_channels (other.n_channels)
			, n_samples (other.n_samples)
		{
			truepeakpos[0] = other.truepeakpos[0];
			truepeakpos[1] = other.truepeakpos[1];
			memcpy (peaks, other.peaks, sizeof(peaks));
			memcpy (spectrum, other.spectrum, sizeof(spectrum));
			memcpy (loudness_hist, other.loudness_hist, sizeof(loudness_hist));
			memcpy (freq, other.freq, sizeof(freq));
			memcpy (lgraph_i, other.lgraph_i, sizeof(lgraph_i));
			memcpy (lgraph_s, other.lgraph_s, sizeof(lgraph_s));
			memcpy (lgraph_m, other.lgraph_m, sizeof(lgraph_m));
			memcpy (limiter_pk, other.limiter_pk, sizeof(limiter_pk));
		}

		float peak;
		float truepeak;
		float loudness_range;
		float integrated_loudness;
		float max_loudness_short;
		float max_loudness_momentary;
		int   loudness_hist[540];
		int   loudness_hist_max;
		bool  have_loudness;
		bool  have_lufs_graph;
		bool  have_dbtp;
		float norm_gain_factor;
		bool  normalized;

		uint32_t n_channels;
		uint32_t n_samples;
		uint32_t freq[6]; // y-pos, 50, 100, 500, 1k, 5k, 10k [Hz]

		PeakData peaks[2][800];
		float spectrum[800][200];
		float lgraph_i[800];
		float lgraph_s[800];
		float lgraph_m[800];
		float limiter_pk[800];
		std::set<samplecnt_t> truepeakpos[2]; // bins with >= -1dBTB
	};

	typedef boost::shared_ptr<ExportAnalysis> ExportAnalysisPtr;
	typedef std::map<std::string, ExportAnalysisPtr> AnalysisResults;

} // namespace ARDOUR
#endif
