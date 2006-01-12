/*
    Copyright (C) 2000 Paul Davis 

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

    $Id$
*/

#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>

#include <pbd/mountpoint.h>
#include <ardour/coreaudio_source.h>

#include "i18n.h"

using namespace ARDOUR;

string CoreAudioSource::peak_dir = "";

CoreAudioSource::CoreAudioSource (const XMLNode& node)
	: Source (node)
{
	if (set_state (node)) {
		throw failed_constructor();
	}

	init (_name, true);
	SourceCreated (this); /* EMIT SIGNAL */
}

CoreAudioSource::CoreAudioSource (const string& idstr, bool build_peak)
	: Source(build_peak)
{
	init (idstr, build_peak);

	if (build_peak) {
		 SourceCreated (this); /* EMIT SIGNAL */
	}
}

void 
CoreAudioSource::init (const string& idstr, bool build_peak)
{
	string::size_type pos;
	string file;

	tmpbuf = 0;
	tmpbufsize = 0;
	af_ref = 0;

	_name = idstr;

	if ((pos = idstr.find_last_of (':')) == string::npos) {
		channel = 0;
		file = idstr;
	} else {
		channel = atoi (idstr.substr (pos+1).c_str());
		file = idstr.substr (0, pos);
	}

	/* note that we temporarily truncated _id at the colon */
	FSRef* ref;
	OSStatus err = FSPathMakeRef ((UInt8*)file.c_str(), ref, 0);
	if (err) {
		throw failed_constructor();
	}
	err = ExtAudioFileOpen (ref, af_ref);
	if (err) {
		throw failed_constructor();
	}
	
	if (channel >= n_channels) {
		error << string_compose(_("CoreAudioSource: file only contains %1 channels; %2 is invalid as a channel number"), n_channels, channel) << endmsg;
		ExtAudioFileDispose(*af_ref);
		throw failed_constructor();
	}

	int64_t ca_frames;
	size_t prop_size = sizeof(ca_frames);

	err = ExtAudioFileGetProperty(*af_ref, kExtAudioFileProperty_FileLengthFrames, &prop_size, &ca_frames);
	if (err) {
		throw failed_constructor();
	}
	_length = ca_frames;

	_path = file;

	if (build_peak) {
		if (initialize_peakfile (false, file)) {
			ExtAudioFileDispose(*af_ref);
			throw failed_constructor ();
		}
	}
}

CoreAudioSource::~CoreAudioSource ()

{
	 GoingAway (this); /* EMIT SIGNAL */

	if (af_ref) {
		ExtAudioFileDispose(*af_ref);
	}

	if (tmpbuf) {
		delete [] tmpbuf;
	}
}

jack_nframes_t
CoreAudioSource::read_unlocked (Sample *dst, jack_nframes_t start, jack_nframes_t cnt) const
{
	return read (dst, start, cnt);
}

jack_nframes_t
CoreAudioSource::read (Sample *dst, jack_nframes_t start, jack_nframes_t cnt) const
{
	uint32_t nread;
	float *ptr;
	uint32_t real_cnt;

	OSStatus err = ExtAudioFileSeek(*af_ref, start);
	if (err) {
		error << string_compose(_("CoreAudioSource: could not seek to frame %1 within %2"), start, _name.substr (1)) << endmsg;
		return 0;
	}

	AudioBufferList abl;
	AudioBuffer ab;

	abl.mNumberBuffers = 1;
	abl.mBuffers[0] = ab;
	ab.mNumberChannels = n_channels;
	ab.mDataByteSize = cnt;
	ab.mData = dst;


	if (n_channels == 1) {
		uint32_t ioNumber = cnt;
		err = ExtAudioFileRead(*af_ref, (UInt32*)&ioNumber, &abl);
		_read_data_count = cnt * sizeof(float);
		return ioNumber;
	}

	real_cnt = cnt * n_channels;

	{
		LockMonitor lm (_tmpbuf_lock, __LINE__, __FILE__);
		
		if (tmpbufsize < real_cnt) {
			
			if (tmpbuf) {
				delete [] tmpbuf;
			}
			tmpbufsize = real_cnt;
			tmpbuf = new float[tmpbufsize];
		}
		
		nread = real_cnt;
		err = ExtAudioFileRead(*af_ref, (UInt32*)&nread, tmpbuf);
		ptr = tmpbuf + channel;
		nread /= n_channels;
		
		/* stride through the interleaved data */
		
		for (uint32_t n = 0; n < nread; ++n) {
			dst[n] = *ptr;
			ptr += n_channels;
		}
	}

	_read_data_count = cnt * sizeof(float);
		
	return nread;
}

string
CoreAudioSource::peak_path (string audio_path)
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
CoreAudioSource::old_peak_path (string audio_path)
{
	return peak_path (audio_path);
}
