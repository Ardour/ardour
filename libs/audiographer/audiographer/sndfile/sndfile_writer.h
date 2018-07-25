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
	SndfileWriter (std::string const & path, int format, ChannelCount channels, samplecnt_t samplerate, boost::shared_ptr<BroadcastInfo> broadcast_info)
	  : SndfileHandle (path, Write, format, channels, samplerate)
	  , path (path)
	{
		init();

		if (broadcast_info) {
			broadcast_info->write_to_file (this);
		}
	}

	virtual ~SndfileWriter () {}

	using SndfileHandle::operator=;

	samplecnt_t get_samples_written() const { return samples_written; }
	void       reset_samples_written_count() { samples_written = 0; }

	/// Writes data to file
	virtual void process (ProcessContext<T> const & c)
	{
		check_flags (*this, c);

		if (throw_level (ThrowStrict) && c.channels() != channels()) {
			throw Exception (*this, boost::str (boost::format
				("Wrong number of channels given to process(), %1% instead of %2%")
				% c.channels() % channels()));
		}

		samplecnt_t const written = write (c.data(), c.samples());
		samples_written += written;

		if (throw_level (ThrowProcess) && written != c.samples()) {
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

	virtual void init()
	{
		if (SF_ERR_NO_ERROR != SndfileHandle::error ()) {
			throw Exception (*this, boost::str (boost::format
						("Could create output file (%1%)") % path));
		}
		samples_written = 0;
		add_supported_flag (ProcessContext<T>::EndOfInput);
	}

  protected:
	std::string path;
	samplecnt_t samples_written;

	private:
	SndfileWriter (SndfileWriter const & other) {}
};

} // namespace

#endif // AUDIOGRAPHER_SNDFILE_WRITER_H
