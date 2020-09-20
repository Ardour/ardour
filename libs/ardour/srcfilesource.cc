/*
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/error.h"
#include "pbd/failed_constructor.h"

#include "ardour/audiofilesource.h"
#include "ardour/debug.h"
#include "ardour/srcfilesource.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

const uint32_t SrcFileSource::max_blocksize = 2097152U; /* see AudioDiskstream::do_refill_with_alloc, max */

SrcFileSource::SrcFileSource (Session& s, boost::shared_ptr<AudioFileSource> src, SrcQuality srcq)
	: Source(s, DataType::AUDIO, src->name(), Flag (src->flags() & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy)))
	, AudioFileSource (s, src->path(), Flag (src->flags() & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy)))
	, _source (src)
	, _src_state (0)
	, _source_position(0)
	, _target_position(0)
	, _fract_position(0)
{
	assert(_source->n_channels() == 1);

	int src_type = SRC_SINC_BEST_QUALITY;

	switch (srcq) {
		case SrcBest:
			src_type = SRC_SINC_BEST_QUALITY;
			break;
		case SrcGood:
			src_type = SRC_SINC_MEDIUM_QUALITY;
			break;
		case SrcQuick:
			src_type = SRC_SINC_FASTEST;
			break;
		case SrcFast:
			src_type = SRC_ZERO_ORDER_HOLD;
			break;
		case SrcFastest:
			src_type = SRC_LINEAR;
			break;
	}


	_ratio = s.nominal_sample_rate() /  _source->sample_rate();
	_src_data.src_ratio = _ratio;

	src_buffer_size = ceil((double)max_blocksize / _ratio) + 2;
	_src_buffer = new float[src_buffer_size];

	int err;
	if ((_src_state = src_new (src_type, 1, &err)) == 0) {
		error << string_compose(_("Import: src_new() failed : %1"), src_strerror (err)) << endmsg ;
		throw failed_constructor ();
	}
}

SrcFileSource::~SrcFileSource ()
{
	DEBUG_TRACE (DEBUG::AudioPlayback, "SrcFileSource::~SrcFileSource\n");
	_src_state = src_delete (_src_state) ;
	delete [] _src_buffer;
}

void
SrcFileSource::close ()
{
	boost::shared_ptr<FileSource> fs = boost::dynamic_pointer_cast<FileSource> (_source);
	if (fs) {
		fs->close ();
	}
}

samplecnt_t
SrcFileSource::read_unlocked (Sample *dst, samplepos_t start, samplecnt_t cnt) const
{
	int err;
	const double srccnt = cnt / _ratio;

	if (_target_position != start) {
		DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("SRC: reset %1 -> %2\n", _target_position, start));
		src_reset(_src_state);
		_fract_position = 0;
		_source_position = start / _ratio;
		_target_position = start;
	}

	const samplecnt_t scnt = ceilf(srccnt - _fract_position);
	_fract_position += (scnt - srccnt);

#ifndef NDEBUG
	if (scnt >= src_buffer_size) {
		DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("SRC: CRASH AHEAD :)  %1 >= %2 (fract=%3, cnt=%4)\n",
					scnt, src_buffer_size, _fract_position, cnt));
	}
#endif
	assert(scnt < src_buffer_size);

	_src_data.input_frames = _source->read (_src_buffer, _source_position, scnt);

	if ((samplecnt_t) _src_data.input_frames * _ratio <= cnt && _source_position + scnt >= _source->length().samples()) {
		_src_data.end_of_input = true;
		DEBUG_TRACE (DEBUG::AudioPlayback, "SRC: END OF INPUT\n");
	} else {
		_src_data.end_of_input = false;
	}

	if ((samplecnt_t) _src_data.input_frames < scnt) {
		_target_position += _src_data.input_frames * _ratio;
	} else {
		_target_position += cnt;
	}

	_src_data.output_frames = cnt;
	_src_data.data_in = _src_buffer;
	_src_data.data_out = dst;

	if ((err = src_process (_src_state, &_src_data))) {
		error << string_compose(_("SrcFileSource: %1"), src_strerror (err)) << endmsg ;
		return 0;
	}

	if (_src_data.end_of_input && _src_data.output_frames_gen <= 0) {
		return 0;
	}

	_source_position += _src_data.input_frames_used;

	samplepos_t saved_target = _target_position;
	samplecnt_t generated = _src_data.output_frames_gen;

	while (generated < cnt) {
		DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("SRC: recurse for %1 samples\n",  cnt - generated));
		samplecnt_t g = read_unlocked(dst + generated, _target_position, cnt - generated);
		generated += g;
		if (g == 0) break;
	}
	_target_position = saved_target;

	DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("SRC: in: %1-> want: %2 || got: %3 total: %4\n",
				_src_data.input_frames, _src_data.output_frames, _src_data.output_frames_gen, generated));

	return generated;
}
