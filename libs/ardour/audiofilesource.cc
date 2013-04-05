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
#include <glibmm/threads.h>

#include "ardour/audiofilesource.h"
#include "ardour/debug.h"
#include "ardour/sndfilesource.h"
#include "ardour/session.h"
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

string AudioFileSource::peak_dir = "";

PBD::Signal0<void> AudioFileSource::HeaderPositionOffsetChanged;
framecnt_t         AudioFileSource::header_position_offset = 0;

/* XXX maybe this too */
char AudioFileSource::bwf_serial_number[13] = "000000000000";

struct SizedSampleBuffer {
	framecnt_t size;
	Sample* buf;

	SizedSampleBuffer (framecnt_t sz) : size (sz) {
		buf = new Sample[size];
	}

	~SizedSampleBuffer() {
		delete [] buf;
	}
};

Glib::Threads::Private<SizedSampleBuffer> thread_interleave_buffer;

/** Constructor used for existing external-to-session files. */
AudioFileSource::AudioFileSource (Session& s, const string& path, Source::Flag flags)
	: Source (s, DataType::AUDIO, path, flags)
	, AudioSource (s, path)
          /* note that external files have their own path as "origin" */
	, FileSource (s, DataType::AUDIO, path, path, flags)
{
	if (init (_path, true)) {
		throw failed_constructor ();
	}
}

/** Constructor used for new internal-to-session files. */
AudioFileSource::AudioFileSource (Session& s, const string& path, const string& origin, Source::Flag flags,
				  SampleFormat /*samp_format*/, HeaderFormat /*hdr_format*/)
	: Source (s, DataType::AUDIO, path, flags)
	, AudioSource (s, path)
	, FileSource (s, DataType::AUDIO, path, origin, flags)
{
        /* note that origin remains empty */

	if (init (_path, false)) {
		throw failed_constructor ();
	}
}

/** Constructor used for existing internal-to-session files via XML.  File must exist. */
AudioFileSource::AudioFileSource (Session& s, const XMLNode& node, bool must_exist)
	: Source (s, node)
	, AudioSource (s, node)
	, FileSource (s, node, must_exist)
{
	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor ();
	}

	if (init (_path, must_exist)) {
		throw failed_constructor ();
	}
}

AudioFileSource::~AudioFileSource ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("AudioFileSource destructor %1, removable? %2\n", _path, removable()));
	if (removable()) {
		unlink (_path.c_str());
		unlink (peakpath.c_str());
	}
}

int
AudioFileSource::init (const string& pathstr, bool must_exist)
{
	return FileSource::init (pathstr, must_exist);
}

string
AudioFileSource::peak_path (string audio_path)
{
	string base;

	base = PBD::basename_nosuffix (audio_path);
	base += '%';
	base += (char) ('A' + _channel);

	return _session.peak_path (base);
}

string
AudioFileSource::find_broken_peakfile (string peak_path, string audio_path)
{
	string str;

	/* check for the broken location in use by 2.0 for several months */

	str = broken_peak_path (audio_path);

	if (Glib::file_test (str, Glib::FILE_TEST_EXISTS)) {

		if (!within_session()) {

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

string
AudioFileSource::broken_peak_path (string audio_path)
{
	return _session.peak_path (basename_nosuffix (audio_path));
}

string
AudioFileSource::old_peak_path (string audio_path)
{
	/* XXX hardly bombproof! fix me */

	struct stat stat_file;
	struct stat stat_mount;

	string mp = mountpoint (audio_path);

	stat (audio_path.c_str(), &stat_file);
	stat (mp.c_str(), &stat_mount);

	char buf[32];
#ifdef __APPLE__
	snprintf (buf, sizeof (buf), "%u-%u-%d.peak", stat_mount.st_ino, stat_file.st_ino, _channel);
#else
	snprintf (buf, sizeof (buf), "%" PRId64 "-%" PRId64 "-%d.peak", (int64_t) stat_mount.st_ino, (int64_t) stat_file.st_ino, _channel);
#endif

	string res = peak_dir;
	res += buf;
	res += peakfile_suffix;

	return res;
}

bool
AudioFileSource::get_soundfile_info (string path, SoundFileInfo& _info, string& error_msg)
{
        /* try sndfile first because it gets timecode info from .wav (BWF) if it exists,
           which at present, ExtAudioFile from Apple seems unable to do.
        */

	if (SndFileSource::get_soundfile_info (path, _info, error_msg) != 0) {
		return true;
	}

#ifdef HAVE_COREAUDIO
	if (CoreAudioSource::get_soundfile_info (path, _info, error_msg) == 0) {
		return true;
	}
#endif // HAVE_COREAUDIO

	return false;
}

XMLNode&
AudioFileSource::get_state ()
{
	XMLNode& root (AudioSource::get_state());
	char buf[32];
	snprintf (buf, sizeof (buf), "%u", _channel);
	root.add_property (X_("channel"), buf);
        root.add_property (X_("origin"), _origin);
	return root;
}

int
AudioFileSource::set_state (const XMLNode& node, int version)
{
	if (Source::set_state (node, version)) {
		return -1;
	}

	if (AudioSource::set_state (node, version)) {
		return -1;
	}

	if (FileSource::set_state (node, version)) {
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

	AudioSource::mark_streaming_write_completed ();
}

int
AudioFileSource::move_dependents_to_trash()
{
	return ::unlink (peakpath.c_str());
}

void
AudioFileSource::set_header_position_offset (framecnt_t offset)
{
	header_position_offset = offset;
	HeaderPositionOffsetChanged ();
}

bool
AudioFileSource::is_empty (Session& /*s*/, string path)
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
		return initialize_peakfile (_path);
	} else {
		return 0;
	}
}

bool
AudioFileSource::safe_audio_file_extension(const string& file)
{
	const char* suffixes[] = {
		".aif", ".AIF",
		".aifc", ".AIFC",
		".aiff", ".AIFF",
		".amb", ".AMB",
		".au", ".AU",
		".caf", ".CAF",
		".cdr", ".CDR",
		".flac", ".FLAC",
		".htk", ".HTK",
		".iff", ".IFF",
		".mat", ".MAT",
		".oga", ".OGA",
		".ogg", ".OGG",
		".paf", ".PAF",
		".pvf", ".PVF",
		".sf", ".SF",
		".smp", ".SMP",
		".snd", ".SND",
		".maud", ".MAUD",
		".voc", ".VOC"
		".vwe", ".VWE",
		".w64", ".W64",
		".wav", ".WAV",
#ifdef HAVE_COREAUDIO
		".aac", ".AAC",
		".adts", ".ADTS",
		".ac3", ".AC3",
		".amr", ".AMR",
		".mpa", ".MPA",
		".mpeg", ".MPEG",
		".mp1", ".MP1",
		".mp2", ".MP2",
		".mp3", ".MP3",
		".mp4", ".MP4",
		".m4a", ".M4A",
		".sd2", ".SD2", 	// libsndfile supports sd2 also, but the resource fork is required to open.
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
AudioFileSource::get_interleave_buffer (framecnt_t size)
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
