#ifndef AUDIOGRAPHER_CMDPIPE_WRITER_H
#define AUDIOGRAPHER_CMDPIPE_WRITER_H

#include <string>

#include <glib.h>
#include <boost/format.hpp>

#include "audiographer/flag_debuggable.h"
#include "audiographer/sink.h"
#include "audiographer/types.h"

#include "pbd/gstdio_compat.h"
#include "pbd/signals.h"

#include "ardour/system_exec.h"
#include "ardour/export_failed.h"

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
	CmdPipeWriter (ARDOUR::SystemExec* proc, std::string const& path, bool pipe1)
		: samples_written (0)
		, _proc (proc)
		, _path (path)
		, encoder_file (0)
	{
		add_supported_flag (ProcessContext<T>::EndOfInput);

		if (pipe1) {
			proc->ReadStdout.connect_same_thread (exec_connections, boost::bind (&CmdPipeWriter::write_ffile, this, _1, _2));
			proc->Terminated.connect_same_thread (exec_connections, boost::bind (&CmdPipeWriter::close_ffile, this));

			encoder_file = g_fopen (path.c_str(), "wb");

			if (!encoder_file) {
				throw ARDOUR::ExportFailed ("Output file cannot be written to.");
			}
		}

		if (proc->start (ARDOUR::SystemExec::IgnoreAndClose)) {
			if (encoder_file) {
				fclose (encoder_file);
				encoder_file = 0;
				g_unlink (path.c_str());
			}
			throw ARDOUR::ExportFailed ("External encoder (ffmpeg) cannot be started.");
		}
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
			if (encoder_file) {
				_proc->close_stdin ();
				_proc->wait ();
				int timeout = 500;
				while (encoder_file && --timeout) {
					Glib::usleep(10000);
				}
			} else {
				_proc->close_stdin ();
				FileWritten (_path);
			}
		}
	}

	using Sink<T>::process;

	PBD::Signal1<void, std::string> FileWritten;

private:
	CmdPipeWriter (CmdPipeWriter const & other) {}

	samplecnt_t samples_written;
	PBD::SystemExec* _proc;
	std::string _path;

	FILE* encoder_file;

	void write_ffile (std::string d, size_t s) {
		fwrite (d.c_str(), sizeof(char), s, encoder_file);
	}

	void close_ffile () {
		fclose (encoder_file);
		encoder_file = 0;
		FileWritten (_path);
	}

	PBD::ScopedConnectionList exec_connections;
};

} // namespace

#endif

