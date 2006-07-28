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

#include "i18n.h"

#include <AudioToolbox/AudioFormat.h>

using namespace ARDOUR;
using namespace PBD;

CoreAudioSource::CoreAudioSource (const XMLNode& node)
	: AudioFileSource (node)
{
	init (_name);
	
	AudioSourceCreated (this); /* EMIT SIGNAL */
}

CoreAudioSource::CoreAudioSource (const string& idstr, Flag flags)
	: AudioFileSource(idstr, flags)
{
	init (idstr);

	AudioSourceCreated (this); /* EMIT SIGNAL */
}

void 
CoreAudioSource::init (const string& idstr)
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
	FSRef fsr;
	err = FSPathMakeRef ((UInt8*)file.c_str(), &fsr, 0);
	if (err != noErr) {
		error << string_compose (_("Could not make reference to file: %1"), name()) << endmsg;
		throw failed_constructor();
	}

	err = ExtAudioFileOpen (&fsr, &af);
	if (err != noErr) {
		error << string_compose (_("Could not open file: %1"), name()) << endmsg;
		ExtAudioFileDispose (af);
		throw failed_constructor();
	}

	AudioStreamBasicDescription file_asbd;
	memset(&file_asbd, 0, sizeof(AudioStreamBasicDescription));
	size_t asbd_size = sizeof(AudioStreamBasicDescription);
	err = ExtAudioFileGetProperty(af,
			kExtAudioFileProperty_FileDataFormat, &asbd_size, &file_asbd);
	if (err != noErr) {
		error << string_compose (_("Could not get file data format for file: %1"), name()) << endmsg;
		ExtAudioFileDispose (af);
		throw failed_constructor();
	}
	n_channels = file_asbd.mChannelsPerFrame;

	cerr << "number of channels: " << n_channels << endl;
	
	if (channel >= n_channels) {
		error << string_compose(_("CoreAudioSource: file only contains %1 channels; %2 is invalid as a channel number"), n_channels, channel) << endmsg;
		ExtAudioFileDispose (af);
		throw failed_constructor();
	}

	int64_t ca_frames;
	size_t prop_size = sizeof(int64_t);

	err = ExtAudioFileGetProperty(af, kExtAudioFileProperty_FileLengthFrames, &prop_size, &ca_frames);
	if (err != noErr) {
		error << string_compose (_("Could not get file length for file: %1"), name()) << endmsg;
		ExtAudioFileDispose (af);
		throw failed_constructor();
	}

	_length = ca_frames;
	_path = file;

	AudioStreamBasicDescription client_asbd;
	memset(&client_asbd, 0, sizeof(AudioStreamBasicDescription));
	client_asbd.mSampleRate = file_asbd.mSampleRate;
	client_asbd.mFormatID = kAudioFormatLinearPCM;
	client_asbd.mFormatFlags = kLinearPCMFormatFlagIsFloat;
	client_asbd.mBytesPerPacket = file_asbd.mChannelsPerFrame * 4;
	client_asbd.mFramesPerPacket = 1;
	client_asbd.mBytesPerFrame = client_asbd.mBytesPerPacket;
	client_asbd.mChannelsPerFrame = file_asbd.mChannelsPerFrame;
	client_asbd.mBitsPerChannel = 32;

	err = ExtAudioFileSetProperty (af, kExtAudioFileProperty_ClientDataFormat, asbd_size, &client_asbd);
	if (err != noErr) {
		error << string_compose (_("Could not set client data format for file: %1"), name()) << endmsg;
		ExtAudioFileDispose (af);
		throw failed_constructor ();
	}
	
	if (_build_peakfiles) {
		if (initialize_peakfile (false, file)) {
			error << string_compose(_("initialize peakfile failed for file %1"), name()) << endmsg;
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
CoreAudioSource::read_unlocked (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const
{
	OSStatus err = noErr;

	err = ExtAudioFileSeek(af, start);
	if (err != noErr) {
		error << string_compose(_("CoreAudioSource: could not seek to frame %1 within %2 (%3)"), start, _name.substr (1), err) << endmsg;
		return 0;
	}

	AudioBufferList abl;
	abl.mNumberBuffers = 1;
	abl.mBuffers[0].mNumberChannels = n_channels;
	abl.mBuffers[0].mDataByteSize = cnt * sizeof(Sample);
	abl.mBuffers[0].mData = dst;

	if (n_channels == 1) {
		err = ExtAudioFileRead(af, (UInt32*) &cnt, &abl);
		_read_data_count = cnt * sizeof(float);
		return cnt;
	}

	uint32_t real_cnt = cnt * n_channels;

	{
		Glib::Mutex::Lock lm (_tmpbuf_lock);
		
		if (tmpbufsize < real_cnt) {
			
			if (tmpbuf) {
				delete [] tmpbuf;
			}
			tmpbufsize = real_cnt;
			tmpbuf = new float[tmpbufsize];
		}

		abl.mBuffers[0].mDataByteSize = real_cnt * sizeof(Sample);
		abl.mBuffers[0].mData = tmpbuf;
		
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

float
CoreAudioSource::sample_rate() const
{
        AudioStreamBasicDescription client_asbd;
        memset(&client_asbd, 0, sizeof(AudioStreamBasicDescription));

	OSStatus err = noErr;
	size_t asbd_size = sizeof(AudioStreamBasicDescription);

        err = ExtAudioFileSetProperty (af, kExtAudioFileProperty_ClientDataFormat, asbd_size, &client_asbd);
        if (err != noErr) {
		error << string_compose(_("Could not detect samplerate for: %1"), name()) << endmsg;
		return 0.0;
	}

	return client_asbd.mSampleRate;
}

int
CoreAudioSource::update_header (jack_nframes_t when, struct tm&, time_t)
{
	return 0;
}

