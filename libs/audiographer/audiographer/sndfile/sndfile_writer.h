#ifndef AUDIOGRAPHER_SNDFILE_WRITER_H
#define AUDIOGRAPHER_SNDFILE_WRITER_H

#include <boost/signals2.hpp>
#include <boost/format.hpp>
#include <string>

#include "audiographer/flag_debuggable.h"
#include "audiographer/sink.h"
#include "audiographer/types.h"
#include "audiographer/sndfile/sndfile_base.h"

#include "pbd/signals.h"

namespace AudioGrapher
{

/** Writer for audio files using libsndfile.
  * Only short, int and float are valid template parameters
  */
template <typename T = DefaultSampleType>
class SndfileWriter
  : public virtual SndfileBase
  , public Sink<T>
  , public Throwing<>
  , public FlagDebuggable<>
{
  public:
	SndfileWriter (std::string const & path, int format, ChannelCount channels, nframes_t samplerate)
	  : SndfileHandle (path, Write, format, channels, samplerate)
	  , path (path)
	{
		add_supported_flag (ProcessContext<T>::EndOfInput);
	}
	
	virtual ~SndfileWriter () {}
	
	SndfileWriter (SndfileWriter const & other) : SndfileHandle (other) {}
	using SndfileHandle::operator=;
	
	/// Writes data to file
	void process (ProcessContext<T> const & c)
	{
		check_flags (*this, c);
		
		if (throw_level (ThrowStrict) && c.channels() != channels()) {
			throw Exception (*this, boost::str (boost::format
				("Wrong number of channels given to process(), %1% instead of %2%")
				% c.channels() % channels()));
		}
		
		nframes_t written = write (c.data(), c.frames());
		if (throw_level (ThrowProcess) && written != c.frames()) {
			throw Exception (*this, boost::str (boost::format
				("Could not write data to output file (%1%)")
				% strError()));
		}

		if (c.has_flag(ProcessContext<T>::EndOfInput)) {
			writeSync();
			FileWritten (path);
		}
	}
	
	using Sink<T>::process;
	
	PBD::Signal1<void, std::string> FileWritten;

  protected:
	/// SndfileHandle has to be constructed directly by deriving classes
	SndfileWriter ()
	{
		add_supported_flag (ProcessContext<T>::EndOfInput);
	}
	
  protected:
	std::string path;
};

} // namespace

#endif // AUDIOGRAPHER_SNDFILE_WRITER_H