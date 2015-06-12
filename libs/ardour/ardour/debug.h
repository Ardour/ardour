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
		LIBARDOUR_API extern DebugBits MidiSourceIO;
		LIBARDOUR_API extern DebugBits MidiPlaylistIO;
		LIBARDOUR_API extern DebugBits MidiDiskstreamIO;
		LIBARDOUR_API extern DebugBits SnapBBT;
		LIBARDOUR_API extern DebugBits Latency;
		LIBARDOUR_API extern DebugBits LatencyCompensation;
		LIBARDOUR_API extern DebugBits Peaks;
		LIBARDOUR_API extern DebugBits Processors;
		LIBARDOUR_API extern DebugBits ProcessThreads;
		LIBARDOUR_API extern DebugBits Graph;
		LIBARDOUR_API extern DebugBits Destruction;
		LIBARDOUR_API extern DebugBits MTC;
		LIBARDOUR_API extern DebugBits LTC;
		LIBARDOUR_API extern DebugBits Transport;
		LIBARDOUR_API extern DebugBits Slave;
		LIBARDOUR_API extern DebugBits SessionEvents;
		LIBARDOUR_API extern DebugBits MidiIO;
		LIBARDOUR_API extern DebugBits MackieControl;
		LIBARDOUR_API extern DebugBits MidiClock;
		LIBARDOUR_API extern DebugBits Monitor;
		LIBARDOUR_API extern DebugBits Solo;
		LIBARDOUR_API extern DebugBits AudioPlayback;
		LIBARDOUR_API extern DebugBits Panning;
		LIBARDOUR_API extern DebugBits LV2;
		LIBARDOUR_API extern DebugBits CaptureAlignment;
		LIBARDOUR_API extern DebugBits PluginManager;
		LIBARDOUR_API extern DebugBits AudioUnits;
		LIBARDOUR_API extern DebugBits ControlProtocols;
		LIBARDOUR_API extern DebugBits CycleTimers;
		LIBARDOUR_API extern DebugBits MidiTrackers;
		LIBARDOUR_API extern DebugBits Layering;
		LIBARDOUR_API extern DebugBits TempoMath;
		LIBARDOUR_API extern DebugBits TempoMap;
		LIBARDOUR_API extern DebugBits OrderKeys;
		LIBARDOUR_API extern DebugBits Automation;
		LIBARDOUR_API extern DebugBits WiimoteControl;
		LIBARDOUR_API extern DebugBits Ports;
		LIBARDOUR_API extern DebugBits AudioEngine;
		LIBARDOUR_API extern DebugBits Soundcloud;
		LIBARDOUR_API extern DebugBits Butler;
	}
}

#endif /* __ardour_debug_h__ */

