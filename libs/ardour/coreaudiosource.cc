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

#include <pbd/error.h>
#include <ardour/coreaudiosource.h>
#include <ardour/utils.h>

#include <appleutility/CAAudioFile.h>
#include <appleutility/CAStreamBasicDescription.h>

#include "i18n.h"

#include <AudioToolbox/AudioFormat.h>

using namespace ARDOUR;
using namespace PBD;

CoreAudioSource::CoreAudioSource (Session& s, const XMLNode& node)
	: AudioFileSource (s, node)
{
	init (_name);
}

CoreAudioSource::CoreAudioSource (Session& s, const string& idstr, Flag flags)
	: AudioFileSource(s, idstr, flags)
{
	init (idstr);
}

void 
CoreAudioSource::init (string idstr)
{
	string::size_type pos;

	tmpbuf = 0;
	tmpbufsize = 0;

	_name = idstr;

	if ((pos = idstr.find_last_of (':')) == string::npos) {
		channel = 0;
		_path = idstr;
	} else {
		channel = atoi (idstr.substr (pos+1).c_str());
		_path = idstr.substr (0, pos);
	}

	cerr << "CoreAudioSource::init() " << name() << endl;
	
	/* note that we temporarily truncated _id at the colon */
	try {
		af.Open(_path.c_str());

		CAStreamBasicDescription file_asbd (af.GetFileDataFormat());
		n_channels = file_asbd.NumberChannels();
		cerr << "number of channels: " << n_channels << endl;
		
		if (channel >= n_channels) {
			error << string_compose("CoreAudioSource: file only contains %1 channels; %2 is invalid as a channel number (%3)", n_channels, channel, name()) << endmsg;
			throw failed_constructor();
		}

		_length = af.GetNumberFrames();

		CAStreamBasicDescription client_asbd(file_asbd);
		client_asbd.SetCanonical(client_asbd.NumberChannels(), false);
		af.SetClientFormat (client_asbd);
	} catch (CAXException& cax) {
		error << string_compose ("CoreAudioSource: %1 (%2)", cax.mOperation, name()) << endmsg;
		throw failed_constructor ();
	}
}

CoreAudioSource::~CoreAudioSource ()
{
	cerr << "CoreAudioSource::~CoreAudioSource() " << name() << endl;
	GoingAway (); /* EMIT SIGNAL */

	if (tmpbuf) {
		delete [] tmpbuf;
	}
	
	cerr << "deletion done" << endl;
}

nframes_t
CoreAudioSource::read_unlocked (Sample *dst, nframes_t start, nframes_t cnt) const
{
	try {
		af.Seek (start);
	} catch (CAXException& cax) {
		error << string_compose("CoreAudioSource: %1 to %2 (%3)", cax.mOperation, start, _name.substr (1)) << endmsg;
		return 0;
	}

	AudioBufferList abl;
	abl.mNumberBuffers = 1;
	abl.mBuffers[0].mNumberChannels = n_channels;

	UInt32 new_cnt = cnt;
	if (n_channels == 1) {
		abl.mBuffers[0].mDataByteSize = cnt * sizeof(Sample);
		abl.mBuffers[0].mData = dst;
		try {
			af.Read (new_cnt, &abl);
		} catch (CAXException& cax) {
			error << string_compose("CoreAudioSource: %1 (%2)", cax.mOperation, _name);
		}
		_read_data_count = new_cnt * sizeof(float);
		return new_cnt;
	}

	UInt32 real_cnt = cnt * n_channels;

	{
		Glib::Mutex::Lock lm (_tmpbuf_lock);
		
		if (tmpbufsize < real_cnt) {
			
			if (tmpbuf) {
				delete [] tmpbuf;
			}
			tmpbufsize = real_cnt;
			tmpbuf = new float[tmpbufsize];
		}

		abl.mBuffers[0].mDataByteSize = tmpbufsize * sizeof(Sample);
		abl.mBuffers[0].mData = tmpbuf;

		cerr << "channel: " << channel << endl;
		
		try {
			af.Read (real_cnt, &abl);
		} catch (CAXException& cax) {
			error << string_compose("CoreAudioSource: %1 (%2)", cax.mOperation, _name);
		}
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

float
CoreAudioSource::sample_rate() const
{
	CAStreamBasicDescription client_asbd;

	try {
		client_asbd = af.GetClientDataFormat ();
	} catch (CAXException& cax) {
		error << string_compose("CoreAudioSource: %1 (%2)", cax.mOperation, _name);
		return 0.0;
	}

	return client_asbd.mSampleRate;
}

int
CoreAudioSource::update_header (nframes_t when, struct tm&, time_t)
{
	return 0;
}

int
CoreAudioSource::get_soundfile_info (string path, SoundFileInfo& _info, string& error_msg)
{
	FSRef ref; 
	ExtAudioFileRef af = 0;
	size_t size;
	CFStringRef name;
	int ret = -1;

	if (FSPathMakeRef ((UInt8*)path.c_str(), &ref, 0) != noErr) {
		goto out;
	}
	
	if (ExtAudioFileOpen(&ref, &af) != noErr) {
		goto out;
	}
	
	AudioStreamBasicDescription absd;
	memset(&absd, 0, sizeof(absd));
	size = sizeof(AudioStreamBasicDescription);
	if (ExtAudioFileGetProperty (af, kExtAudioFileProperty_FileDataFormat, &size, &absd) != noErr) {
		goto out;
	}
	
	_info.samplerate = absd.mSampleRate;
	_info.channels   = absd.mChannelsPerFrame;

	size = sizeof(_info.length);
	if (ExtAudioFileGetProperty(af, kExtAudioFileProperty_FileLengthFrames, &size, &_info.length) != noErr) {
		goto out;
	}
	
	size = sizeof(CFStringRef);
	if (AudioFormatGetProperty(kAudioFormatProperty_FormatName, sizeof(absd), &absd, &size, &name) != noErr) {
		goto out;
	}

	_info.format_name = CFStringRefToStdString(name);

	// XXX it would be nice to find a way to get this information if it exists

	_info.timecode = 0;
	ret = 0;
	
  out:
	ExtAudioFileDispose (af);
	return ret;
	
}
