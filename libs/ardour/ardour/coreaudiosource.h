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

#include <appleutility/CAAudioFile.h>

#include <ardour/audiofilesource.h>

namespace ARDOUR {

class CoreAudioSource : public AudioFileSource {
  public:
	CoreAudioSource (ARDOUR::Session&, const XMLNode&);
	CoreAudioSource (ARDOUR::Session&, const string& path_plus_channel, Flag);
	~CoreAudioSource ();

	float sample_rate() const;
	int update_header (nframes_t when, struct tm&, time_t);

	int flush_header () {return 0;};
	void set_header_timeline_position () {};

  protected:
	nframes_t read_unlocked (Sample *dst, nframes_t start, nframes_t cnt) const;
	nframes_t write_unlocked (Sample *dst, nframes_t cnt) { return 0; }

  private:
	mutable CAAudioFile af;
	uint16_t n_channels;

	mutable float *tmpbuf;
	mutable nframes_t tmpbufsize;
	mutable Glib::Mutex _tmpbuf_lock;

	void init (string str);
};

}; /* namespace ARDOUR */

#endif /* __coreaudio_source_h__ */

