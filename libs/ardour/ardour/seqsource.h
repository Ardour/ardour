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

#ifndef __playlist_seqsource_h__ 
#define __playlist_seqsource_h__

#include <string>

#include "edl.h"

namespace EDL {

class PlaylistSource : public Source {
  public:
	PlaylistSource (Playlist&);
	~PlaylistSource ();

	const gchar * const id() { return playlist.name().c_str(); }
	uint32_t length() { return playlist.length(); }
	uint32_t read (Source::Data *dst, uint32_t start, uint32_t cnt) {
		return playlist.read (dst, start, cnt, false);
	}
	uint32_t write (Source::Data *src, uint32_t where, uint32_t cnt) {
		return playlist.write (src, where, cnt);
	}

//	int read_peaks (peak_data_t *, uint32_t npeaks, uint32_t start, uint32_t cnt);
//	int build_peak (uint32_t first_frame, uint32_t cnt);

  protected:

  private:
	Playlist& playlist;
};

}; /* namespace EDL */

#endif /* __playlist_seqsource_h__ */
