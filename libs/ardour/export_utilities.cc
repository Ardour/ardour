/*
    Copyright (C) 1999-2008 Paul Davis

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

/* see gdither.cc for why we have to do this */

#define	_ISOC9X_SOURCE	1
#define _ISOC99_SOURCE	1
#include <cmath>
#undef  _ISOC99_SOURCE
#undef  _ISOC9X_SOURCE
#undef  __USE_SVID 
#define __USE_SVID 1
#include <cstdlib>
#undef  __USE_SVID

#include <unistd.h>
#include <inttypes.h>
#include <float.h>

/* ...*/

#include <ardour/export_utilities.h>

#include <string.h>

#include <ardour/export_failed.h>
#include <ardour/gdither.h>
#include <ardour/dB.h>
#include <pbd/failed_constructor.h>

#include "i18n.h"

using namespace PBD;

namespace ARDOUR
{
/* SampleRateConverter */

SampleRateConverter::SampleRateConverter (uint32_t channels, nframes_t in_rate, nframes_t out_rate, int quality) :
  channels (channels),
  leftover_frames (0),
  max_leftover_frames (0),
  frames_in (0),
  frames_out(0),
  data_in (0),
  leftover_data (0),
  data_out (0),
  data_out_size (0),
  src_state (0)
{
	if (in_rate == out_rate) {
		active = false;
		return;
	}
	
	active = true;
	int err;

	if ((src_state = src_new (quality, channels, &err)) == 0) {
		throw ExportFailed (string_compose (X_("Cannot initialize sample rate conversion: %1"), src_strerror (err)));
	}
	
	src_data.src_ratio = out_rate / (double) in_rate;
}

SampleRateConverter::~SampleRateConverter ()
{
	if (src_state) {
		src_delete (src_state);
	}

	delete [] data_out;

	if (leftover_data) {
		free (leftover_data);
	}
}

nframes_t
SampleRateConverter::process (float * data, nframes_t frames)
{
	if (!active) {
		// Just pass it on...
		return piped_to->write (data, frames);
	}

	/* Manage memory */
	
	nframes_t out_samples_max = (nframes_t) ceil (frames * src_data.src_ratio * channels);
	if (data_out_size < out_samples_max) {

		delete[] data_out;

		data_out = new float[out_samples_max];
		src_data.data_out = data_out;
		
		max_leftover_frames = 4 * frames;
		leftover_data = (float *) realloc (leftover_data, max_leftover_frames * channels * sizeof (float));
		if (!leftover_data) {
			throw ExportFailed (X_("A memory allocation error occured during sample rate conversion"));
		}
		
		data_out_size = out_samples_max;
	}

	/* Do SRC */

	data_in = data;
	frames_in = frames;

	int err;
	int cnt = 0;
	nframes_t frames_out_total = 0;
	
	do {
		src_data.output_frames = out_samples_max / channels;
		src_data.end_of_input = end_of_input;
		src_data.data_out = data_out;

		if (leftover_frames > 0) {

			/* input data will be in leftover_data rather than data_in */

			src_data.data_in = leftover_data;

			if (cnt == 0) {
				
				/* first time, append new data from data_in into the leftover_data buffer */

				memcpy (leftover_data + (leftover_frames * channels), data_in, frames_in * channels * sizeof(float));
				src_data.input_frames = frames_in + leftover_frames;
			} else {
				
				/* otherwise, just use whatever is still left in leftover_data; the contents
					were adjusted using memmove() right after the last SRC call (see
					below)
				*/

				src_data.input_frames = leftover_frames;
			}
				
		} else {

			src_data.data_in = data_in;
			src_data.input_frames = frames_in;

		}

		++cnt;

		if ((err = src_process (src_state, &src_data)) != 0) {
			throw ExportFailed (string_compose ("An error occured during sample rate conversion: %1", src_strerror (err)));
		}
	
		frames_out = src_data.output_frames_gen;
		leftover_frames = src_data.input_frames - src_data.input_frames_used;

		if (leftover_frames > 0) {
			if (leftover_frames > max_leftover_frames) {
				error << _("warning, leftover frames overflowed, glitches might occur in output") << endmsg;
				leftover_frames = max_leftover_frames;
			}
			memmove (leftover_data, (char *) (src_data.data_in + (src_data.input_frames_used * channels)),
					leftover_frames * channels * sizeof(float));
		}
		
		
		nframes_t frames_written = piped_to->write (data_out, frames_out);
		if (frames_written < 0) {
			return frames_written;
		} else {
			frames_out_total += frames_written;
		}

	} while (leftover_frames > frames_in);

	
	return frames_out_total;
}

/* SampleFormatConverter */

template <typename TOut>
SampleFormatConverter<TOut>::SampleFormatConverter (uint32_t channels, ExportFormatBase::DitherType type, int data_width_) :
  channels (channels),
  data_width (data_width_),
  dither (0),
  data_out_size (0),
  data_out (0),
  clip_floats (false)
{
	if (data_width != 24) {
		data_width = sizeof (TOut) * 8;
	}
	
	GDitherSize dither_size = GDitherFloat;

	switch (data_width) {
	case 8:
		dither_size = GDither8bit;
		break;

	case 16:
		dither_size = GDither16bit;
		break;
	case 24:
		dither_size = GDither32bit;
	}
	
	dither = gdither_new ((GDitherType) type, channels, dither_size, data_width);
}

template <typename TOut>
SampleFormatConverter<TOut>::~SampleFormatConverter ()
{
	if (dither) {
		gdither_free (dither);
	}

	delete[] data_out;
}

template <typename TOut>
nframes_t
SampleFormatConverter<TOut>::process (float * data, nframes_t frames)
{
	/* Make sure we have enough memory allocated */
	
	size_t data_size = channels * frames * sizeof (TOut);
	if (data_size  > data_out_size) {

		delete[] data_out;

		data_out = new TOut[data_size];
		data_out_size = data_size;
	}
	
	/* Do conversion */
	
	if (data_width < 32) {
		for (uint32_t chn = 0; chn < channels; ++chn) {
			gdither_runf (dither, chn, frames, data, data_out);
		}
	} else {
		for (uint32_t chn = 0; chn < channels; ++chn) {
			
			TOut * ob = data_out;
			const double int_max = (float) INT_MAX;
			const double int_min = (float) INT_MIN;
		
			nframes_t i;
			for (nframes_t x = 0; x < frames; ++x) {
				i = chn + (x * channels);
			
				if (data[i] > 1.0f) {
					ob[i] = static_cast<TOut> (INT_MAX);
				} else if (data[i] < -1.0f) {
					ob[i] = static_cast<TOut> (INT_MIN);
				} else {
					if (data[i] >= 0.0f) {
						ob[i] = lrintf (int_max * data[i]);
					} else {
						ob[i] = - lrintf (int_min * data[i]);
					}
				}
			}
		}
	}
	
	/* Write forward */
	
	return GraphSinkVertex<float, TOut>::piped_to->write (data_out, frames);
}

template<>
nframes_t
SampleFormatConverter<float>::process (float * data, nframes_t frames)
{
	if (clip_floats) {
		for (nframes_t x = 0; x < frames * channels; ++x) {
			if (data[x] > 1.0f) {
				data[x] = 1.0f;
			} else if (data[x] < -1.0f) {
				data[x] = -1.0f;
			} 
		}
	}
	
	return piped_to->write (data, frames);
}

template class SampleFormatConverter<short>;
template class SampleFormatConverter<int>;
template class SampleFormatConverter<float>;

/* Normalizer */

Normalizer::Normalizer (uint32_t channels, float target_dB) :
  channels (channels),
  enabled (false)
{
	target = dB_to_coefficient (target_dB);	
	
	if (target == 1.0f) {
		/* do not normalize to precisely 1.0 (0 dBFS), to avoid making it appear
		   that we may have clipped.
		*/
		target -= FLT_EPSILON;
	}
}

Normalizer::~Normalizer ()
{

}

void
Normalizer::set_peak (float peak)
{	
	if (peak == 0.0f || peak == target) {
		/* don't even try */
		enabled = false;
	} else {
		enabled = true;
		gain = target / peak;
	}
}

nframes_t
Normalizer::process (float * data, nframes_t frames)
{
	if (enabled) {
		for (nframes_t i = 0; i < (channels * frames); ++i) {
			data[i] *= gain;
		}
	}
	return piped_to->write (data, frames);
}

};
