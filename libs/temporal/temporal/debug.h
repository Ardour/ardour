/*
    Copyright (C) 2018 Paul Davis

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

#ifndef __temporal_debug_h__
#define __temporal_debug_h__

#include "temporal/visibility.h"
#include "pbd/debug.h"

namespace PBD {
	namespace DEBUG {
		LIBTEMPORAL_API extern DebugBits TemporalDomainConvert;
		LIBTEMPORAL_API extern DebugBits TemporalMap;
		LIBTEMPORAL_API extern DebugBits SnapBBT;
	}
}

#endif /* __ardour_debug_h__ */

