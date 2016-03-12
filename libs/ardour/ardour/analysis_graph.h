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

#ifndef __ardour_analysis_graph_h__
#define __ardour_analysis_graph_h__

#include <map>
#include <set>
#include <cstring>
#include <boost/shared_ptr.hpp>

#include "ardour/audioregion.h"
#include "ardour/audioplaylist.h"
#include "ardour/export_analysis.h"
#include "ardour/libardour_visibility.h"

namespace AudioGrapher {
	class Analyser;
	template <typename T> class Chunker;
	template <typename T> class Interleaver;
}

namespace ARDOUR {
class LIBARDOUR_API AnalysisGraph {
	public:
		AnalysisGraph (ARDOUR::Session*);
		~AnalysisGraph ();

		void analyze_region (boost::shared_ptr<ARDOUR::AudioRegion>);
		void analyze_range (boost::shared_ptr<ARDOUR::Route>, boost::shared_ptr<ARDOUR::AudioPlaylist>, const std::list<AudioRange>&);
		const AnalysisResults& results () const { return _results; }

		void cancel () { _canceled = true; }
		bool canceled () const { return _canceled; }

		void set_total_frames (framecnt_t p) { _frames_end = p; }
		PBD::Signal2<void, framecnt_t, framecnt_t> Progress;

	private:
		ARDOUR::Session* _session;
		AnalysisResults  _results;
		framecnt_t       _max_chunksize;

		ARDOUR::Sample*  _buf;
		ARDOUR::Sample*  _mixbuf;
		float*           _gainbuf;
		framecnt_t       _frames_read;
		framecnt_t       _frames_end;
		bool             _canceled;

		typedef boost::shared_ptr<AudioGrapher::Analyser> AnalysisPtr;
		typedef boost::shared_ptr<AudioGrapher::Chunker<float> > ChunkerPtr;
		typedef boost::shared_ptr<AudioGrapher::Interleaver<Sample> > InterleaverPtr;

		InterleaverPtr  interleaver;
		ChunkerPtr      chunker;
		AnalysisPtr     analyser;
};
} // namespace ARDOUR
#endif
