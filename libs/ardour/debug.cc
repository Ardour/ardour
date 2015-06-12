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

#include <cstring>
#include <cstdlib>
#include <iostream>

#include "ardour/debug.h"

using namespace std;

PBD::DebugBits PBD::DEBUG::MidiSourceIO = PBD::new_debug_bit ("midisourceio");
PBD::DebugBits PBD::DEBUG::MidiPlaylistIO = PBD::new_debug_bit ("midiplaylistio");
PBD::DebugBits PBD::DEBUG::MidiDiskstreamIO = PBD::new_debug_bit ("mididiskstreamio");
PBD::DebugBits PBD::DEBUG::SnapBBT = PBD::new_debug_bit ("snapbbt");
PBD::DebugBits PBD::DEBUG::Latency = PBD::new_debug_bit ("latency");
PBD::DebugBits PBD::DEBUG::LatencyCompensation = PBD::new_debug_bit ("latencycompensation");
PBD::DebugBits PBD::DEBUG::Peaks = PBD::new_debug_bit ("peaks");
PBD::DebugBits PBD::DEBUG::Processors = PBD::new_debug_bit ("processors");
PBD::DebugBits PBD::DEBUG::ProcessThreads = PBD::new_debug_bit ("processthreads");
PBD::DebugBits PBD::DEBUG::Graph = PBD::new_debug_bit ("graph");
PBD::DebugBits PBD::DEBUG::Destruction = PBD::new_debug_bit ("destruction");
PBD::DebugBits PBD::DEBUG::MTC = PBD::new_debug_bit ("mtc");
PBD::DebugBits PBD::DEBUG::LTC = PBD::new_debug_bit ("ltc");
PBD::DebugBits PBD::DEBUG::Transport = PBD::new_debug_bit ("transport");
PBD::DebugBits PBD::DEBUG::Slave = PBD::new_debug_bit ("slave");
PBD::DebugBits PBD::DEBUG::SessionEvents = PBD::new_debug_bit ("sessionevents");
PBD::DebugBits PBD::DEBUG::MidiIO = PBD::new_debug_bit ("midiio");
PBD::DebugBits PBD::DEBUG::MackieControl = PBD::new_debug_bit ("mackiecontrol");
PBD::DebugBits PBD::DEBUG::MidiClock = PBD::new_debug_bit ("midiclock");
PBD::DebugBits PBD::DEBUG::Monitor = PBD::new_debug_bit ("monitor");
PBD::DebugBits PBD::DEBUG::Solo = PBD::new_debug_bit ("solo");
PBD::DebugBits PBD::DEBUG::AudioPlayback = PBD::new_debug_bit ("audioplayback");
PBD::DebugBits PBD::DEBUG::Panning = PBD::new_debug_bit ("panning");
PBD::DebugBits PBD::DEBUG::LV2 = PBD::new_debug_bit ("lv2");
PBD::DebugBits PBD::DEBUG::CaptureAlignment = PBD::new_debug_bit ("capturealignment");
PBD::DebugBits PBD::DEBUG::PluginManager = PBD::new_debug_bit ("pluginmanager");
PBD::DebugBits PBD::DEBUG::AudioUnits = PBD::new_debug_bit ("audiounits");
PBD::DebugBits PBD::DEBUG::ControlProtocols = PBD::new_debug_bit ("controlprotocols");
PBD::DebugBits PBD::DEBUG::CycleTimers = PBD::new_debug_bit ("cycletimers");
PBD::DebugBits PBD::DEBUG::MidiTrackers = PBD::new_debug_bit ("miditrackers");
PBD::DebugBits PBD::DEBUG::Layering = PBD::new_debug_bit ("layering");
PBD::DebugBits PBD::DEBUG::TempoMath = PBD::new_debug_bit ("tempomath");
PBD::DebugBits PBD::DEBUG::TempoMap = PBD::new_debug_bit ("tempomap");
PBD::DebugBits PBD::DEBUG::OrderKeys = PBD::new_debug_bit ("orderkeys");
PBD::DebugBits PBD::DEBUG::Automation = PBD::new_debug_bit ("automation");
PBD::DebugBits PBD::DEBUG::WiimoteControl = PBD::new_debug_bit ("wiimotecontrol");
PBD::DebugBits PBD::DEBUG::Ports = PBD::new_debug_bit ("Ports");
PBD::DebugBits PBD::DEBUG::AudioEngine = PBD::new_debug_bit ("AudioEngine");
PBD::DebugBits PBD::DEBUG::Soundcloud = PBD::new_debug_bit ("Soundcloud");
PBD::DebugBits PBD::DEBUG::Butler = PBD::new_debug_bit ("Butler");


