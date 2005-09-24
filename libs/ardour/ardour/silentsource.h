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

#ifndef __playlist_const_buffer_h__ 
#define __playlist_const_buffer_h__

#include <string>
#include <cstdlib>

#include "source.h"

namespace ARDOUR {

class SilentSource : public Source {
  public:
	SilentSource () {
		_name = "Silent Source";
	}
	
	static bool is_silent_source (const string& name) {
		return name == "Silent Source";
	}
	
	jack_nframes_t length() { return ~0U; }

	jack_nframes_t read (Source::Data *dst, jack_nframes_t start, jack_nframes_t cnt) {
		jack_nframes_t n = cnt;
		while (n--) *dst++ = 0;
		return cnt;
	}

	void peak (guint8 *max, guint8 *min, jack_nframes_t start, jack_nframes_t cnt) {
		*max = *min = 0;
	}
};

}

#endif /* __playlist_const_buffer_h__ */
