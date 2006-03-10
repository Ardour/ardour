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

#include <ardour/coreaudio_source.h>

#include "i18n.h"

using namespace ARDOUR;

CoreAudioSource::CoreAudioSource (const XMLNode& node)
	: ExternalSource (node)
{
	init (_name, true);
	SourceCreated (this); /* EMIT SIGNAL */
}

CoreAudioSource::CoreAudioSource (const string& idstr, bool build_peak)
	: ExternalSource(idstr, build_peak)
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
	af = 0;
	OSStatus err = noErr;

	_name = idstr;

	if ((pos = idstr.find_last_of (':')) == string::npos) {
		channel = 0;
		file = idstr;
	} else {
		channel = atoi (idstr.substr (pos+1).c_str());
		file = idstr.substr (0, pos);
	}

	/* note that we temporarily truncated _id at the colon */
	FSRef ref;
	err = FSPathMakeRef ((UInt8*)file.c_str(), &ref, 0);
	if (err != noErr) {
		throw failed_constructor();
	}

	err = ExtAudioFileOpen (&ref, &af);
	if (err != noErr) {
		ExtAudioFileDispose (af);
		throw failed_constructor();
	}

	AudioStreamBasicDescription absd;
	memset(&absd, 0, sizeof(absd));
	size_t absd_size = sizeof(absd);
	err = ExtAudioFileGetProperty(af,
			kExtAudioFileProperty_FileDataFormat, &absd_size, &absd);
	if (err != noErr) {
		ExtAudioFileDispose (af);
		throw failed_constructor();
	}
	n_channels = absd.mChannelsPerFrame;

	if (channel >= n_channels) {
		error << string_compose(_("CoreAudioSource: file only contains %1 channels; %2 is invalid as a channel number"), n_channels, channel) << endmsg;
		ExtAudioFileDispose (af);
		throw failed_constructor();
	}

	int64_t ca_frames;
	size_t prop_size = sizeof(ca_frames);

	err = ExtAudioFileGetProperty(af, kExtAudioFileProperty_FileLengthFrames, &prop_size, &ca_frames);
	if (err != noErr) {
		ExtAudioFileDispose (af);
		throw failed_constructor();
	}
	_length = ca_frames;

	_path = file;

	if (build_peak) {
		if (initialize_peakfile (false, file)) {
			ExtAudioFileDispose (af);
			throw failed_constructor ();
		}
	}
}

CoreAudioSource::~CoreAudioSource ()
{
	GoingAway (this); /* EMIT SIGNAL */

	if (af) {
		ExtAudioFileDispose (af);
	}

	if (tmpbuf) {
		delete [] tmpbuf;
	}
}

jack_nframes_t
CoreAudioSource::read (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const
{
	OSStatus err = noErr;

	err = ExtAudioFileSeek(af, start);
	if (err != noErr) {
		error << string_compose(_("CoreAudioSource: could not seek to frame %1 within %2 (%3)"), start, _name.substr (1), err) << endmsg;
		return 0;
	}

	AudioBuffer ab;
	ab.mNumberChannels = n_channels;
	ab.mDataByteSize = cnt;
	ab.mData = dst;

	AudioBufferList abl;
	abl.mNumberBuffers = 1;
	abl.mBuffers[1] = ab;

	if (n_channels == 1) {
		err = ExtAudioFileRead(af, (UInt32*) &cnt, &abl);
		_read_data_count = cnt * sizeof(float);
		return cnt;
	}

	uint32_t real_cnt = cnt * n_channels;

	{
		LockMonitor lm (_tmpbuf_lock, __LINE__, __FILE__);
		
		if (tmpbufsize < real_cnt) {
			
			if (tmpbuf) {
				delete [] tmpbuf;
			}
			tmpbufsize = real_cnt;
			tmpbuf = new float[tmpbufsize];
		}

		ab.mDataByteSize = real_cnt;
		ab.mData = tmpbuf;
		
		err = ExtAudioFileRead(af, (UInt32*) &real_cnt, &abl);
		float *ptr = tmpbuf + channel;
		real_cnt /= n_channels;
		
		/* stride through the interleaved data */
		
		for (uint32_t n = 0; n < real_cnt; ++n) {
			dst[n] = *ptr;
			ptr += n_channels;
		}
	}

	_read_data_count = cnt * sizeof(float);
		
	return real_cnt;
}

