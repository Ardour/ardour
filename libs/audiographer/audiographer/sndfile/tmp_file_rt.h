#ifndef AUDIOGRAPHER_TMP_FILE_RT_H
#define AUDIOGRAPHER_TMP_FILE_RT_H

#include <cstdio>
#include <string>

#include <glib.h>
#include "pbd/gstdio_compat.h"
#include "pbd/ringbuffer.h"

#include "audiographer/flag_debuggable.h"
#include "audiographer/sink.h"
#include "sndfile_reader.h"

namespace AudioGrapher
{

/// A temporary file deleted after this class is destructed
template<typename T = DefaultSampleType>
class TmpFileRt
	: public virtual SndfileReader<T>
  , public virtual SndfileBase
  , public Sink<T>
  , public FlagDebuggable<>
{
  public:

	/// \a filename_template must match the requirements for mkstemp, i.e. end in "XXXXXX"
	TmpFileRt (char * filename_template, int format, ChannelCount channels, framecnt_t samplerate)
		: SndfileHandle (g_mkstemp(filename_template), true, SndfileBase::ReadWrite, format, channels, samplerate)
		, filename (filename_template)
  , _rb (samplerate * channels)
	{
		init ();
	}

	using SndfileHandle::operator=;

	~TmpFileRt()
	{
		end_write ();
		/* explicitly close first, some OS (yes I'm looking at you windows)
		 * cannot delete files that are still open
		 */
		if (!filename.empty()) {
			SndfileBase::close();
			std::remove(filename.c_str());
		}
		pthread_mutex_destroy (&_disk_thread_lock);
		pthread_cond_destroy  (&_data_ready);
	}

	framecnt_t get_frames_written() const { return frames_written; }
	void       reset_frames_written_count() { frames_written = 0; }

	/// Writes data to file
	void process (ProcessContext<T> const & c)
	{
		check_flags (*this, c);

		if (SndfileReader<T>::throw_level (ThrowStrict) && c.channels() != channels()) {
			throw Exception (*this, boost::str (boost::format
				("Wrong number of channels given to process(), %1% instead of %2%")
				% c.channels() % channels()));
		}

		if (SndfileReader<T>::throw_level (ThrowProcess) && _rb.write_space() < c.frames()) {
			throw Exception (*this, boost::str (boost::format
				("Could not write data to ringbuffer/output file (%1%)")
				% strError()));
		}

		_rb.write (c.data(), c.frames());

		if (pthread_mutex_trylock (&_disk_thread_lock) == 0) {
			pthread_cond_signal (&_data_ready);
			pthread_mutex_unlock (&_disk_thread_lock);
		}

		if (c.has_flag(ProcessContext<T>::EndOfInput)) {
			end_write (); // XXX not rt-safe -- TODO add API call to flush
			FileWritten (filename);
		}
	}

	using Sink<T>::process;

	PBD::Signal1<void, std::string> FileWritten;

	void disk_thread ()
	{
		const size_t chunksize = 8192; // samples
		T *framebuf = (T*) malloc (chunksize * sizeof (T));

		pthread_mutex_lock (&_disk_thread_lock);

		while (1) {
			if (!_capture) {
				break;
			}
			if (_rb.read_space () >= chunksize) {
				_rb.read (framebuf, chunksize);
				framecnt_t const written = write (framebuf, chunksize);
				assert (written == chunksize);
				frames_written += written;
			}
			pthread_cond_wait (&_data_ready, &_disk_thread_lock);
		}

		// flush ringbuffer
		while (_rb.read_space () > 0) {
			size_t remain = std::min ((size_t)_rb.read_space (), chunksize);
			_rb.read (framebuf, remain);
			framecnt_t const written = write (framebuf, remain);
			frames_written += written;
		}
		writeSync();

		pthread_mutex_unlock (&_disk_thread_lock);
		free (framebuf);
	}

  protected:
	std::string filename;
	framecnt_t frames_written;

	bool _capture;
	RingBuffer<T> _rb;

	pthread_mutex_t _disk_thread_lock;
	pthread_cond_t  _data_ready;
	pthread_t _thread_id;

	static void * _disk_thread (void *arg)
	{
		TmpFileRt *d = static_cast<TmpFileRt *>(arg);
		d->disk_thread ();
		pthread_exit (0);
		return 0;
	}

	void end_write () {
		pthread_mutex_lock (&_disk_thread_lock);
		if (!_capture) {
			pthread_mutex_unlock (&_disk_thread_lock);
			return;
		}
		_capture = false;
		pthread_cond_signal (&_data_ready);
		pthread_mutex_unlock (&_disk_thread_lock);
		pthread_join (_thread_id, NULL);
	}

	void init()
	{
		frames_written = 0;
		_capture = true;
		add_supported_flag (ProcessContext<T>::EndOfInput);
		pthread_mutex_init (&_disk_thread_lock, 0);
		pthread_cond_init  (&_data_ready, 0);

		pthread_create (&_thread_id, NULL, _disk_thread, this);
	}

	private:
	TmpFileRt (TmpFileRt const & other) : SndfileHandle (other) {}
};

} // namespace

#endif // AUDIOGRAPHER_TMP_FILE_RT_H
