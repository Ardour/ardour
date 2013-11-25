/*
    Copyright (C) 2012 Paul Davis 
    Author: Sakari Bergen

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

#include "audiographer/general/sample_format_converter.h"

#include "audiographer/exception.h"
#include "audiographer/type_utils.h"
#include "private/gdither/gdither.h"

#include <boost/format.hpp>

namespace AudioGrapher
{

template <typename TOut>
SampleFormatConverter<TOut>::SampleFormatConverter (ChannelCount channels) :
  channels (channels),
  dither (0),
  data_out_size (0),
  data_out (0),
  clip_floats (false)
{
}

template <>
void
SampleFormatConverter<float>::init (framecnt_t max_frames, int /* type */, int data_width)
{
	if (throw_level (ThrowObject) && data_width != 32) {
		throw Exception (*this, "Unsupported data width");
	}
	init_common (max_frames);
	dither = gdither_new (GDitherNone, channels, GDitherFloat, data_width);
}

template <>
void
SampleFormatConverter<int32_t>::init (framecnt_t max_frames, int type, int data_width)
{
	// GDither is broken with GDither32bit if the dither depth is bigger than 24
	if(throw_level (ThrowObject) && data_width > 24) {
		throw Exception (*this, "Trying to use SampleFormatConverter<int32_t> a data width > 24");
	}
	init_common (max_frames);
	dither = gdither_new ((GDitherType) type, channels, GDither32bit, data_width);
}

template <>
void
SampleFormatConverter<int16_t>::init (framecnt_t max_frames, int type, int data_width)
{
	if (throw_level (ThrowObject) && data_width > 16) {
		throw Exception (*this, boost::str(boost::format
		    ("Data width (%1) too large for int16_t")
		    % data_width));
	}
	init_common (max_frames);
	dither = gdither_new ((GDitherType) type, channels, GDither16bit, data_width);
}

template <>
void
SampleFormatConverter<uint8_t>::init (framecnt_t max_frames, int type, int data_width)
{
	if (throw_level (ThrowObject) && data_width > 8) {
		throw Exception (*this, boost::str(boost::format
		    ("Data width (%1) too large for uint8_t")
		    % data_width));
	}
	init_common (max_frames);
	dither = gdither_new ((GDitherType) type, channels, GDither8bit, data_width);
}

template <typename TOut>
void
SampleFormatConverter<TOut>::init_common (framecnt_t max_frames)
{
	reset();
	if (max_frames  > data_out_size) {

		delete[] data_out;

		data_out = new TOut[max_frames];
		data_out_size = max_frames;
	}
}

template <typename TOut>
SampleFormatConverter<TOut>::~SampleFormatConverter ()
{
	reset();
}

template <typename TOut>
void
SampleFormatConverter<TOut>::reset()
{
	if (dither) {
		gdither_free (dither);
		dither = 0;
	}
	
	delete[] data_out;
	data_out_size = 0;
	data_out = 0;
	
	clip_floats = false;
}

/* Basic const version of process() */
template <typename TOut>
void
SampleFormatConverter<TOut>::process (ProcessContext<float> const & c_in)
{
	float const * const data = c_in.data();
	
	check_frame_and_channel_count (c_in.frames (), c_in.channels ());

	/* Do conversion */

	for (uint32_t chn = 0; chn < c_in.channels(); ++chn) {
		gdither_runf (dither, chn, c_in.frames_per_channel (), data, data_out);
	}

	/* Write forward */

	ProcessContext<TOut> c_out(c_in, data_out);
	this->output (c_out);
}

/* Basic non-const version of process(), calls the const one */
template<typename TOut>
void
SampleFormatConverter<TOut>::process (ProcessContext<float> & c_in)
{
	process (static_cast<ProcessContext<float> const &> (c_in));
}

/* template specialization for float, in-place processing (non-const) */
template<>
void
SampleFormatConverter<float>::process (ProcessContext<float> & c_in)
{
	framecnt_t frames = c_in.frames();
	float * data = c_in.data();
	
	if (clip_floats) {
		for (framecnt_t x = 0; x < frames; ++x) {
			if (data[x] > 1.0f) {
				data[x] = 1.0f;
			} else if (data[x] < -1.0f) {
				data[x] = -1.0f;
			}
		}
	}

	output (c_in);
}

/* template specialized const version, copies the data, and calls the non-const version */
template<>
void
SampleFormatConverter<float>::process (ProcessContext<float> const & c_in)
{
	// Make copy of data and pass it to non-const version
	check_frame_and_channel_count (c_in.frames(), c_in.channels());
	TypeUtils<float>::copy (c_in.data(), data_out, c_in.frames());
	
	ProcessContext<float> c (c_in, data_out);
	process (c);
}

template<typename TOut>
void
SampleFormatConverter<TOut>::check_frame_and_channel_count (framecnt_t frames, ChannelCount channels_)
{
	if (throw_level (ThrowStrict) && channels_ != channels) {
		throw Exception (*this, boost::str (boost::format
			("Wrong channel count given to process(), %1% instead of %2%")
			% channels_ % channels));
	}
	
	if (throw_level (ThrowProcess) && frames  > data_out_size) {
		throw Exception (*this, boost::str (boost::format
			("Too many frames given to process(), %1% instad of %2%")
			% frames % data_out_size));
	}
}

template class SampleFormatConverter<uint8_t>;
template class SampleFormatConverter<int16_t>;
template class SampleFormatConverter<int32_t>;
template class SampleFormatConverter<float>;

} // namespace
