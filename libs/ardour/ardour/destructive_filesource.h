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

    $Id$
*/

#ifndef __ardour_destructive_file_source_h__ 
#define __ardour_destructive_file_source_h__

#include <string>

#include <ardour/filesource.h>

struct tm;

namespace ARDOUR {

class DestructiveFileSource : public FileSource {
  public:
	DestructiveFileSource (std::string path, jack_nframes_t rate, bool repair_first = false);
	DestructiveFileSource (const XMLNode&, jack_nframes_t rate);
	~DestructiveFileSource ();

	int  seek (jack_nframes_t frame);
	void mark_capture_start ();
	void mark_capture_end ();
	void clear_capture_marks();

	jack_nframes_t write (Sample *src, jack_nframes_t cnt);

  private:
	static jack_nframes_t xfade_frames;
	static gain_t* out_coefficient;
	static gain_t* in_coefficient;
	static void setup_standard_crossfades (jack_nframes_t sample_rate);

	bool          _capture_start;
	bool          _capture_end;
	jack_nframes_t file_pos;
	Sample*        xfade_buf;

	jack_nframes_t crossfade (Sample* data, jack_nframes_t cnt, int dir);

};

}

#endif /* __ardour_destructive_file_source_h__ */
