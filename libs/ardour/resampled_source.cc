/*
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2016 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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
#include "ardour/resampled_source.h"
#include "pbd/failed_constructor.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

#ifdef PLATFORM_WINDOWS
const uint32_t ResampledImportableSource::blocksize = 524288U;
#else
const uint32_t ResampledImportableSource::blocksize = 16384U;
#endif

ResampledImportableSource::ResampledImportableSource (boost::shared_ptr<ImportableSource> src, samplecnt_t rate, SrcQuality srcq)
	: source (src)
	, _src_state (0)
{
	_src_type = SRC_SINC_BEST_QUALITY;

	switch (srcq) {
	case SrcBest:
		_src_type = SRC_SINC_BEST_QUALITY;
		break;
	case SrcGood:
		_src_type = SRC_SINC_MEDIUM_QUALITY;
		break;
	case SrcQuick:
		_src_type = SRC_SINC_FASTEST;
		break;
	case SrcFast:
		_src_type = SRC_ZERO_ORDER_HOLD;
		break;
	case SrcFastest:
		_src_type = SRC_LINEAR;
		break;
	}

	_input = new float[blocksize];

	seek (0);

	_src_data.src_ratio = ((float) rate) / source->samplerate();
}

ResampledImportableSource::~ResampledImportableSource ()
{
	_src_state = src_delete (_src_state) ;
	delete [] _input;
}

samplecnt_t
ResampledImportableSource::read (Sample* output, samplecnt_t nframes)
{
	int err;
	size_t bs = floor ((float)(blocksize / source->channels())) *  source->channels();

	/* If the input buffer is empty, refill it. */
	if (_src_data.input_frames == 0) {

		_src_data.input_frames = source->read (_input, bs);

		/* The last read will not be a full buffer, so set end_of_input. */
		if ((size_t) _src_data.input_frames < bs) {
			_end_of_input = true;
		}

		_src_data.input_frames /= source->channels();
		_src_data.data_in = _input;
	}

	_src_data.data_out = output;
	_src_data.output_frames = nframes / source->channels();

	if (_end_of_input && _src_data.input_frames * _src_data.src_ratio <= _src_data.output_frames) {
		/* only set src_data.end_of_input for the last cycle.
		 *
		 * The flag only affects writing out remaining data in the
		 * internal buffer of src_state.
		 * SRC is not aware of data bufered here in _src_data.input
		 * which needs to be processed first.
		 */
		_src_data.end_of_input = true;
	}

	if ((err = src_process (_src_state, &_src_data))) {
		error << string_compose(_("Import: %1"), src_strerror (err)) << endmsg ;
		return 0 ;
	}

	/* Terminate if at end */
	if (_src_data.end_of_input && _src_data.output_frames_gen == 0) {
		return 0;
	}

	_src_data.data_in += _src_data.input_frames_used * source->channels();
	_src_data.input_frames -= _src_data.input_frames_used ;

	return _src_data.output_frames_gen * source->channels();
}

void
ResampledImportableSource::seek (samplepos_t pos)
{
	source->seek (pos);

	/* and reset things so that we start from scratch with the conversion */

	if (_src_state) {
		src_delete (_src_state);
	}

	int err;

	if ((_src_state = src_new (_src_type, source->channels(), &err)) == 0) {
		error << string_compose(_("Import: src_new() failed : %1"), src_strerror (err)) << endmsg ;
		throw failed_constructor ();
	}

	_src_data.input_frames = 0;
	_src_data.data_in = _input;
	_src_data.end_of_input = 0;
	_end_of_input = false;
}

samplepos_t
ResampledImportableSource::natural_position () const
{
        return source->natural_position() * ratio ();
}
