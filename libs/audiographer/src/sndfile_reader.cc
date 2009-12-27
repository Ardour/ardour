#include "audiographer/sndfile_reader.h"

#include <boost/format.hpp>

#include "audiographer/exception.h"

namespace AudioGrapher
{

template<typename T>
SndfileReader<T>::SndfileReader (ChannelCount channels, nframes_t samplerate, int format, std::string path)
  : SndfileBase (channels, samplerate, format, path)
{
	init ();
}

template<typename T>
nframes_t
SndfileReader<T>::seek (nframes_t frames, SeekType whence)
{
	return sf_seek (sndfile, frames, whence);
}

template<typename T>
nframes_t
SndfileReader<T>::read (ProcessContext<T> & context)
{
	if (context.channels() != sf_info.channels) {
		throw Exception (*this, boost::str (boost::format (
			"ProcessContext given to read() has a wrong amount of channels: %1% instead of %2%")
			% context.channels() % sf_info.channels));
	}
	
	nframes_t frames_read = (*read_func) (sndfile, context.data(), context.frames());
	if (frames_read < context.frames()) {
		context.frames() = frames_read;
		context.set_flag (ProcessContext<T>::EndOfInput);
	}
	output (context);
	return frames_read;
}

template<>
void
SndfileReader<short>::init()
{
	read_func = &sf_read_short;
}

template<>
void
SndfileReader<int>::init()
{
	read_func = &sf_read_int;
}

template<>
void
SndfileReader<float>::init()
{
	read_func = &sf_read_float;
}

template class SndfileReader<short>;
template class SndfileReader<int>;
template class SndfileReader<float>;

} // namespace