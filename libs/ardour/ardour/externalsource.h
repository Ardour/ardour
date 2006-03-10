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

#ifndef __external_source_h__ 
#define __external_source_h__

#include <ardour/source.h>

namespace ARDOUR {

struct SoundFileInfo {
    float    samplerate;
    uint16_t channels;
    int64_t length;
    std::string format_name;
};

class ExternalSource : public Source {
  public:
	virtual ~ExternalSource ();

	string path() const { return _path; }

	virtual jack_nframes_t read (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const = 0;

	void mark_for_remove() {} // we never remove external sndfiles 
	string peak_path(string audio_path);
	string old_peak_path(string audio_path);

	static void set_peak_dir (string dir) { peak_dir = dir; }

	static ExternalSource* create (const string& path_plus_channel, bool build_peak = true);
	static ExternalSource* create (const XMLNode& node);
	static bool get_soundfile_info (string path, SoundFileInfo& _info, string& error);

  protected:
	ExternalSource (const string& path_plus_channel, bool build_peak = true);
	ExternalSource (const XMLNode&);

	static string peak_dir;

	uint16_t channel;
	string  _path;

	jack_nframes_t read_unlocked (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const;
};

}; /* namespace ARDOUR */

#endif /* __external_source_h__ */

