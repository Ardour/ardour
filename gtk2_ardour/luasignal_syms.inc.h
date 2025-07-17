/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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

// editor
#if 0
STATIC(SessionLoad, &)
STATIC(SessionClose, &)
#endif
STATIC(ConfigChanged, &ARDOUR::Config->ParameterChanged)

// engine instance
ENGINE(EngineRunning, Running)
ENGINE(EngineStopped, Stopped)
ENGINE(EngineHalted, Halted)
ENGINE(EngineDeviceListChanged, DeviceListChanged)
ENGINE(BufferSizeChanged, BufferSizeChanged)
ENGINE(SampleRateChanged, SampleRateChanged)

// session static
STATIC(FeedbackDetected, &ARDOUR::Session::FeedbackDetected)
STATIC(SuccessfulGraphSort, &ARDOUR::Session::SuccessfulGraphSort)
STATIC(StartTimeChanged, &ARDOUR::Session::StartTimeChanged)
STATIC(EndTimeChanged, &ARDOUR::Session::EndTimeChanged)
STATIC(Exported, &ARDOUR::Session::Exported)

// stripable static globals
STATIC(Change, &PresentationInfo::Change)

// session specific (re-subscribe when session changes)
SESSION(SessionConfigChanged, config.ParameterChanged)
SESSION(TransportStateChange, TransportStateChange)
SESSION(DirtyChanged, DirtyChanged)
SESSION(StateSaved, StateSaved)
SESSION(Xrun, Xrun)
SESSION(TransportLooped, TransportLooped)
SESSION(SoloActive, SoloActive)
SESSION(SoloChanged, SoloChanged)
SESSION(IsolatedChanged, IsolatedChanged)
SESSION(MonitorChanged, MonitorChanged)
SESSION(RecordStateChanged, RecordStateChanged)
SESSION(RecordArmStateChanged, RecordArmStateChanged)
SESSION(AudioLoopLocationChanged, auto_loop_location_changed)
SESSION(AudioPunchLocationChanged, auto_punch_location_changed)
SESSION(LocationsModified, locations_modified)
SESSION(AuditionActive, AuditionActive)
SESSION(BundleAddedOrRemoved, BundleAddedOrRemoved)
SESSION(PositionChanged, PositionChanged)
SESSION(Located, Located)
SESSION(RoutesReconnected, session_routes_reconnected)
SESSION(RouteAdded, RouteAdded)
SESSION(RouteGroupPropertyChanged, RouteGroupPropertyChanged)
SESSION(RouteAddedToRouteGroup, RouteAddedToRouteGroup)
SESSION(RouteRemovedFromRouteGroup, RouteRemovedFromRouteGroup)
SESSION(StepEditStatusChange, StepEditStatusChange)
SESSION(RouteGroupAdded, route_group_added)
SESSION(RouteGroupRemoved, route_group_removed)
SESSION(RouteGroupsReordered, route_groups_reordered)

// plugin manager instance
STATIC(PluginListChanged, &(PluginManager::instance().PluginListChanged))
STATIC(PluginStatusChanged, &(PluginManager::instance().PluginStatusChanged))
//STATIC(PluginStatusesChanged, &(PluginManager::instance().PluginTagsChanged))

// Diskstream static global
STATIC(DiskOverrun, &ARDOUR::DiskWriter::Overrun)
STATIC(DiskUnderrun, &ARDOUR::DiskReader::Underrun)

// Region static
STATIC(RegionsPropertyChanged, &ARDOUR::Region::RegionsPropertyChanged)

// Timers
STATIC(LuaTimerS,  &LuaInstance::LuaTimerS)
STATIC(LuaTimerDS, &LuaInstance::LuaTimerDS)

// Session load
STATIC(SetSession, &LuaInstance::SetSession)

// Editor Selection Changed
STATIC(SelectionChanged, &LuaInstance::SelectionChanged)

// TODO per track/route signals,
// TODO per plugin actions / controllables
// TODO per region actions
//SESSIONOBJECT(PropertyChanged, &ARDOUR::Stateful::PropertyChanged, 1)

// TODO any location action

// /////////////////////////////////////////////////////////////////////
// NOTE: WHEN ADDING/REMOVING SIGNALS, UPDATE libs/ardour/luabindings.cc
// TO MATCH THE TOTAL NUMBER OF SIGNALS.
//
// CLASSKEYS(std::bitset<50ul>); // LuaSignal::LAST_SIGNAL
// /////////////////////////////////////////////////////////////////////
