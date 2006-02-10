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

#ifndef __playlist_snd_file_buffer_h__ 
#define __playlist_snd_file_buffer_h__

#include <sndfile.h>

#include <ardour/source.h>

namespace ARDOUR {

class SndFileSource : public Source {
  public:
	SndFileSource (const string& path_plus_channel, bool build_peak = true);
	SndFileSource (const XMLNode&);
	~SndFileSource ();

	jack_nframes_t length() const { return _info.frames; }
	jack_nframes_t read (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const;
	void           mark_for_remove() {} // we never remove external sndfiles 
	string         peak_path(string audio_path);
	string         old_peak_path(string audio_path);
	string         path() const { return _path; }

	static void set_peak_dir (string dir) { peak_dir = dir; }

  private:
	static string peak_dir;

	SNDFILE *sf;
	SF_INFO _info;
	uint16_t channel;
	mutable float *tmpbuf;
	mutable jack_nframes_t tmpbufsize;
	mutable PBD::Lock _tmpbuf_lock;
	string  _path;

	void init (const string &str, bool build_peak);
	jack_nframes_t read_unlocked (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const;
};

}; /* namespace EDL */

#endif /* __playlist_snd_file_buffer_h__ */

