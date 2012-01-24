#ifndef AUDIOGRAPHER_SNDFILE_WRITER_H
#define AUDIOGRAPHER_SNDFILE_WRITER_H

#include <string>

#include <boost/format.hpp>

#include "audiographer/flag_debuggable.h"
#include "audiographer/sink.h"
#include "audiographer/types.h"
#include "audiographer/sndfile/sndfile_base.h"
#include "audiographer/broadcast_info.h"

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
	SndfileWriter (std::string const & path, int format, ChannelCount channels, framecnt_t samplerate, boost::shared_ptr<BroadcastInfo> broadcast_info)
	  : SndfileHandle (path, Write, format, channels, samplerate)
	  , path (path)
	{
		init();

		if (broadcast_info) {
			broadcast_info->write_to_file (this);
		}
	}
	
	virtual ~SndfileWriter () {}
	
	SndfileWriter (SndfileWriter const & other)
		: SndfileHandle (other)
	{
		init();
	}

	using SndfileHandle::operator=;

	framecnt_t get_frames_written() const { return frames_written; }
	void       reset_frames_written_count() { frames_written = 0; }
	
	/// Writes data to file
	void process (ProcessContext<T> const & c)
	{
		check_flags (*this, c);
		
		if (throw_level (ThrowStrict) && c.channels() != channels()) {
			throw Exception (*this, boost::str (boost::format
				("Wrong number of channels given to process(), %1% instead of %2%")
				% c.channels() % channels()));
		}
		
		framecnt_t const written = write (c.data(), c.frames());
		frames_written += written;

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
		init();
	}

	void init()
	{
		frames_written = 0;
		add_supported_flag (ProcessContext<T>::EndOfInput);
	}

  protected:
	std::string path;
	framecnt_t frames_written;
};

} // namespace

#endif // AUDIOGRAPHER_SNDFILE_WRITER_H
