/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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


#include "ardour/analysis_graph.h"
#include "ardour/progress.h"
#include "ardour/route.h"
#include "ardour/session.h"

#include "temporal/time.h"

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
	, _samples_read (0)
	, _samples_end (0)
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
AnalysisGraph::analyze_region (boost::shared_ptr<AudioRegion> region, bool raw)
{
	analyze_region (region.get(), raw, (ARDOUR::Progress*)0);
}

void
AnalysisGraph::analyze_region (AudioRegion const* region, bool raw, ARDOUR::Progress* p)
{
	int n_channels = region->n_channels();
	if (n_channels == 0 || n_channels > _max_chunksize) {
		return;
	}
	samplecnt_t n_samples = _max_chunksize - (_max_chunksize % n_channels);

	interleaver.reset (new Interleaver<Sample> ());
	interleaver->init (n_channels, _max_chunksize);
	chunker.reset (new Chunker<Sample> (n_samples));
	analyser.reset (new Analyser (
				_session->nominal_sample_rate(),
				n_channels,
				n_samples,
				region->length_samples()));
	interleaver->add_output(chunker);
	chunker->add_output (analyser);

	samplecnt_t x = 0;
	samplecnt_t length = region->length_samples();
	while (x < length) {
		samplecnt_t chunk = std::min (_max_chunksize, length - x);
		samplecnt_t n = 0;
		for (unsigned int channel = 0; channel < region->n_channels(); ++channel) {
			memset (_buf, 0, chunk * sizeof (Sample));

			if (raw) {
				n = region->read_raw_internal (_buf, region->start_sample() + x, chunk, channel);
			} else {
				n = region->read_at (_buf, _mixbuf, _gainbuf, region->position_sample() + x, chunk, channel);
			}

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
		_samples_read += n;
		Progress (_samples_read, _samples_end);
		if (_canceled) {
			return;
		}
		if (p) {
			p->set_progress (_samples_read / (float) _samples_end);
			if (p->cancelled ()) {
				return;
			}
		}
	}
	_results.insert (std::make_pair (region->name(), analyser->result ()));
}

void
AnalysisGraph::analyze_range (boost::shared_ptr<Route> route, boost::shared_ptr<AudioPlaylist> pl, const std::list<TimelineRange>& range)
{
	const uint32_t n_audio = route->n_inputs().n_audio();
	if (n_audio == 0 || n_audio > _max_chunksize) {
		return;
	}
	const samplecnt_t n_samples = _max_chunksize - (_max_chunksize % n_audio);

	for (std::list<TimelineRange>::const_iterator j = range.begin(); j != range.end(); ++j) {

		interleaver.reset (new Interleaver<Sample> ());
		interleaver->init (n_audio, _max_chunksize);

		chunker.reset (new Chunker<Sample> (n_samples));
		analyser.reset (new Analyser (
					_session->nominal_sample_rate(),
					n_audio, n_samples, (*j).length_samples()));

		interleaver->add_output(chunker);
		chunker->add_output (analyser);

		samplecnt_t x = 0;
		const samplecnt_t rlen = j->length().samples();
		const samplepos_t rpos = j->start().samples();

		while (x < rlen) {
			samplecnt_t chunk = std::min (_max_chunksize, rlen - x);
			samplecnt_t n = 0;

			for (uint32_t channel = 0; channel < n_audio; ++channel) {
				n = pl->read (_buf, _mixbuf, _gainbuf, timepos_t (rpos + x), timecnt_t (chunk), channel).samples();

				ConstProcessContext<Sample> context (_buf, n, 1);
				if (n < _max_chunksize) {
					context().set_flag (ProcessContext<Sample>::EndOfInput);
				}
				interleaver->input (channel)->process (context);
			}
			x += n;
			_samples_read += n;
			Progress (_samples_read, _samples_end);
			if (_canceled) {
				return;
			}
		}

		std::string name = string_compose (_("%1 (%2..%3)"), route->name(),
				Timecode::timecode_format_sampletime (
					rpos,
					_session->nominal_sample_rate(),
					100, false),
				Timecode::timecode_format_sampletime (
					(*j).end().samples(),
					_session->nominal_sample_rate(),
					100, false)
				);
		_results.insert (std::make_pair (name, analyser->result ()));
	}
}
