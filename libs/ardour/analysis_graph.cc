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


#include "ardour/analysis_graph.h"
#include "ardour/route.h"
#include "ardour/session.h"

#include "timecode/time.h"

#include "audiographer/process_context.h"
#include "audiographer/general/chunker.h"
#include "audiographer/general/interleaver.h"
#include "audiographer/general/analyser.h"
#include "audiographer/general/peak_reader.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace AudioGrapher;

AnalysisGraph::AnalysisGraph (Session *s)
	: _session (s)
	, _max_chunksize (8192)
	, _frames_read (0)
	, _frames_end (0)
	, _canceled (false)
{
	_buf     = (Sample *) malloc(sizeof(Sample) * _max_chunksize);
	_mixbuf  = (Sample *) malloc(sizeof(Sample) * _max_chunksize);
	_gainbuf = (float *)  malloc(sizeof(float)  * _max_chunksize);
}

AnalysisGraph::~AnalysisGraph ()
{
	free (_buf);
	free (_mixbuf);
	free (_gainbuf);
}

void
AnalysisGraph::analyze_region (boost::shared_ptr<AudioRegion> region)
{
	interleaver.reset (new Interleaver<Sample> ());
	interleaver->init (region->n_channels(), _max_chunksize);
	chunker.reset (new Chunker<Sample> (_max_chunksize));
	analyser.reset (new Analyser (
				_session->nominal_frame_rate(),
				region->n_channels(),
				_max_chunksize,
				region->length()));

	interleaver->add_output(chunker);
	chunker->add_output (analyser);

	framecnt_t x = 0;
	framecnt_t length = region->length();
	while (x < length) {
		framecnt_t chunk = std::min (_max_chunksize, length - x);
		framecnt_t n = 0;
		for (unsigned int channel = 0; channel < region->n_channels(); ++channel) {
			memset (_buf, 0, chunk * sizeof (Sample));
			n = region->read_at (_buf, _mixbuf, _gainbuf, region->position() + x, chunk, channel);
			ConstProcessContext<Sample> context (_buf, n, 1);
			if (n < _max_chunksize) {
				context().set_flag (ProcessContext<Sample>::EndOfInput);
			}
			interleaver->input (channel)->process (context);

			if (n == 0) {
				std::cerr << "AnalysisGraph::analyze_region read zero samples\n";
				break;
			}
		}
		x += n;
		_frames_read += n;
		Progress (_frames_read, _frames_end);
		if (_canceled) {
			return;
		}
	}
	_results.insert (std::make_pair (region->name(), analyser->result ()));
}

void
AnalysisGraph::analyze_range (boost::shared_ptr<Route> route, boost::shared_ptr<AudioPlaylist> pl, const std::list<AudioRange>& range)
{
	const uint32_t n_audio = route->n_inputs().n_audio();

	for (std::list<AudioRange>::const_iterator j = range.begin(); j != range.end(); ++j) {

		interleaver.reset (new Interleaver<Sample> ());
		interleaver->init (n_audio, _max_chunksize);
		chunker.reset (new Chunker<Sample> (_max_chunksize));
		analyser.reset (new Analyser (48000.f,  n_audio, _max_chunksize, (*j).length()));

		interleaver->add_output(chunker);
		chunker->add_output (analyser);

		framecnt_t x = 0;
		while (x < j->length()) {
			framecnt_t chunk = std::min (_max_chunksize, (*j).length() - x);
			framecnt_t n = 0;
			for (uint32_t channel = 0; channel < n_audio; ++channel) {
				n = pl->read (_buf, _mixbuf, _gainbuf, (*j).start + x, chunk, channel);

				ConstProcessContext<Sample> context (_buf, n, 1);
				if (n < _max_chunksize) {
					context().set_flag (ProcessContext<Sample>::EndOfInput);
				}
				interleaver->input (channel)->process (context);
			}
			x += n;
			_frames_read += n;
			Progress (_frames_read, _frames_end);
			if (_canceled) {
				return;
			}
		}

		std::string name = string_compose (_("%1 (%2..%3)"), route->name(),
				Timecode::timecode_format_sampletime (
					(*j).start,
					_session->nominal_frame_rate(),
					100, false),
				Timecode::timecode_format_sampletime (
					(*j).start + (*j).length(),
					_session->nominal_frame_rate(),
					100, false)
				);
		_results.insert (std::make_pair (name, analyser->result ()));
	}
}
