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

#include <pbd/error.h>
#include <ardour/resampled_source.h>
#include <pbd/failed_constructor.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

const uint32_t ResampledImportableSource::blocksize = 4096U;

ResampledImportableSource::ResampledImportableSource (SNDFILE* sf, SF_INFO* info, nframes_t rate)
	: ImportableSource (sf, info) 
{
	int err;
	
	sf_seek (in, 0, SEEK_SET) ;
	
	/* Initialize the sample rate converter. */
	
	if ((src_state = src_new (SRC_SINC_BEST_QUALITY, sf_info->channels, &err)) == 0) {	
		error << string_compose(_("Import: src_new() failed : %1"), src_strerror (err)) << endmsg ;
		throw failed_constructor ();
	}
	
	src_data.end_of_input = 0 ; /* Set this later. */
	
	/* Start with zero to force load in while loop. */
	
	src_data.input_frames = 0 ;
	src_data.data_in = input ;
	
	src_data.src_ratio = ((float) rate) / sf_info->samplerate ;
	
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

		src_data.input_frames = ImportableSource::read (input, blocksize);

		/* The last read will not be a full buffer, so set end_of_input. */

		if ((nframes_t) src_data.input_frames < blocksize) {
			src_data.end_of_input = SF_TRUE ;
		}		

		src_data.input_frames /= sf_info->channels;
		src_data.data_in = input ;
	} 
	
	src_data.data_out = output;

	if (!src_data.end_of_input) {
		src_data.output_frames = nframes / sf_info->channels ;
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
	
	src_data.data_in += src_data.input_frames_used * sf_info->channels ;
	src_data.input_frames -= src_data.input_frames_used ;

	return src_data.output_frames_gen * sf_info->channels;
}

