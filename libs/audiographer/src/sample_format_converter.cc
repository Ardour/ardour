#include "audiographer/sample_format_converter.h"

#include "gdither/gdither.h"
#include "audiographer/exception.h"

#include <boost/format.hpp>

#include <cstring>

namespace AudioGrapher
{

template <typename TOut>
SampleFormatConverter<TOut>::SampleFormatConverter (uint32_t channels) :
  channels (channels),
  dither (0),
  data_out_size (0),
  data_out (0),
  clip_floats (false)
{
}

template <>
void
SampleFormatConverter<float>::init (nframes_t max_frames, int type, int data_width)
{
	if (data_width != 32) { throw Exception (*this, "Unsupported data width"); }
	init_common (max_frames);
	dither = gdither_new (GDitherNone, channels, GDitherFloat, data_width);
}

template <>
void
SampleFormatConverter<int32_t>::init (nframes_t max_frames, int type, int data_width)
{
	if(data_width < 24) { throw Exception (*this, "Use SampleFormatConverter<int16_t> for data widths < 24"); }
	
	init_common (max_frames);
	
	if (data_width == 24) {
		dither = gdither_new ((GDitherType) type, channels, GDither32bit, data_width);
	} else if (data_width == 32) {
		dither = gdither_new (GDitherNone, channels, GDitherFloat, data_width);
	} else {
		throw Exception (*this, "Unsupported data width");
	}
}

template <>
void
SampleFormatConverter<int16_t>::init (nframes_t max_frames, int type, int data_width)
{
	if (data_width != 16) { throw Exception (*this, "Unsupported data width"); }
	init_common (max_frames);
	dither = gdither_new ((GDitherType) type, channels, GDither16bit, data_width);
}

template <>
void
SampleFormatConverter<uint8_t>::init (nframes_t max_frames, int type, int data_width)
{
	if (data_width != 8) { throw Exception (*this, "Unsupported data width"); }
	init_common (max_frames);
	dither = gdither_new ((GDitherType) type, channels, GDither8bit, data_width);
}

template <typename TOut>
void
SampleFormatConverter<TOut>::init_common (nframes_t max_frames )
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
	nframes_t const frames = c_in.frames();
	
	check_frame_count (frames);

	/* Do conversion */

	for (uint32_t chn = 0; chn < channels; ++chn) {
		gdither_runf (dither, chn, frames / channels, data, data_out);
	}

	/* Write forward */

	ProcessContext<TOut> c_out(c_in, data_out);
	output (c_out);
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
	nframes_t frames = c_in.frames();
	float * data = c_in.data();
	
	if (clip_floats) {
		for (nframes_t x = 0; x < frames; ++x) {
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
	nframes_t frames = c_in.frames();
	check_frame_count (frames);
	memcpy (data_out, c_in.data(), frames * sizeof(float));
	
	ProcessContext<float> c (c_in, data_out);
	process (c);
}

template<typename TOut>
void
SampleFormatConverter<TOut>::check_frame_count(nframes_t frames)
{
	if (frames % channels != 0) {
		throw Exception (*this, boost::str (boost::format (
			"Number of frames given to process() was not a multiple of channels: %1% frames with %2% channels")
			% frames % channels));
	}
	
	if (frames  > data_out_size) {
		throw Exception (*this, boost::str (boost::format (
			"Too many frames given to process(), %1% instad of %2%")
			% frames % data_out_size));
	}
}

template class SampleFormatConverter<uint8_t>;
template class SampleFormatConverter<int16_t>;
template class SampleFormatConverter<int32_t>;
template class SampleFormatConverter<float>;

} // namespace
