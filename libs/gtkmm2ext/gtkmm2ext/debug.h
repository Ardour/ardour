/*
 * Copyright (C) 2013-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libgtkmm2ext_debug_h__
#define __libgtkmm2ext_debug_h__

#include <stdint.h>

#include "pbd/debug.h"

#include "gtkmm2ext/visibility.h"

namespace PBD {
	namespace DEBUG {
                LIBGTKMM2EXT_API extern DebugBits Keyboard;
                LIBGTKMM2EXT_API extern DebugBits Bindings;
                LIBGTKMM2EXT_API extern DebugBits Actions;
	}
}

#endif /* __libgtkm2ext_debug_h__ */

