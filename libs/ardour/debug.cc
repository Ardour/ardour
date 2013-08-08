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

uint64_t PBD::DEBUG::MidiSourceIO = PBD::new_debug_bit ("midisourceio");
uint64_t PBD::DEBUG::MidiPlaylistIO = PBD::new_debug_bit ("midiplaylistio");
uint64_t PBD::DEBUG::MidiDiskstreamIO = PBD::new_debug_bit ("mididiskstreamio");
uint64_t PBD::DEBUG::SnapBBT = PBD::new_debug_bit ("snapbbt");
uint64_t PBD::DEBUG::Configuration = PBD::new_debug_bit ("configuration");
uint64_t PBD::DEBUG::Latency = PBD::new_debug_bit ("latency");
uint64_t PBD::DEBUG::Processors = PBD::new_debug_bit ("processors");
uint64_t PBD::DEBUG::ProcessThreads = PBD::new_debug_bit ("processthreads");
uint64_t PBD::DEBUG::Graph = PBD::new_debug_bit ("graph");
uint64_t PBD::DEBUG::Destruction = PBD::new_debug_bit ("destruction");
uint64_t PBD::DEBUG::MTC = PBD::new_debug_bit ("mtc");
uint64_t PBD::DEBUG::LTC = PBD::new_debug_bit ("ltc");
uint64_t PBD::DEBUG::Transport = PBD::new_debug_bit ("transport");
uint64_t PBD::DEBUG::Slave = PBD::new_debug_bit ("slave");
uint64_t PBD::DEBUG::SessionEvents = PBD::new_debug_bit ("sessionevents");
uint64_t PBD::DEBUG::MidiIO = PBD::new_debug_bit ("midiio");
uint64_t PBD::DEBUG::MackieControl = PBD::new_debug_bit ("mackiecontrol");
uint64_t PBD::DEBUG::MidiClock = PBD::new_debug_bit ("midiclock");
uint64_t PBD::DEBUG::Monitor = PBD::new_debug_bit ("monitor");
uint64_t PBD::DEBUG::Solo = PBD::new_debug_bit ("solo");
uint64_t PBD::DEBUG::AudioPlayback = PBD::new_debug_bit ("audioplayback");
uint64_t PBD::DEBUG::Panning = PBD::new_debug_bit ("panning");
uint64_t PBD::DEBUG::LV2 = PBD::new_debug_bit ("lv2");
uint64_t PBD::DEBUG::CaptureAlignment = PBD::new_debug_bit ("capturealignment");
uint64_t PBD::DEBUG::PluginManager = PBD::new_debug_bit ("pluginmanager");
uint64_t PBD::DEBUG::AudioUnits = PBD::new_debug_bit ("audiounits");
uint64_t PBD::DEBUG::ControlProtocols = PBD::new_debug_bit ("controlprotocols");
uint64_t PBD::DEBUG::CycleTimers = PBD::new_debug_bit ("cycletimers");
uint64_t PBD::DEBUG::MidiTrackers = PBD::new_debug_bit ("miditrackers");
uint64_t PBD::DEBUG::Layering = PBD::new_debug_bit ("layering");
uint64_t PBD::DEBUG::TempoMath = PBD::new_debug_bit ("tempomath");
uint64_t PBD::DEBUG::TempoMap = PBD::new_debug_bit ("tempomap");
uint64_t PBD::DEBUG::OrderKeys = PBD::new_debug_bit ("orderkeys");
uint64_t PBD::DEBUG::Automation = PBD::new_debug_bit ("automation");
uint64_t PBD::DEBUG::WiimoteControl = PBD::new_debug_bit ("wiimotecontrol");
uint64_t PBD::DEBUG::Ports = PBD::new_debug_bit ("Ports");


