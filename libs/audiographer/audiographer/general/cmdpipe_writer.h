#ifndef AUDIOGRAPHER_CMDPIPE_WRITER_H
#define AUDIOGRAPHER_CMDPIPE_WRITER_H

#include <string>

#include <boost/format.hpp>

#include "audiographer/flag_debuggable.h"
#include "audiographer/sink.h"
#include "audiographer/types.h"

#include "pbd/signals.h"
#include "pbd/system_exec.h"

namespace AudioGrapher
{

/** Writer for audio files using libsndfile.
  * Only short, int and float are valid template parameters
  */
template <typename T = DefaultSampleType>
class CmdPipeWriter
  : public Sink<T>
  , public Throwing<>
  , public FlagDebuggable<>
{
public:
	CmdPipeWriter (PBD::SystemExec* proc, std::string const& path)
		: samples_written (0)
		, _proc (proc)
		, _path (path)
	{
		add_supported_flag (ProcessContext<T>::EndOfInput);
	}

	virtual ~CmdPipeWriter () {
		delete _proc;
	}

	samplecnt_t get_samples_written() const { return samples_written; }
	void       reset_samples_written_count() { samples_written = 0; }

	void close (void)
	{
		_proc->terminate ();
	}

	virtual void process (ProcessContext<T> const & c)
	{
		check_flags (*this, c);

		if (!_proc || !_proc->is_running()) {
			throw Exception (*this, boost::str (boost::format
						("Target encoder process is not running")));
		}

		const size_t bytes_per_sample = sizeof (T);
		samplecnt_t const written = _proc->write_to_stdin ((const void*) c.data(), c.samples() * bytes_per_sample) / bytes_per_sample;
		samples_written += written;

		if (throw_level (ThrowProcess) && written != c.samples()) {
			throw Exception (*this, boost::str (boost::format
						("Could not write data to output file")));
		}

		if (c.has_flag(ProcessContext<T>::EndOfInput)) {
			_proc->close_stdin ();
			FileWritten (_path);
		}
	}

	using Sink<T>::process;

	PBD::Signal1<void, std::string> FileWritten;

private:
	CmdPipeWriter (CmdPipeWriter const & other) {}

	samplecnt_t samples_written;
	PBD::SystemExec* _proc;
	std::string _path;
};

} // namespace

#endif

