/*
    Copyright (C) 2011 Paul Davis

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

#ifndef __libgtkmm2ext_debug_h__
#define __libgtkmm2ext_debug_h__

#include "gtkmm2ext/visibility.h"

#include <stdint.h>

namespace Gtkmm2ext {
	namespace DEBUG {
                LIBGTKMM2EXT_API extern uint64_t Keyboard;
                LIBGTKMM2EXT_API extern uint64_t Bindings;
	}
}

#endif /* __libgtkm2ext_debug_h__ */

