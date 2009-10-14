/*
    Copyright (C) 2007 Paul Davis

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

#include "pbd/error.h"
#include "ardour/resampled_source.h"
#include "pbd/failed_constructor.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

const uint32_t ResampledImportableSource::blocksize = 16384U;

ResampledImportableSource::ResampledImportableSource (boost::shared_ptr<ImportableSource> src, nframes_t rate, SrcQuality srcq)
	: source (src)
{
	int err;

	source->seek (0);

	/* Initialize the sample rate converter. */

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

	if ((src_state = src_new (src_type, source->channels(), &err)) == 0) {
		error << string_compose(_("Import: src_new() failed : %1"), src_strerror (err)) << endmsg ;
		throw failed_constructor ();
	}

	src_data.end_of_input = 0 ; /* Set this later. */

	/* Start with zero to force load in while loop. */

	src_data.input_frames = 0 ;
	src_data.data_in = input ;

	src_data.src_ratio = ((float) rate) / source->samplerate();

	input = new float[blocksize];
}

ResampledImportableSource::~ResampledImportableSource ()
{
	src_state = src_delete (src_state) ;
	delete [] input;
}

nframes_t
ResampledImportableSource::read (Sample* output, nframes_t nframes)
{
	int err;

	/* If the input buffer is empty, refill it. */

	if (src_data.input_frames == 0) {

		src_data.input_frames = source->read (input, blocksize);

		/* The last read will not be a full buffer, so set end_of_input. */

		if ((nframes_t) src_data.input_frames < blocksize) {
			src_data.end_of_input = true;
		}

		src_data.input_frames /= source->channels();
		src_data.data_in = input;
	}

	src_data.data_out = output;

	if (!src_data.end_of_input) {
		src_data.output_frames = nframes / source->channels();
	} else {
		src_data.output_frames = src_data.input_frames;
	}

	if ((err = src_process (src_state, &src_data))) {
		error << string_compose(_("Import: %1"), src_strerror (err)) << endmsg ;
		return 0 ;
	}

	/* Terminate if at end */

	if (src_data.end_of_input && src_data.output_frames_gen == 0) {
		return 0;
	}

	src_data.data_in += src_data.input_frames_used * source->channels();
	src_data.input_frames -= src_data.input_frames_used ;

	return src_data.output_frames_gen * source->channels();
}

