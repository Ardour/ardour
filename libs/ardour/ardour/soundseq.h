/*
    Copyright (C) 2001 Paul Davis

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

#ifndef __soundseq_h__
#define __soundseq_h__

#include "edl.h"

namespace ARDOUR {

typedef gint16 peak_datum;

struct LIBARDOUR_API peak_data_t {
    peak_datum min;
    peak_datum max;
};

const uint32_t frames_per_peak = 2048;

class LIBARDOUR_API Sound : public EDL::Piece {
  public:
	int peak (peak_data_t& pk, uint32_t start, uint32_t cnt);
	int read_peaks (peak_data_t *, uint32_t npeaks, uint32_t start, uint32_t cnt);
	int build_peak (uint32_t first_frame, uint32_t cnt);
};

class LIBARDOUR_API SoundPlaylist : public EDL::Playlist {
  public:
	int read_peaks (peak_data_t *, uint32_t npeaks, uint32_t start, uint32_t cnt);
};

} /* namespace ARDOUR */

#endif	/* __soundseq_h__ */



