/*
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Térence Clastres <t.clastres@gmail.com>
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

#include <algorithm>

#include "launch_control_xl.h"
#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/debug.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/search_path.h"
#include "pbd/enumwriter.h"

#include "midi++/parser.h"

#include "ardour/amp.h"
#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/midiport_manager.h"
#include "ardour/midi_track.h"
#include "ardour/midi_port.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/types_convert.h"
#include "ardour/vca_manager.h"

#include "gui.h"

#include "pbd/i18n.h"

using namespace ArdourSurface;
using namespace ARDOUR;
using namespace std;
using namespace PBD;

MidiByteArray
LaunchControlXL::SelectButton::state_msg(bool light) const {
  uint8_t velocity = ( color() + flag() ) * light;
  return MidiByteArray (11, 0xF0, 0x00, 0x20, 0x29, 0x02, 0x11, 0x78, lcxl->template_number(), index(), velocity, 0xF7);
}

MidiByteArray
LaunchControlXL::TrackButton::state_msg(bool light) const {
  uint8_t velocity = ( color() + flag() ) * light;
  return MidiByteArray (11, 0xF0, 0x00, 0x20, 0x29, 0x02, 0x11, 0x78, lcxl->template_number(), index(), velocity, 0xF7);

}

MidiByteArray
LaunchControlXL::TrackStateButton::state_msg(bool light) const {
  uint8_t velocity = ( color() + flag() ) * light;
  return MidiByteArray (11, 0xF0, 0x00, 0x20, 0x29, 0x02, 0x11, 0x78, lcxl->template_number(), index(), velocity, 0xF7);

}

MidiByteArray
LaunchControlXL::Knob::state_msg(bool light) const {
  uint8_t velocity = ( color() + flag() ) * light;
  return MidiByteArray (11, 0xF0, 0x00, 0x20, 0x29, 0x02, 0x11, 0x78, lcxl->template_number(), index(), velocity, 0xF7);
}
