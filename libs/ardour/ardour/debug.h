/*
    Copyright (C) 2009 Paul Davis

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

#ifndef __ardour_debug_h__
#define __ardour_debug_h__

#include <stdint.h>

#include <sstream>

#include "pbd/debug.h"

namespace PBD {
	namespace DEBUG {
		extern uint64_t MidiSourceIO;
		extern uint64_t MidiPlaylistIO;
		extern uint64_t MidiDiskstreamIO;
		extern uint64_t SnapBBT;
		extern uint64_t Configuration;
		extern uint64_t Latency;
		extern uint64_t Peaks;
		extern uint64_t Processors;
		extern uint64_t ProcessThreads;
		extern uint64_t Graph;
		extern uint64_t Destruction;
		extern uint64_t MTC;
		extern uint64_t LTC;
		extern uint64_t Transport;
		extern uint64_t Slave;
		extern uint64_t SessionEvents;
		extern uint64_t MidiIO;
		extern uint64_t MackieControl;
		extern uint64_t MidiClock;
		extern uint64_t Monitor;
		extern uint64_t Solo;
		extern uint64_t AudioPlayback;
		extern uint64_t Panning;
		extern uint64_t LV2;
		extern uint64_t CaptureAlignment;
		extern uint64_t PluginManager;
		extern uint64_t AudioUnits;
		extern uint64_t ControlProtocols;
		extern uint64_t CycleTimers;
		extern uint64_t MidiTrackers;
		extern uint64_t Layering;
		extern uint64_t TempoMath;
		extern uint64_t TempoMap;
		extern uint64_t OrderKeys;
		extern uint64_t Automation;
		extern uint64_t WiimoteControl;
	}
}

#endif /* __ardour_debug_h__ */

