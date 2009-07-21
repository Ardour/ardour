/*
    Copyright (C) 2006 Paul Davis 

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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <vector>

#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h> // for rename(), sigh
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "pbd/convert.h"
#include "pbd/basename.h"
#include "pbd/mountpoint.h"
#include "pbd/stl_delete.h"
#include "pbd/strsplit.h"
#include "pbd/shortpath.h"
#include "pbd/enumwriter.h"

#include <sndfile.h>

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>
#include <glibmm/thread.h>

#include "ardour/audiofilesource.h"
#include "ardour/sndfile_helpers.h"
#include "ardour/sndfilesource.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/source_factory.h"
#include "ardour/filename_extensions.h"

// if these headers come before sigc++ is included
// the parser throws ObjC++ errors. (nil is a keyword)
#ifdef HAVE_COREAUDIO 
#include "ardour/coreaudiosource.h"
#include <AudioToolbox/ExtendedAudioFile.h>
#include <AudioToolbox/AudioFormat.h>
#endif // HAVE_COREAUDIO

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Glib;

ustring AudioFileSource::peak_dir = "";

sigc::signal<void> AudioFileSource::HeaderPositionOffsetChanged;
uint64_t           AudioFileSource::header_position_offset = 0;

/* XXX maybe this too */
char AudioFileSource::bwf_serial_number[13] = "000000000000";

struct SizedSampleBuffer {
    nframes_t size;
    Sample* buf;

    SizedSampleBuffer (nframes_t sz) : size (sz) { 
	    buf = new Sample[size];
    }

    ~SizedSampleBuffer() {
	    delete [] buf;
    }
};

Glib::StaticPrivate<SizedSampleBuffer> thread_interleave_buffer = GLIBMM_STATIC_PRIVATE_INIT;

/** Constructor used for existing internal-to-session files. */
AudioFileSource::AudioFileSource (Session& s, const ustring& path, bool embedded, Source::Flag flags)
	: Source (s, DataType::AUDIO, path, flags)
	, AudioSource (s, path)
	, FileSource (s, DataType::AUDIO, path, embedded, flags)
{
	if (init (path, true)) {
		throw failed_constructor ();
	}
}

/** Constructor used for new internal-to-session files. */
AudioFileSource::AudioFileSource (Session& s, const ustring& path, bool embedded, Source::Flag flags,
				  SampleFormat /*samp_format*/, HeaderFormat /*hdr_format*/)
	: Source (s, DataType::AUDIO, path, flags)
	, AudioSource (s, path)
	, FileSource (s, DataType::AUDIO, path, embedded, flags)
{
	_is_embedded = false;

	if (init (path, false)) {
		throw failed_constructor ();
	}
}

/** Constructor used for existing internal-to-session files.  File must exist. */
AudioFileSource::AudioFileSource (Session& s, const XMLNode& node, bool must_exist)
	: Source (s, node)
	, AudioSource (s, node)
	, FileSource (s, node, must_exist)
{
	if (set_state (node)) {
		throw failed_constructor ();
	}

	if (init (_name, must_exist)) {
		throw failed_constructor ();
	}
}

AudioFileSource::~AudioFileSource ()
{
	if (removable()) {
		unlink (_path.c_str());
		unlink (peakpath.c_str());
	}
}

int
AudioFileSource::init (const ustring& pathstr, bool must_exist)
{
	_peaks_built = false;
	return FileSource::init (pathstr, must_exist);
}

ustring
AudioFileSource::peak_path (ustring audio_path)
{
	ustring base;

	base = PBD::basename_nosuffix (audio_path);
	base += '%';
	base += (char) ('A' + _channel);

	return _session.peak_path (base);
}

ustring
AudioFileSource::find_broken_peakfile (ustring peak_path, ustring audio_path)
{
	ustring str;

	/* check for the broken location in use by 2.0 for several months */
	
	str = broken_peak_path (audio_path);
	
	if (Glib::file_test (str, Glib::FILE_TEST_EXISTS)) {
		
		if (is_embedded()) {
			
			/* it would be nice to rename it but the nature of 
			   the bug means that we can't reliably use it.
			*/
			
			peak_path = str;
			
		} else {
			/* all native files are mono, so we can just rename
			   it.
			*/
			::rename (str.c_str(), peak_path.c_str());
		}
		
	} else {
		/* Nasty band-aid for older sessions that were created before we
		   used libsndfile for all audio files.
		*/
		
		
		str = old_peak_path (audio_path);	
		if (Glib::file_test (str, Glib::FILE_TEST_EXISTS)) {
			peak_path = str;
		}
	}

	return peak_path;
}

ustring
AudioFileSource::broken_peak_path (ustring audio_path)
{
	return _session.peak_path (audio_path);
}

ustring
AudioFileSource::old_peak_path (ustring audio_path)
{
	/* XXX hardly bombproof! fix me */

	struct stat stat_file;
	struct stat stat_mount;

	ustring mp = mountpoint (audio_path);

	stat (audio_path.c_str(), &stat_file);
	stat (mp.c_str(), &stat_mount);

	char buf[32];
#ifdef __APPLE__
	snprintf (buf, sizeof (buf), "%u-%u-%d.peak", stat_mount.st_ino, stat_file.st_ino, _channel);
#else
	snprintf (buf, sizeof (buf), "%ld-%ld-%d.peak", stat_mount.st_ino, stat_file.st_ino, _channel);
#endif

	ustring res = peak_dir;
	res += buf;
	res += peakfile_suffix;

	return res;
}

bool
AudioFileSource::get_soundfile_info (ustring path, SoundFileInfo& _info, string& error_msg)
{
#ifdef HAVE_COREAUDIO
	if (CoreAudioSource::get_soundfile_info (path, _info, error_msg) == 0) {
		return true;
	}
#endif // HAVE_COREAUDIO

	if (SndFileSource::get_soundfile_info (path, _info, error_msg) != 0) {
		return true;
	}

	return false;
}

XMLNode&
AudioFileSource::get_state ()
{
	XMLNode& root (AudioSource::get_state());
	char buf[32];
	snprintf (buf, sizeof (buf), "%u", _channel);
	root.add_property (X_("channel"), buf);
	return root;
}

int
AudioFileSource::set_state (const XMLNode& node)
{
	if (Source::set_state (node)) {
		return -1;
	}

	if (AudioSource::set_state (node)) {
		return -1;
	}
	
	if (FileSource::set_state (node)) {
		return -1;
	}

	return 0;
}

void
AudioFileSource::mark_streaming_write_completed ()
{
	if (!writable()) {
		return;
	}
	
	/* XXX notice that we're readers of _peaks_built
	   but we must hold a solid lock on PeaksReady.
	*/

	Glib::Mutex::Lock lm (_lock);

	if (_peaks_built) {
		PeaksReady (); /* EMIT SIGNAL */
	}
}

int
AudioFileSource::move_dependents_to_trash()
{
	return ::unlink (peakpath.c_str());
}

void
AudioFileSource::set_header_position_offset (nframes_t offset)
{
	header_position_offset = offset;
	HeaderPositionOffsetChanged ();
}

bool
AudioFileSource::is_empty (Session& /*s*/, ustring path)
{
	SoundFileInfo info;
	string err;
	
	if (!get_soundfile_info (path, info, err)) {
		/* dangerous: we can't get info, so assume that its not empty */
		return false; 
	}

	return info.length == 0;
}

int
AudioFileSource::setup_peakfile ()
{
	if (!(_flags & NoPeakFile)) {
		return initialize_peakfile (_file_is_new, _path);
	} else {
		return 0;
	}
}

bool
AudioFileSource::safe_audio_file_extension(const ustring& file)
{
	const char* suffixes[] = {
		".wav", ".WAV",
		".aiff", ".AIFF",
		".caf", ".CAF",
		".aif", ".AIF",
		".amb", ".AMB",
		".snd", ".SND",
		".au", ".AU",
		".raw", ".RAW",
		".sf", ".SF",
		".cdr", ".CDR",
		".smp", ".SMP",
		".maud", ".MAUD",
		".vwe", ".VWE",
		".paf", ".PAF",
		".voc", ".VOC",
#ifdef HAVE_OGG
		".ogg", ".OGG",
#endif /* HAVE_OGG */
#ifdef HAVE_FLAC
		".flac", ".FLAC",
#else
#endif // HAVE_FLAC
#ifdef HAVE_COREAUDIO
		".mp3", ".MP3",
		".aac", ".AAC",
		".mp4", ".MP4",
#endif // HAVE_COREAUDIO
	};

	for (size_t n = 0; n < sizeof(suffixes)/sizeof(suffixes[0]); ++n) {
		if (file.rfind (suffixes[n]) == file.length() - strlen (suffixes[n])) {
			return true;
		}
	}

	return false;
}

Sample*
AudioFileSource::get_interleave_buffer (nframes_t size)
{
	SizedSampleBuffer* ssb;

	if ((ssb = thread_interleave_buffer.get()) == 0) {
		ssb = new SizedSampleBuffer (size);
		thread_interleave_buffer.set (ssb);
	}

	if (ssb->size < size) {
		ssb = new SizedSampleBuffer (size);
		thread_interleave_buffer.set (ssb);
	}

	return ssb->buf;
}
