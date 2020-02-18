/*
 * Copyright (C) 2011-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __gtk2_ardour_fix_carbon_h__
#define __gtk2_ardour_fix_carbon_h__

#ifdef __APPLE__

/* an awful hack to stop Carbon #defines from messing with other code
 */

#ifdef CARBON_FLAT_HEADERS
#include "MacTypes.h"
#else
#include "/System/Library/Frameworks/CoreServices.framework/Headers/../Frameworks/CarbonCore.framework/Headers/MacTypes.h"
#endif
#undef Style
#undef Fixed
#undef Yes
#undef No
#undef nil
#endif

#endif
