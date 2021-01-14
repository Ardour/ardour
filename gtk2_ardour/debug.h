/*
 * Copyright (C) 2011-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2016 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef __gtk2_ardour_debug_h__
#define __gtk2_ardour_debug_h__

#include <stdint.h>

#include "pbd/debug.h"

namespace PBD {
	namespace DEBUG {
                extern DebugBits Drags;
                extern DebugBits CutNPaste;
                extern DebugBits Accelerators;
                extern DebugBits GUITiming;
                extern DebugBits EngineControl;
		extern DebugBits GuiStartup;
	}
}

#endif /* __gtk2_ardour_debug_h__ */
