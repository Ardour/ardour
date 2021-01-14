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

#include <cstring>
#include <cstdlib>
#include <iostream>

#include "debug.h"

using namespace std;

PBD::DebugBits PBD::DEBUG::Drags = PBD::new_debug_bit ("drags");
PBD::DebugBits PBD::DEBUG::CutNPaste = PBD::new_debug_bit ("cutnpaste");
PBD::DebugBits PBD::DEBUG::Accelerators = PBD::new_debug_bit ("accelerators");
PBD::DebugBits PBD::DEBUG::GUITiming = PBD::new_debug_bit ("guitiming");
PBD::DebugBits PBD::DEBUG::EngineControl = PBD::new_debug_bit ("enginecontrol");
PBD::DebugBits PBD::DEBUG::GuiStartup = PBD::new_debug_bit ("guistartup");
