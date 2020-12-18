/*
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef __ardour_debug_h__
#define __ardour_debug_h__

#include <stdint.h>

#include <sstream>

#include "ardour/libardour_visibility.h"
#include "pbd/debug.h"

namespace PBD {
	namespace DEBUG {
		LIBARDOUR_API extern DebugBits AudioEngine;
		LIBARDOUR_API extern DebugBits AudioPlayback;
		LIBARDOUR_API extern DebugBits AudioUnitConfig;
		LIBARDOUR_API extern DebugBits AudioUnitGUI;
		LIBARDOUR_API extern DebugBits AudioUnitProcess;
		LIBARDOUR_API extern DebugBits Automation;
		LIBARDOUR_API extern DebugBits BackendAudio;
		LIBARDOUR_API extern DebugBits BackendCallbacks;
		LIBARDOUR_API extern DebugBits BackendMIDI;
		LIBARDOUR_API extern DebugBits BackendPorts;
		LIBARDOUR_API extern DebugBits BackendThreads;
		LIBARDOUR_API extern DebugBits BackendTiming;
		LIBARDOUR_API extern DebugBits Butler;
		LIBARDOUR_API extern DebugBits CC121;
		LIBARDOUR_API extern DebugBits CaptureAlignment;
		LIBARDOUR_API extern DebugBits ChanMapping;
		LIBARDOUR_API extern DebugBits ContourDesignControl;
		LIBARDOUR_API extern DebugBits ControlProtocols;
		LIBARDOUR_API extern DebugBits CycleTimers;
		LIBARDOUR_API extern DebugBits Destruction;
		LIBARDOUR_API extern DebugBits DiskIO;
		LIBARDOUR_API extern DebugBits FaderPort8;
		LIBARDOUR_API extern DebugBits FaderPort;
		LIBARDOUR_API extern DebugBits GenericMidi;
		LIBARDOUR_API extern DebugBits Graph;
		LIBARDOUR_API extern DebugBits LTC;
		LIBARDOUR_API extern DebugBits LV2;
		LIBARDOUR_API extern DebugBits LV2Automate;
		LIBARDOUR_API extern DebugBits LatencyCompensation;
		LIBARDOUR_API extern DebugBits LatencyDelayLine;
		LIBARDOUR_API extern DebugBits LatencyIO;
		LIBARDOUR_API extern DebugBits LatencyRoute;
		LIBARDOUR_API extern DebugBits LaunchControlXL;
		LIBARDOUR_API extern DebugBits Layering;
		LIBARDOUR_API extern DebugBits MTC;
		LIBARDOUR_API extern DebugBits MackieControl;
		LIBARDOUR_API extern DebugBits MidiClock;
		LIBARDOUR_API extern DebugBits MidiDiskIO;
		LIBARDOUR_API extern DebugBits MidiIO;
		LIBARDOUR_API extern DebugBits MidiPlaylistIO;
		LIBARDOUR_API extern DebugBits MidiRingBuffer;
		LIBARDOUR_API extern DebugBits MidiSourceIO;
		LIBARDOUR_API extern DebugBits MidiTrackers;
		LIBARDOUR_API extern DebugBits Monitor;
		LIBARDOUR_API extern DebugBits OrderKeys;
		LIBARDOUR_API extern DebugBits Panning;
		LIBARDOUR_API extern DebugBits Peaks;
		LIBARDOUR_API extern DebugBits PluginManager;
		LIBARDOUR_API extern DebugBits PortConnectAuto;
		LIBARDOUR_API extern DebugBits PortConnectIO;
		LIBARDOUR_API extern DebugBits Ports;
		LIBARDOUR_API extern DebugBits ProcessThreads;
		LIBARDOUR_API extern DebugBits Processors;
		LIBARDOUR_API extern DebugBits Push2;
		LIBARDOUR_API extern DebugBits Selection;
		LIBARDOUR_API extern DebugBits SessionEvents;
		LIBARDOUR_API extern DebugBits Slave;
		LIBARDOUR_API extern DebugBits Solo;
		LIBARDOUR_API extern DebugBits Soundcloud;
		LIBARDOUR_API extern DebugBits TFSMEvents;
		LIBARDOUR_API extern DebugBits TFSMState;
		LIBARDOUR_API extern DebugBits TXLTC;
		LIBARDOUR_API extern DebugBits TempoMap;
		LIBARDOUR_API extern DebugBits TempoMath;
		LIBARDOUR_API extern DebugBits Transport;
		LIBARDOUR_API extern DebugBits US2400;
		LIBARDOUR_API extern DebugBits VCA;
		LIBARDOUR_API extern DebugBits VST3Callbacks;
		LIBARDOUR_API extern DebugBits VST3Config;
		LIBARDOUR_API extern DebugBits VST3Process;
		LIBARDOUR_API extern DebugBits VSTCallbacks;
		LIBARDOUR_API extern DebugBits WiimoteControl;

	}
}

#endif /* __ardour_debug_h__ */
