/*
 * Copyright (C) 2020 Paul Davis <paul@linuxaudiosystems.com>
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


#include "temporal/debug.h"

using namespace std;

PBD::DebugBits PBD::DEBUG::TemporalMap = PBD::new_debug_bit ("TemporalMap");
PBD::DebugBits PBD::DEBUG::TemporalDomainConvert = PBD::new_debug_bit ("TemporalDomainConvert");
PBD::DebugBits PBD::DEBUG::Grid = PBD::new_debug_bit ("Grid");
PBD::DebugBits PBD::DEBUG::SnapBBT = PBD::new_debug_bit ("SnapBBT");
PBD::DebugBits PBD::DEBUG::Beats = PBD::new_debug_bit ("Beats");

