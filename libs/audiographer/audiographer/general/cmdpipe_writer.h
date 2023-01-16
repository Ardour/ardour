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
	CmdPipeWriter (ARDOUR::SystemExec* proc, std::string const& path, int tmp_fd = -1, gchar* tmp_file = 0)
		: samples_written (0)
		, _proc (proc)
		, _path (path)
		, _tmp_fd (tmp_fd)
		, _tmp_file (tmp_file)
	{
		add_supported_flag (ProcessContext<T>::EndOfInput);

		if (tmp_fd >= 0) {
			;
		} else if (proc->start (ARDOUR::SystemExec::ShareWithParent)) {
			throw ARDOUR::ExportFailed ("External encoder (ffmpeg) cannot be started.");
		}
		proc->Terminated.connect_same_thread (exec_connections, boost::bind (&CmdPipeWriter::encode_complete, this));
	}

	virtual ~CmdPipeWriter () {
		delete _proc;
		if (_tmp_fd >= 0) {
			::close (_tmp_fd);
		}
		if (_tmp_file) {
			g_unlink (_tmp_file);
			g_free (_tmp_file);
		}
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

		if (_tmp_fd < 0 && (!_proc || !_proc->is_running())) {
			throw Exception (*this, boost::str (boost::format
						("Target encoder process is not running")));
		}

		const size_t bytes_per_sample = sizeof (T);
		samplecnt_t written;
		if (_tmp_fd >= 0) {
			written = write (_tmp_fd, (const void*) c.data(), c.samples() * bytes_per_sample) / bytes_per_sample;
		} else {
			written = _proc->write_to_stdin ((const void*) c.data(), c.samples() * bytes_per_sample) / bytes_per_sample;
		}

		samples_written += written;

		if (throw_level (ThrowProcess) && written != c.samples()) {
			throw Exception (*this, boost::str (boost::format
						("Could not write data to output file")));
		}

		if (c.has_flag(ProcessContext<T>::EndOfInput)) {
			if (_tmp_fd >= 0) {
				::close (_tmp_fd);
				_tmp_fd = -1;
				if (_proc->start (ARDOUR::SystemExec::ShareWithParent)) {
					throw ARDOUR::ExportFailed ("External encoder (ffmpeg) cannot be started.");
				}
			} else {
				_proc->close_stdin ();
			}
			_proc->wait ();
		}
	}

	using Sink<T>::process;

	PBD::Signal1<void, std::string> FileWritten;

private:
	CmdPipeWriter (CmdPipeWriter const & other) {}

	samplecnt_t samples_written;
	ARDOUR::SystemExec* _proc;
	std::string _path;
	std::vector<char> _tmpfile_path_buf;
	int _tmp_fd;
	gchar* _tmp_file;

	void encode_complete () {
		if (_tmp_file) {
			g_unlink (_tmp_file);
			g_free (_tmp_file);
			_tmp_file = NULL;
		}
		FileWritten (_path);
	}

	PBD::ScopedConnectionList exec_connections;
};

} // namespace

#endif

