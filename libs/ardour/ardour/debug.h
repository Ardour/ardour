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

#include "ardour/libardour_visibility.h"
#include "pbd/debug.h"

namespace PBD {
	namespace DEBUG {
		LIBARDOUR_API extern uint64_t MidiSourceIO;
		LIBARDOUR_API extern uint64_t MidiPlaylistIO;
		LIBARDOUR_API extern uint64_t MidiDiskstreamIO;
		LIBARDOUR_API extern uint64_t SnapBBT;
		LIBARDOUR_API extern uint64_t Configuration;
		LIBARDOUR_API extern uint64_t Latency;
		LIBARDOUR_API extern uint64_t Peaks;
		LIBARDOUR_API extern uint64_t Processors;
		LIBARDOUR_API extern uint64_t ProcessThreads;
		LIBARDOUR_API extern uint64_t Graph;
		LIBARDOUR_API extern uint64_t Destruction;
		LIBARDOUR_API extern uint64_t MTC;
		LIBARDOUR_API extern uint64_t LTC;
		LIBARDOUR_API extern uint64_t Transport;
		LIBARDOUR_API extern uint64_t Slave;
		LIBARDOUR_API extern uint64_t SessionEvents;
		LIBARDOUR_API extern uint64_t MidiIO;
		LIBARDOUR_API extern uint64_t MackieControl;
		LIBARDOUR_API extern uint64_t MidiClock;
		LIBARDOUR_API extern uint64_t Monitor;
		LIBARDOUR_API extern uint64_t Solo;
		LIBARDOUR_API extern uint64_t AudioPlayback;
		LIBARDOUR_API extern uint64_t Panning;
		LIBARDOUR_API extern uint64_t LV2;
		LIBARDOUR_API extern uint64_t CaptureAlignment;
		LIBARDOUR_API extern uint64_t PluginManager;
		LIBARDOUR_API extern uint64_t AudioUnits;
		LIBARDOUR_API extern uint64_t ControlProtocols;
		LIBARDOUR_API extern uint64_t CycleTimers;
		LIBARDOUR_API extern uint64_t MidiTrackers;
		LIBARDOUR_API extern uint64_t Layering;
		LIBARDOUR_API extern uint64_t TempoMath;
		LIBARDOUR_API extern uint64_t TempoMap;
		LIBARDOUR_API extern uint64_t OrderKeys;
		LIBARDOUR_API extern uint64_t Automation;
		LIBARDOUR_API extern uint64_t WiimoteControl;
		LIBARDOUR_API extern uint64_t Ports;
	}
}

#endif /* __ardour_debug_h__ */

