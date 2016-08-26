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

#include "pbd/gstdio_compat.h"
#include "pbd/convert.h"
#include "pbd/basename.h"
#include "pbd/file_utils.h"
#include "pbd/mountpoint.h"
#include "pbd/stl_delete.h"
#include "pbd/strsplit.h"
#include "pbd/shortpath.h"
#include "pbd/stacktrace.h"
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

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Glib;

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

/** Constructor used for existing internal-to-session files during crash
 * recovery. File must exist
 */
AudioFileSource::AudioFileSource (Session& s, const string& path, Source::Flag flags, bool /* ignored-exists-for-prototype differentiation */)
	: Source (s, DataType::AUDIO, path, flags)
	, AudioSource (s, path)
	, FileSource (s, DataType::AUDIO, path, string(), flags)
{
        /* note that origin remains empty */

	if (init (_path, true)) {
		throw failed_constructor ();
	}
}


/** Constructor used for sources listed in session-files (XML)
 * and missing sources (SilentFileSource).
 *
 * If _origin is an absolute path after ::set_state(), then the
 * file is external to the session.
 */
AudioFileSource::AudioFileSource (Session& s, const XMLNode& node, bool must_exist)
	: Source (s, node)
	, AudioSource (s, node)
	, FileSource (s, node, must_exist)
{
	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor ();
	}

	if (Glib::path_is_absolute (_origin)) {
		_path = _origin;
	}

	if (init (_path, must_exist)) {
		throw failed_constructor ();
	}
}

AudioFileSource::~AudioFileSource ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("AudioFileSource destructor %1, removable? %2\n", _path, removable()));
	if (removable()) {
		::g_unlink (_path.c_str());
		::g_unlink (_peakpath.c_str());
	}
}

int
AudioFileSource::init (const string& pathstr, bool must_exist)
{
	return FileSource::init (pathstr, must_exist);
}

string
AudioFileSource::construct_peak_filepath (const string& audio_path, const bool in_session, const bool old_peak_name) const
{
	string base;
	if (old_peak_name) {
		base = audio_path.substr (0, audio_path.find_last_of ('.'));
	} else {
		base = audio_path;
	}
	base += '%';
	base += (char) ('A' + _channel);
	return _session.construct_peak_filepath (base, in_session, old_peak_name);
}

bool
AudioFileSource::get_soundfile_info (const string& path, SoundFileInfo& _info, string& error_msg)
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
	LocaleGuard lg;
	XMLNode& root (AudioSource::get_state());
	root.set_property (X_("channel"), _channel);
	root.set_property (X_("origin"), _origin);
	root.set_property (X_("gain"), _gain);
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
AudioFileSource::mark_streaming_write_completed (const Lock& lock)
{
	if (!writable()) {
		return;
	}

	AudioSource::mark_streaming_write_completed (lock);
}

int
AudioFileSource::move_dependents_to_trash()
{
	return ::g_unlink (_peakpath.c_str());
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
	if (_session.deletion_in_progress()) {
		return 0;
	}
	if (!(_flags & NoPeakFile)) {
		return initialize_peakfile (_path, within_session());
	} else {
		return 0;
	}
}

void
AudioFileSource::set_gain (float g, bool temporarily)
{
	if (_gain == g) {
		return;
	}
	_gain = g;
	if (temporarily) {
		return;
	}
	close_peakfile();
	setup_peakfile ();
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

