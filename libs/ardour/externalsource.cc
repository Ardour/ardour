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

#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>

#include <sndfile.h>

#include <pbd/mountpoint.h>
#include <ardour/externalsource.h>
#include <ardour/sndfilesource.h>
#include <ardour/sndfile_helpers.h>

// if these headers come before sigc++ is included
// the parser throws ObjC++ errors. (nil is a keyword)
#ifdef HAVE_COREAUDIO 
#include <ardour/coreaudio_source.h>
#include <AudioToolbox/ExtendedAudioFile.h>
#include <AudioToolbox/AudioFormat.h>
#endif // HAVE_COREAUDIO

#include "i18n.h"

using namespace ARDOUR;

string ExternalSource::peak_dir = "";

ExternalSource::ExternalSource (const XMLNode& node)
	: Source (node)
{
}

ExternalSource::ExternalSource (const string& idstr, bool build_peak)
	: Source(build_peak)
{
}

ExternalSource::~ExternalSource ()
{
}

jack_nframes_t
ExternalSource::read_unlocked (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const
{
	return read (dst, start, cnt, workbuf);
}

string
ExternalSource::peak_path (string audio_path)
{
	/* XXX hardly bombproof! fix me */

	struct stat stat_file;
	struct stat stat_mount;

	string mp = mountpoint (audio_path);

	stat (audio_path.c_str(), &stat_file);
	stat (mp.c_str(), &stat_mount);

	char buf[32];
	snprintf (buf, sizeof (buf), "%ld-%ld-%d.peak", stat_mount.st_ino, stat_file.st_ino, channel);

	string res = peak_dir;
	res += buf;

	return res;
}

string
ExternalSource::old_peak_path (string audio_path)
{
	return peak_path (audio_path);
}

ExternalSource*
ExternalSource::create (const XMLNode& node)
{
	return new SndFileSource (node);
}

ExternalSource*
ExternalSource::create (const string& idstr, bool build_peak)
{
	return new SndFileSource (idstr, build_peak);
}

#ifdef HAVE_COREAUDIO
std::string 
CFStringRefToStdString(CFStringRef stringRef)
{
	CFIndex size = 
		CFStringGetMaximumSizeForEncoding(CFStringGetLength(stringRef) , 
		kCFStringEncodingASCII);
	    char *buf = new char[size];
	
	std::string result;

	if(CFStringGetCString(stringRef, buf, size, kCFStringEncodingASCII)) {
	    result = buf;
	}
	delete [] buf;
	return result;
}
#endif // HAVE_COREAUDIO

bool
ExternalSource::get_soundfile_info (string path, SoundFileInfo& _info, string& error_msg)
{
#ifdef HAVE_COREAUDIO
	OSStatus err = noErr;
    FSRef ref; 
	ExtAudioFileRef af = 0;
	size_t size;
    CFStringRef name;

    err = FSPathMakeRef ((UInt8*)path.c_str(), &ref, 0);
	if (err != noErr) {
        ExtAudioFileDispose (af);
		goto libsndfile;
	}

	err = ExtAudioFileOpen(&ref, &af);
	if (err != noErr) {
        ExtAudioFileDispose (af);
		goto libsndfile;
	}

	AudioStreamBasicDescription absd;
	memset(&absd, 0, sizeof(absd));
	size = sizeof(AudioStreamBasicDescription);
	err = ExtAudioFileGetProperty(af,
			kExtAudioFileProperty_FileDataFormat, &size, &absd);
	if (err != noErr) {
        ExtAudioFileDispose (af);
		goto libsndfile;
	}

	_info.samplerate = absd.mSampleRate;
	_info.channels   = absd.mChannelsPerFrame;

    size = sizeof(_info.length);
    err = ExtAudioFileGetProperty(af, kExtAudioFileProperty_FileLengthFrames, &size, &_info.length);
    if (err != noErr) {
        ExtAudioFileDispose (af);
		goto libsndfile;
    }

	size = sizeof(CFStringRef);
	err = AudioFormatGetProperty(
			kAudioFormatProperty_FormatName, sizeof(absd), &absd, &size, &name);
	if (err != noErr) {
        ExtAudioFileDispose (af);
		goto libsndfile;
	}

	_info.format_name = CFStringRefToStdString(name);

    ExtAudioFileDispose (af);
	return true;
	
libsndfile:
#endif // HAVE_COREAUDIO

	SNDFILE *sf;
	SF_INFO sf_info;

	sf_info.format = 0; // libsndfile says to clear this before sf_open().

	if ((sf = sf_open ((char*) path.c_str(), SFM_READ, &sf_info)) == 0) { 
		char errbuf[256];
		error_msg = sf_error_str (0, errbuf, sizeof (errbuf) - 1);
		return false;
	}

	sf_close (sf);

	_info.samplerate  = sf_info.samplerate;
	_info.channels    = sf_info.channels;
	_info.length      = sf_info.frames;
	_info.format_name = string_compose("Format: %1, %2",
			sndfile_major_format(sf_info.format),
			sndfile_minor_format(sf_info.format));

	return true;
}
