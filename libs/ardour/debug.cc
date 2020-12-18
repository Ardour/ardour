/*
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

#include <cstring>
#include <cstdlib>
#include <iostream>

#include "ardour/debug.h"

using namespace std;

PBD::DebugBits PBD::DEBUG::AudioEngine = PBD::new_debug_bit ("AudioEngine");
PBD::DebugBits PBD::DEBUG::AudioPlayback = PBD::new_debug_bit ("audioplayback");
PBD::DebugBits PBD::DEBUG::AudioUnitConfig = PBD::new_debug_bit ("AudioUnitConfig");
PBD::DebugBits PBD::DEBUG::AudioUnitGUI = PBD::new_debug_bit ("AudioUnitGUI");
PBD::DebugBits PBD::DEBUG::AudioUnitProcess = PBD::new_debug_bit ("AudioUnitProcess");
PBD::DebugBits PBD::DEBUG::Automation = PBD::new_debug_bit ("automation");
PBD::DebugBits PBD::DEBUG::BackendAudio = PBD::new_debug_bit ("backendaudio");
PBD::DebugBits PBD::DEBUG::BackendCallbacks = PBD::new_debug_bit("BackendCallbacks");
PBD::DebugBits PBD::DEBUG::BackendMIDI = PBD::new_debug_bit ("backendmidi");
PBD::DebugBits PBD::DEBUG::BackendPorts = PBD::new_debug_bit ("backendports");
PBD::DebugBits PBD::DEBUG::BackendThreads = PBD::new_debug_bit ("backendthreads");
PBD::DebugBits PBD::DEBUG::BackendTiming = PBD::new_debug_bit ("backendtiming");
PBD::DebugBits PBD::DEBUG::Butler = PBD::new_debug_bit ("Butler");
PBD::DebugBits PBD::DEBUG::CC121 = PBD::new_debug_bit ("cc121");
PBD::DebugBits PBD::DEBUG::CaptureAlignment = PBD::new_debug_bit ("capturealignment");
PBD::DebugBits PBD::DEBUG::ChanMapping = PBD::new_debug_bit ("chanmapping");
PBD::DebugBits PBD::DEBUG::ContourDesignControl = PBD::new_debug_bit ("contourdesigncontrol");
PBD::DebugBits PBD::DEBUG::ControlProtocols = PBD::new_debug_bit ("controlprotocols");
PBD::DebugBits PBD::DEBUG::CycleTimers = PBD::new_debug_bit ("cycletimers");
PBD::DebugBits PBD::DEBUG::Destruction = PBD::new_debug_bit ("destruction");
PBD::DebugBits PBD::DEBUG::DiskIO = PBD::new_debug_bit ("diskio");
PBD::DebugBits PBD::DEBUG::FaderPort = PBD::new_debug_bit ("faderport");
PBD::DebugBits PBD::DEBUG::FaderPort8 = PBD::new_debug_bit ("faderport8");
PBD::DebugBits PBD::DEBUG::GenericMidi = PBD::new_debug_bit ("genericmidi");
PBD::DebugBits PBD::DEBUG::Graph = PBD::new_debug_bit ("graph");
PBD::DebugBits PBD::DEBUG::LTC = PBD::new_debug_bit ("ltc");
PBD::DebugBits PBD::DEBUG::LV2 = PBD::new_debug_bit ("lv2");
PBD::DebugBits PBD::DEBUG::LV2Automate = PBD::new_debug_bit ("lv2automate");
PBD::DebugBits PBD::DEBUG::LatencyCompensation = PBD::new_debug_bit ("latencycompensation");
PBD::DebugBits PBD::DEBUG::LatencyDelayLine = PBD::new_debug_bit ("latencydelayline");
PBD::DebugBits PBD::DEBUG::LatencyIO = PBD::new_debug_bit ("latencyio");
PBD::DebugBits PBD::DEBUG::LatencyRoute = PBD::new_debug_bit ("latencyroute");
PBD::DebugBits PBD::DEBUG::LaunchControlXL = PBD::new_debug_bit("launchcontrolxl");
PBD::DebugBits PBD::DEBUG::Layering = PBD::new_debug_bit ("layering");
PBD::DebugBits PBD::DEBUG::MTC = PBD::new_debug_bit ("mtc");
PBD::DebugBits PBD::DEBUG::MackieControl = PBD::new_debug_bit ("mackiecontrol");
PBD::DebugBits PBD::DEBUG::MidiClock = PBD::new_debug_bit ("midiclock");
PBD::DebugBits PBD::DEBUG::MidiDiskIO = PBD::new_debug_bit ("mididiskio");
PBD::DebugBits PBD::DEBUG::MidiIO = PBD::new_debug_bit ("midiio");
PBD::DebugBits PBD::DEBUG::MidiPlaylistIO = PBD::new_debug_bit ("midiplaylistio");
PBD::DebugBits PBD::DEBUG::MidiRingBuffer = PBD::new_debug_bit ("midiringbuffer");
PBD::DebugBits PBD::DEBUG::MidiSourceIO = PBD::new_debug_bit ("midisourceio");
PBD::DebugBits PBD::DEBUG::MidiTrackers = PBD::new_debug_bit ("miditrackers");
PBD::DebugBits PBD::DEBUG::Monitor = PBD::new_debug_bit ("monitor");
PBD::DebugBits PBD::DEBUG::OrderKeys = PBD::new_debug_bit ("orderkeys");
PBD::DebugBits PBD::DEBUG::Panning = PBD::new_debug_bit ("panning");
PBD::DebugBits PBD::DEBUG::Peaks = PBD::new_debug_bit ("peaks");
PBD::DebugBits PBD::DEBUG::PluginManager = PBD::new_debug_bit ("pluginmanager");
PBD::DebugBits PBD::DEBUG::PortConnectAuto = PBD::new_debug_bit ("PortConnectAuto");
PBD::DebugBits PBD::DEBUG::PortConnectIO = PBD::new_debug_bit ("PortConnectIO");
PBD::DebugBits PBD::DEBUG::Ports = PBD::new_debug_bit ("Ports");
PBD::DebugBits PBD::DEBUG::ProcessThreads = PBD::new_debug_bit ("processthreads");
PBD::DebugBits PBD::DEBUG::Processors = PBD::new_debug_bit ("processors");
PBD::DebugBits PBD::DEBUG::Push2 = PBD::new_debug_bit ("push2");
PBD::DebugBits PBD::DEBUG::Selection = PBD::new_debug_bit ("selection");
PBD::DebugBits PBD::DEBUG::SessionEvents = PBD::new_debug_bit ("sessionevents");
PBD::DebugBits PBD::DEBUG::Slave = PBD::new_debug_bit ("slave");
PBD::DebugBits PBD::DEBUG::Solo = PBD::new_debug_bit ("solo");
PBD::DebugBits PBD::DEBUG::Soundcloud = PBD::new_debug_bit ("Soundcloud");
PBD::DebugBits PBD::DEBUG::TFSMEvents = PBD::new_debug_bit ("tfsmevents");
PBD::DebugBits PBD::DEBUG::TFSMState = PBD::new_debug_bit ("tfsmstate");
PBD::DebugBits PBD::DEBUG::TXLTC = PBD::new_debug_bit ("tx-ltc");
PBD::DebugBits PBD::DEBUG::TempoMap = PBD::new_debug_bit ("tempomap");
PBD::DebugBits PBD::DEBUG::TempoMath = PBD::new_debug_bit ("tempomath");
PBD::DebugBits PBD::DEBUG::Transport = PBD::new_debug_bit ("transport");
PBD::DebugBits PBD::DEBUG::US2400 = PBD::new_debug_bit ("us2400");
PBD::DebugBits PBD::DEBUG::VCA = PBD::new_debug_bit ("vca");
PBD::DebugBits PBD::DEBUG::VST3Callbacks = PBD::new_debug_bit ("VST3Callbacks");
PBD::DebugBits PBD::DEBUG::VST3Config = PBD::new_debug_bit ("VST3Config");
PBD::DebugBits PBD::DEBUG::VST3Process = PBD::new_debug_bit ("VST3Process");
PBD::DebugBits PBD::DEBUG::VSTCallbacks = PBD::new_debug_bit ("vstcallbacks");
PBD::DebugBits PBD::DEBUG::WiimoteControl = PBD::new_debug_bit ("wiimotecontrol");
