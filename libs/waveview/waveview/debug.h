/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#ifndef _WAVEVIEW_DEBUG_H_
#define _WAVEVIEW_DEBUG_H_

#include <sys/time.h>
#include <map>
#include "pbd/debug.h"

#include "waveview/visibility.h"

namespace PBD {
	namespace DEBUG {
		LIBWAVEVIEW_API extern DebugBits WaveView;
	}
}

#endif
