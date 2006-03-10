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

*/

#ifndef __coreaudio_source_h__ 
#define __coreaudio_source_h__

#include <ardour/externalsource.h>
#include <AudioToolbox/ExtendedAudioFile.h>

namespace ARDOUR {

class CoreAudioSource : public ExternalSource {
  public:
	CoreAudioSource (const string& path_plus_channel, bool build_peak = true);
	CoreAudioSource (const XMLNode&);
	~CoreAudioSource ();

	jack_nframes_t read (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const;

  private:
	ExtAudioFileRef af;
	uint16_t n_channels;

	mutable float *tmpbuf;
	mutable jack_nframes_t tmpbufsize;
	mutable PBD::Lock _tmpbuf_lock;

	void init (const string &str, bool build_peak);
};

}; /* namespace ARDOUR */

#endif /* __coreaudio_source_h__ */

