/*
    Copyright (C) 2016 Robin Gareus <robin@gareus.org>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "timecode/bbt_time.h"
#include "evoral/Control.hpp"
#include "evoral/ControlList.hpp"

#include "ardour/audioengine.h"
#include "ardour/audiosource.h"
#include "ardour/audio_backend.h"
#include "ardour/audio_buffer.h"
#include "ardour/audio_track.h"
#include "ardour/buffer_set.h"
#include "ardour/chan_mapping.h"
#include "ardour/dB.h"
#include "ardour/dsp_filter.h"
#include "ardour/lua_api.h"
#include "ardour/luabindings.h"
#include "ardour/meter.h"
#include "ardour/midi_track.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/runtime_functions.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/session_object.h"
#include "ardour/tempo.h"

#include "LuaBridge/LuaBridge.h"

/* Some notes on Lua bindings for libardour and friends
 *
 * - Prefer factory methods over Contructors whenever possible.
 *   Don't expose the constructor method unless required.
 *
 *   e.g. Don't allow the script to construct a "Track" Object directly
 *   but do allow to create a "BBT_TIME" object.
 *
 * - Do not dereference Shared or Weak Pointers. Pass the pointer to Lua.
 * - Define Objects as boost:shared_ptr Object whenever possible.
 *
 *   Storing a boost::shared_ptr in a Lua-variable keeps the reference
 *   until that variable is set to 'nil'.
 *   (if the script were to keep a direct pointer to the object instance, the
 *   behaviour is undefined if the actual object goes away)
 *
 *   Methods of the actual class are indirectly exposed,
 *   boost::*_ptr get() and ::lock() is implicit when the class is exported
 *   as LuaBridge's "WSPtrClass".
 */

using namespace ARDOUR;

void
LuaBindings::stddef (lua_State* L)
{
	// std::list<std::string>
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginStdList <std::string> ("StringList")
		.endClass ()
		.endNamespace ();

	// std::vector<std::string>
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginStdVector <std::string> ("StringVector")
		.endClass ()
		.endNamespace ();

	// register float array (float*)
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.registerArray <float> ("FloatArray")
		.endNamespace ();

	// register float array (int32_t*)
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.registerArray <int32_t> ("IntArray")
		.endNamespace ();

	// std::vector<std::string>
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginStdVector <double> ("DoubleVector")
		.endClass ()
		.endNamespace ();

	// TODO std::set
}

void
LuaBindings::common (lua_State* L)
{
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("PBD")
		.beginClass <PBD::ID> ("ID")
		.addConstructor <void (*) (std::string)> ()
		.addFunction ("to_s", &PBD::ID::to_s) // TODO special case LUA __tostring ?
		.endClass ()

		.beginWSPtrClass <PBD::Stateful> ("Stateful")
		.addFunction ("properties", &PBD::Stateful::properties)
		.endClass ()

		.deriveWSPtrClass <PBD::StatefulDestructible, PBD::Stateful> ("StatefulDestructible")
		.endClass ()

		.deriveWSPtrClass <PBD::Controllable, PBD::StatefulDestructible> ("Controllable")
		.addFunction ("get_value", &PBD::Controllable::get_value)
		.endClass ()

		.beginNamespace ("GroupControlDisposition")
		.addConst ("InverseGroup", PBD::Controllable::GroupControlDisposition(PBD::Controllable::InverseGroup))
		.addConst ("NoGroup", PBD::Controllable::GroupControlDisposition(PBD::Controllable::NoGroup))
		.addConst ("UseGroup", PBD::Controllable::GroupControlDisposition(PBD::Controllable::UseGroup))
		.endNamespace ()

		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR") // XXX really libtimecode
		.beginClass <Timecode::BBT_Time> ("BBT_TIME")
		.addConstructor <void (*) (uint32_t, uint32_t, uint32_t)> ()
		.endClass ()
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginWSPtrClass <PluginInfo> ("PluginInfo")
		.addVoidConstructor ()
		.endClass ()

		.beginNamespace ("Route")
		.beginClass <Route::ProcessorStreams> ("ProcessorStreams")
		.addVoidConstructor ()
		.endClass ()
		.endNamespace ()

		.beginNamespace ("Properties")
		// templated class definitions
		.beginClass <PBD::PropertyDescriptor<bool> > ("BoolProperty").endClass ()
		.beginClass <PBD::PropertyDescriptor<float> > ("FloatProperty").endClass ()
		.beginClass <PBD::PropertyDescriptor<framepos_t> > ("FrameposProperty").endClass ()
		// actual references (TODO: also expose GQuark for std::set)
		//   ardour/region.h
		.addConst ("Start", &ARDOUR::Properties::start)
		.addConst ("Length", &ARDOUR::Properties::length)
		.addConst ("Position", &ARDOUR::Properties::position)
		.endNamespace ()

		.beginClass <PBD::PropertyChange> ("PropertyChange")
		// TODO add special handling (std::set<PropertyID>), PropertyID is a GQuark.
		// -> direct map to lua table  beginStdSet()A
		//
		// expand templated PropertyDescriptor<T>
		.addFunction ("containsBool", &PBD::PropertyChange::contains<bool>)
		.addFunction ("containsFloat", &PBD::PropertyChange::contains<float>)
		.addFunction ("containsFramePos", &PBD::PropertyChange::contains<framepos_t>)
		.endClass ()

		.beginClass <PBD::PropertyList> ("PropertyList")
		// is-a  std::map<PropertyID, PropertyBase*>
		.endClass ()

		.deriveClass <PBD::OwnedPropertyList, PBD::PropertyList> ("OwnedPropertyList")
		.endClass ()

		.deriveWSPtrClass <SessionObject, PBD::StatefulDestructible> ("SessionObject")
		.addFunction ("name", &SessionObject::name)
		.endClass ()

		.deriveWSPtrClass <Route, SessionObject> ("Route")
		.addFunction ("set_name", &Route::set_name)
		.addFunction ("comment", &Route::comment)
		.addFunction ("active", &Route::active)
		.addFunction ("set_active", &Route::set_active)
		.addFunction ("nth_plugin", &Route::nth_plugin)
		.addFunction ("add_processor_by_index", &Route::add_processor_by_index)
		.endClass ()

		.deriveWSPtrClass <Track, Route> ("Track")
		.addFunction ("set_name", &Track::set_name)
		.addFunction ("can_record", &Track::can_record)
		.addFunction ("record_enabled", &Track::record_enabled)
		.addFunction ("record_safe", &Track::record_safe)
		.addFunction ("set_record_enabled", &Track::set_record_enabled)
		.addFunction ("set_record_safe", &Track::set_record_safe)
		.endClass ()

		.deriveWSPtrClass <AudioTrack, Track> ("AudioTrack")
		.endClass ()

		.deriveWSPtrClass <MidiTrack, Track> ("MidiTrack")
		.endClass ()

		.deriveWSPtrClass <Region, SessionObject> ("Region")
		/* properties */
		.addFunction ("position", &Region::position)
		.addFunction ("start", &Region::start)
		.addFunction ("length", &Region::length)
		.addFunction ("layer", &Region::layer)
		.addFunction ("data_type", &Region::data_type)
		.addFunction ("stretch", &Region::stretch)
		.addFunction ("shift", &Region::shift)
		.addFunction ("sync_offset", &Region::sync_offset)
		.addFunction ("sync_position", &Region::sync_position)
		.addFunction ("hidden", &Region::hidden)
		.addFunction ("muted", &Region::muted)
		.addFunction ("opaque", &Region::opaque)
		.addFunction ("locked", &Region::locked)
		.addFunction ("position_locked", &Region::position_locked)
		.addFunction ("video_locked", &Region::video_locked)
		.addFunction ("valid_transients", &Region::valid_transients)
		.addFunction ("automatic", &Region::automatic)
		.addFunction ("whole_file", &Region::whole_file)
		.addFunction ("captured", &Region::captured)
		.addFunction ("can_move", &Region::can_move)
		.addFunction ("sync_marked", &Region::sync_marked)
		.addFunction ("external", &Region::external)
		.addFunction ("import", &Region::import)
		.addFunction ("covers", &Region::covers)
		.addFunction ("at_natural_position", &Region::at_natural_position)
		.addFunction ("is_compound", &Region::is_compound)
		/* editing operations */
		.addFunction ("set_length", &Region::set_length)
		.addFunction ("set_start", &Region::set_start)
		.addFunction ("set_position", &Region::set_position)
		.addFunction ("set_initial_position", &Region::set_initial_position)
		.addFunction ("nudge_position", &Region::nudge_position)
		.addFunction ("move_to_natural_position", &Region::move_to_natural_position)
		.addFunction ("move_start", &Region::move_start)
		.addFunction ("trim_front", &Region::trim_front)
		.addFunction ("trim_end", &Region::trim_end)
		.addFunction ("trim_to", &Region::trim_to)
		.addFunction ("cut_front", &Region::cut_front)
		.addFunction ("cut_end", &Region::cut_end)
		.addFunction ("raise", &Region::raise)
		.addFunction ("lower", &Region::lower)
		.addFunction ("raise_to_top", &Region::raise_to_top)
		.addFunction ("lower_to_bottom", &Region::lower_to_bottom)
		.addFunction ("set_sync_position", &Region::set_sync_position)
		.addFunction ("clear_sync_position", &Region::clear_sync_position)
		.addFunction ("set_hidden", &Region::set_hidden)
		.addFunction ("set_muted", &Region::set_muted)
		.addFunction ("set_opaque", &Region::set_opaque)
		.addFunction ("set_locked", &Region::set_locked)
		.addFunction ("set_video_locked", &Region::set_video_locked)
		.addFunction ("set_position_locked", &Region::set_position_locked)
		.endClass ()

		.beginWSPtrClass <Source> ("Source")
		.endClass ()

		.beginClass <Evoral::Parameter> ("EvoralParameter")
		.addConstructor <void (*) (uint32_t, uint8_t, uint32_t)> ()
		.addFunction ("type", &Evoral::Parameter::type)
		.addFunction ("channel", &Evoral::Parameter::channel)
		.addFunction ("id", &Evoral::Parameter::id)
		.endClass ()

		.beginWSPtrClass <Evoral::ControlList> ("EvoralControlList")
		.addFunction ("add", &Evoral::ControlList::add)
		.endClass ()

		.beginWSPtrClass <Evoral::ControlSet> ("EvoralControlSet")
		.endClass ()

		.deriveWSPtrClass <Automatable, Evoral::ControlSet> ("Automatable")
		.addFunction ("automation_control", (boost::shared_ptr<AutomationControl>(Automatable::*)(const Evoral::Parameter&, bool))&Automatable::automation_control)
		.endClass ()

		.beginWSPtrClass <Evoral::Control> ("EvoralControl")
		.addFunction ("list", (boost::shared_ptr<Evoral::ControlList>(Evoral::Control::*)())&Evoral::Control::list)
		.endClass ()

		.beginClass <Evoral::ParameterDescriptor> ("EvoralParameterDescriptor")
		.addVoidConstructor ()
		.addData ("lower", &Evoral::ParameterDescriptor::lower)
		.addData ("upper", &Evoral::ParameterDescriptor::upper)
		.addData ("normal", &Evoral::ParameterDescriptor::normal)
		.addData ("toggled", &Evoral::ParameterDescriptor::toggled)
		.endClass ()

		.deriveClass <ParameterDescriptor, Evoral::ParameterDescriptor> ("ParameterDescriptor")
		.addVoidConstructor ()
		.addData ("label", &ParameterDescriptor::label)
		.addData ("logarithmic", &ParameterDescriptor::logarithmic)
		.endClass ()

		.deriveWSPtrClass <Processor, SessionObject> ("Processor")
		// TODO mult. inheritance
		.endClass ()

		.deriveWSPtrClass <Processor, Automatable> ("Processor")
		.addCast<PluginInsert> ("to_insert")
		.addFunction ("display_name", &Processor::display_name)
		.addFunction ("active", &Processor::active)
		.addFunction ("activate", &Processor::activate)
		.addFunction ("deactivate", &Processor::deactivate)
		.addFunction ("control", (boost::shared_ptr<Evoral::Control>(Evoral::ControlSet::*)(const Evoral::Parameter&, bool))&Evoral::ControlSet::control)
		.addFunction ("automation_control", (boost::shared_ptr<AutomationControl>(Automatable::*)(const Evoral::Parameter&, bool))&Automatable::automation_control)
		.endClass ()

		.deriveWSPtrClass <Plugin, PBD::StatefulDestructible> ("PluginInsert")
		.addFunction ("label", &Plugin::label)
		.addFunction ("name", &Plugin::name)
		.addFunction ("maker", &Plugin::maker)
		.addFunction ("parameter_count", &Plugin::parameter_count)
		.addFunction ("nth_parameter", &Plugin::nth_parameter)
		.addFunction ("parameter_is_input", &Plugin::parameter_is_input)
		.addFunction ("get_docs", &Plugin::get_docs)
		.addFunction ("get_parameter_docs", &Plugin::get_parameter_docs)
		.addRefFunction ("get_parameter_descriptor", &Plugin::get_parameter_descriptor)
		.endClass ()

		.deriveWSPtrClass <PluginInsert, Processor> ("PluginInsert")
		.addFunction ("plugin", &PluginInsert::plugin)
		.addFunction ("activate", &PluginInsert::activate)
		.addFunction ("deactivate", &PluginInsert::deactivate)
		.endClass ()

		.deriveWSPtrClass <AutomationControl, Evoral::Control> ("AutomationControl")
		.addFunction ("automation_state", &AutomationControl::automation_state)
		.addFunction ("automation_style", &AutomationControl::automation_style)
		.addFunction ("set_automation_state", &AutomationControl::set_automation_state)
		.addFunction ("set_automation_style", &AutomationControl::set_automation_style)
		.addFunction ("start_touch", &AutomationControl::start_touch)
		.addFunction ("stop_touch", &AutomationControl::stop_touch)
		.addFunction ("get_value", &AutomationControl::get_value)
		.addFunction ("set_value", &AutomationControl::set_value)
		.addFunction ("writable", &AutomationControl::writable)
		.endClass ()

		.deriveWSPtrClass <PluginInsert::PluginControl, AutomationControl> ("PluginControl")
		.endClass ()

		.deriveWSPtrClass <AudioSource, Source> ("AudioSource")
		.addFunction ("readable_length", &AudioSource::readable_length)
		.addFunction ("n_channels", &AudioSource::n_channels)
		.endClass ()

	// <std::list<boost::shared_ptr <AudioTrack> >
		.beginStdList <boost::shared_ptr<AudioTrack> > ("AudioTrackList")
		.endClass ()

	// std::list<boost::shared_ptr <MidiTrack> >
		.beginStdList <boost::shared_ptr<MidiTrack> > ("MidiTrackList")
		.endClass ()

	// RouteList ==  boost::shared_ptr<std::list<boost::shared_ptr<Route> > >
		.beginPtrStdList <boost::shared_ptr<Route> > ("RouteListPtr")
		.endClass ()

	// typedef std::list<boost::weak_ptr <Route> > WeakRouteList
		.beginConstStdList <boost::weak_ptr<Route> > ("WeakRouteList")
		.endClass ()

	// std::list< boost::weak_ptr <AudioSource>
		.beginConstStdList <boost::weak_ptr<AudioSource> > ("WeakAudioSourceList")
		.endClass ()

#if 0  // depends on Evoal:: Note, Beats see note_fixer.h
	// typedef Evoral::Note<Evoral::Beats> Note;
	// std::set< boost::weak_ptr<Note> >
		.beginStdSet <boost::weak_ptr<Note> > ("WeakNoteSet")
		.endClass ()
#endif

	// typedef std::set<boost::weak_ptr<AudioPort> > PortSet
		.beginStdSet <boost::weak_ptr<AudioPort> > ("WeakPortSet")
		.endClass ()

	// std::list<boost::weak_ptr<Source> >
		.beginConstStdList <boost::weak_ptr<Source> > ("WeakSourceList")
		.endClass ()

		.beginClass <Tempo> ("Tempo")
		.addConstructor <void (*) (double, double)> ()
		.addFunction ("note_type", &Tempo::note_type)
		.addFunction ("beats_per_minute", &Tempo::beats_per_minute)
		.addFunction ("frames_per_beat", &Tempo::frames_per_beat)
		.endClass ()

		.beginClass <Meter> ("Meter")
		.addConstructor <void (*) (double, double)> ()
		.addFunction ("divisions_per_bar", &Meter::divisions_per_bar)
		.addFunction ("note_divisor", &Meter::note_divisor)
		.addFunction ("frames_per_bar", &Meter::frames_per_bar)
		.addFunction ("frames_per_grid", &Meter::frames_per_grid)
		.endClass ()

		.beginClass <TempoMap> ("TempoMap")
		.addFunction ("add_tempo", &TempoMap::add_tempo)
		.addFunction ("add_meter", &TempoMap::add_meter)
		.endClass ()

		.beginClass <ChanCount> ("ChanCount")
		.addFunction ("n_audio", &ChanCount::n_audio)
		.endClass()

		.beginClass <DataType> ("DataType")
		.addConstructor <void (*) (std::string)> ()
		.endClass()

		/* libardour enums */
		.beginNamespace ("Autostyle")
		.addConst ("Absolute", ARDOUR::AutoState(Absolute))
		.addConst ("Trim", ARDOUR::AutoState(Trim))
		.endNamespace ()

		.beginNamespace ("AutoState")
		.addConst ("Off", ARDOUR::AutoState(Off))
		.addConst ("Write", ARDOUR::AutoState(Write))
		.addConst ("Touch", ARDOUR::AutoState(Touch))
		.addConst ("Play", ARDOUR::AutoState(Play))
		.endNamespace ()

		.beginNamespace ("AutomationType")
		.addConst ("PluginAutomation", ARDOUR::AutomationType(PluginAutomation))
		.endNamespace ()

		.beginNamespace ("SrcQuality")
		.addConst ("SrcBest", ARDOUR::SrcQuality(SrcBest))
		.endNamespace ()

		.beginNamespace ("PlaylistDisposition")
		.addConst ("CopyPlaylist", ARDOUR::PlaylistDisposition(CopyPlaylist))
		.addConst ("NewPlaylist", ARDOUR::PlaylistDisposition(NewPlaylist))
		.addConst ("SharePlaylist", ARDOUR::PlaylistDisposition(SharePlaylist))
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginClass <AudioBackendInfo> ("AudioBackendInfo")
		.addData ("name", &AudioBackendInfo::name)
		.endClass()
		.beginStdVector <const AudioBackendInfo*> ("BackendVector").endClass ()

		.beginNamespace ("ARDOUR")
		.beginClass <AudioBackend::DeviceStatus> ("DeviceStatus")
		.addData ("name", &AudioBackend::DeviceStatus::name)
		.addData ("available", &AudioBackend::DeviceStatus::available)
		.endClass()
		.beginStdVector <AudioBackend::DeviceStatus> ("DeviceStatusVector").endClass ()
		.endNamespace ()

		.beginWSPtrClass <AudioBackend> ("AudioBackend")
		.addFunction ("info", &AudioBackend::info)
		.addFunction ("sample_rate", &AudioBackend::sample_rate)
		.addFunction ("buffer_size", &AudioBackend::buffer_size)
		.addFunction ("period_size", &AudioBackend::period_size)
		.addFunction ("input_channels", &AudioBackend::input_channels)
		.addFunction ("output_channels", &AudioBackend::output_channels)
		.addFunction ("dsp_load", &AudioBackend::dsp_load)

		.addFunction ("set_sample_rate", &AudioBackend::set_sample_rate)
		.addFunction ("set_buffer_size", &AudioBackend::set_buffer_size)
		.addFunction ("set_peridod_size", &AudioBackend::set_peridod_size)

		.addFunction ("enumerate_drivers", &AudioBackend::enumerate_drivers)
		.addFunction ("driver_name", &AudioBackend::driver_name)
		.addFunction ("set_driver", &AudioBackend::set_driver)

		.addFunction ("use_separate_input_and_output_devices", &AudioBackend::use_separate_input_and_output_devices)
		.addFunction ("enumerate_devices", &AudioBackend::enumerate_devices)
		.addFunction ("enumerate_input_devices", &AudioBackend::enumerate_input_devices)
		.addFunction ("enumerate_output_devices", &AudioBackend::enumerate_output_devices)
		.addFunction ("device_name", &AudioBackend::device_name)
		.addFunction ("input_device_name", &AudioBackend::input_device_name)
		.addFunction ("output_device_name", &AudioBackend::output_device_name)
		.addFunction ("set_device_name", &AudioBackend::set_device_name)
		.addFunction ("set_input_device_name", &AudioBackend::set_input_device_name)
		.addFunction ("set_output_device_name", &AudioBackend::set_output_device_name)
		.endClass()

		.beginClass <AudioEngine> ("AudioEngine")
		.addFunction ("available_backends", &AudioEngine::available_backends)
		.addFunction ("current_backend_name", &AudioEngine::current_backend_name)
		.addFunction ("set_backend", &AudioEngine::set_backend)
		.addFunction ("setup_required", &AudioEngine::setup_required)
		.addFunction ("start", &AudioEngine::start)
		.addFunction ("stop", &AudioEngine::stop)
		.addFunction ("get_dsp_load", &AudioEngine::get_dsp_load)
		.addFunction ("set_device_name", &AudioEngine::set_device_name)
		.addFunction ("set_sample_rate", &AudioEngine::set_sample_rate)
		.addFunction ("set_buffer_size", &AudioEngine::set_buffer_size)
		.addFunction ("get_last_backend_error", &AudioEngine::get_last_backend_error)
		.endClass()
		.endNamespace ();

	// basic representation of Session
	// functions which can be used from realtime and non-realtime contexts
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginClass <Session> ("Session")
		.addFunction ("scripts_changed", &Session::scripts_changed) // used internally
		.addFunction ("transport_rolling", &Session::transport_rolling)
		.addFunction ("request_transport_speed", &Session::request_transport_speed)
		.addFunction ("transport_frame", &Session::transport_frame)
		.addFunction ("transport_speed", &Session::transport_speed)
		.addFunction ("frame_rate", &Session::frame_rate)
		.addFunction ("nominal_frame_rate", &Session::nominal_frame_rate)
		.addFunction ("frames_per_timecode_frame", &Session::frames_per_timecode_frame)
		.addFunction ("timecode_frames_per_hour", &Session::timecode_frames_per_hour)
		.addFunction ("timecode_frames_per_second", &Session::timecode_frames_per_second)
		.addFunction ("timecode_drop_frames", &Session::timecode_drop_frames)
		.addFunction ("request_locate", &Session::request_locate)
		.addFunction ("request_stop", &Session::request_stop)
		.addFunction ("last_transport_start", &Session::last_transport_start)
		.addFunction ("goto_start", &Session::goto_start)
		.addFunction ("goto_end", &Session::goto_end)
		.addFunction ("current_start_frame", &Session::current_start_frame)
		.addFunction ("current_end_frame", &Session::current_end_frame)
		.addFunction ("actively_recording", &Session::actively_recording)
		.addFunction ("get_routes", &Session::get_routes)
		.addFunction ("get_tracks", &Session::get_tracks)
		.addFunction ("name", &Session::name)
		.addFunction ("path", &Session::path)
		.addFunction ("record_status", &Session::record_status)
		.addFunction ("route_by_id", &Session::route_by_id)
		.addFunction ("route_by_name", &Session::route_by_name)
		.addFunction ("route_by_remote_id", &Session::route_by_remote_id)
		.addFunction ("track_by_diskstream_id", &Session::track_by_diskstream_id)
		.addFunction ("source_by_id", &Session::source_by_id)
		.addFunction ("controllable_by_id", &Session::controllable_by_id)
		.addFunction ("processor_by_id", &Session::processor_by_id)
		.addFunction ("snap_name", &Session::snap_name)
		.addFunction ("tempo_map", (TempoMap& (Session::*)())&Session::tempo_map)
		.endClass ()

		.beginClass <RegionFactory> ("RegionFactory")
		.addStaticFunction ("region_by_id", &RegionFactory::region_by_id)
		.endClass ()

		/* session enums */
		.beginNamespace ("Session")

		.beginNamespace ("RecordState")
		.addConst ("Disabled", ARDOUR::Session::RecordState(Session::Disabled))
		.addConst ("Enabled", ARDOUR::Session::RecordState(Session::Enabled))
		.addConst ("Recording", ARDOUR::Session::RecordState(Session::Recording))
		.endNamespace ()

		.endNamespace () // END Session enums

		.beginNamespace ("LuaAPI")
		.addFunction ("new_luaproc", ARDOUR::LuaAPI::new_luaproc)
		.endNamespace ()

		.endNamespace ();// END ARDOUR
}

void
LuaBindings::dsp (lua_State* L)
{
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")

		.beginClass <AudioBuffer> ("AudioBuffer")
		.addFunction ("data", (Sample*(AudioBuffer::*)(framecnt_t))&AudioBuffer::data)
		.addFunction ("silence", &AudioBuffer::silence)
		.addFunction ("apply_gain", &AudioBuffer::apply_gain)
		.endClass()

		.beginClass <MidiBuffer> ("MidiBuffer")
		.addFunction ("silence", &MidiBuffer::silence)
		.addFunction ("empty", &MidiBuffer::empty)
		// TODO iterators..
		.endClass()

		.beginClass <BufferSet> ("BufferSet")
		.addFunction ("get_audio", static_cast<AudioBuffer&(BufferSet::*)(size_t)>(&BufferSet::get_audio))
		.addFunction ("count", static_cast<const ChanCount&(BufferSet::*)()const>(&BufferSet::count))
		.endClass()

		.beginClass <ChanMapping> ("ChanMapping")
		.addFunction ("get", static_cast<uint32_t(ChanMapping::*)(DataType, uint32_t)>(&ChanMapping::get))
		.addFunction ("set", &ChanMapping::set)
		.addConst ("Invalid", 4294967295) // UINT32_MAX
		.endClass ()
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Evoral")
		.beginClass <Evoral::Event<framepos_t> > ("Event")
		.addFunction ("clear", &Evoral::Event<framepos_t>::clear)
		.addFunction ("size", &Evoral::Event<framepos_t>::size)
		.addFunction ("set_buffer", &Evoral::Event<framepos_t>::set_buffer)
		.addFunction ("buffer", (uint8_t*(Evoral::Event<framepos_t>::*)())&Evoral::Event<framepos_t>::buffer)
		.endClass ()

		.deriveClass <Evoral::MIDIEvent<framepos_t>, Evoral::Event<framepos_t> > ("MidiEvent")
		// add Ctor?
		.addFunction ("type", &Evoral::MIDIEvent<framepos_t>::type)
		.addFunction ("channel", &Evoral::MIDIEvent<framepos_t>::channel)
		.addFunction ("set_type", &Evoral::MIDIEvent<framepos_t>::type)
		.addFunction ("set_channel", &Evoral::MIDIEvent<framepos_t>::channel)
		.endClass ()
		.endNamespace ();

	// dsp releated session functions
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginClass <Session> ("Session")
		.addFunction ("get_scratch_buffers", &Session::get_scratch_buffers)
		.addFunction ("get_silent_buffers", &Session::get_silent_buffers)
		.endClass ()
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginNamespace ("DSP")
		.addFunction ("compute_peak", ARDOUR::compute_peak)
		.addFunction ("find_peaks", ARDOUR::find_peaks)
		.addFunction ("apply_gain_to_buffer", ARDOUR::apply_gain_to_buffer)
		.addFunction ("mix_buffers_no_gain", ARDOUR::mix_buffers_no_gain)
		.addFunction ("mix_buffers_with_gain", ARDOUR::mix_buffers_with_gain)
		.addFunction ("copy_vector", ARDOUR::copy_vector)
		.addFunction ("dB_to_coefficient", &dB_to_coefficient)
		.addFunction ("fast_coefficient_to_dB", &fast_coefficient_to_dB)
		.addFunction ("accurate_coefficient_to_dB", &accurate_coefficient_to_dB)
		.addFunction ("memset", &DSP::memset)
		.addFunction ("mmult", &DSP::mmult)
		.addFunction ("log_meter", &DSP::log_meter)
		.addFunction ("log_meter_coeff", &DSP::log_meter_coeff)
		.addRefFunction ("peaks", &DSP::peaks)

		.beginClass <DSP::LowPass> ("LowPass")
		.addConstructor <void (*) (double, float)> ()
		.addFunction ("proc", &DSP::LowPass::proc)
		.addFunction ("ctrl", &DSP::LowPass::ctrl)
		.addFunction ("set_cutoff", &DSP::LowPass::set_cutoff)
		.addFunction ("reset", &DSP::LowPass::reset)
		.endClass ()
		.beginClass <DSP::BiQuad> ("Biquad")
		.addConstructor <void (*) (double)> ()
		.addFunction ("run", &DSP::BiQuad::run)
		.addFunction ("compute", &DSP::BiQuad::compute)
		.addFunction ("reset", &DSP::BiQuad::reset)
		.endClass ()

		.beginClass <DSP::DspShm> ("DspShm")
		.addFunction ("allocate", &DSP::DspShm::allocate)
		.addFunction ("clear", &DSP::DspShm::clear)
		.addFunction ("to_float", &DSP::DspShm::to_float)
		.addFunction ("to_int", &DSP::DspShm::to_int)
		.addFunction ("atomic_set_int", &DSP::DspShm::atomic_set_int)
		.addFunction ("atomic_get_int", &DSP::DspShm::atomic_get_int)
		.endClass ()

		.endNamespace () // DSP
		.endNamespace (); // ARDOUR
}

void
LuaBindings::session (lua_State* L)
{
	// non-realtime session functions
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginClass <Session> ("Session")
		.addFunction ("save_state", &Session::save_state)
		.addFunction ("set_dirty", &Session::set_dirty)
		.addFunction ("unknown_processors", &Session::unknown_processors)

		.addFunction<RouteList (Session::*)(uint32_t, const std::string&, const std::string&, PlaylistDisposition)> ("new_route_from_template", &Session::new_route_from_template)
		// TODO  session_add_audio_track  session_add_midi_track  session_add_mixed_track
		//.addFunction ("new_midi_track", &Session::new_midi_track)
		.endClass ()

		.endNamespace (); // ARDOUR
}

void
LuaBindings::osc (lua_State* L)
{
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("OSC")
		.beginClass<LuaAPI::LuaOSCAddress> ("Address")
		.addConstructor<void (*) (std::string)> ()
		.addCFunction ("send", &LuaAPI::LuaOSCAddress::send)
		.endClass ()
		.endNamespace ();
}

void
LuaBindings::set_session (lua_State* L, Session *s)
{
	/* LuaBridge uses unique keys to identify classes/c-types.
	 *
	 * Those keys are "generated" by using the memory-address of a static
	 * variable, templated for every Class.
	 * (see libs/lua/LuaBridge/detail/ClassInfo.h)
	 *
	 * When linking the final executable there must be exactly one static
	 * function (static variable) for every templated class.
	 * This works fine on OSX and Linux...
	 *
	 * Windows (mingw and MSVC) however expand the template differently for libardour
	 * AND gtk2_ardour. We end up with two identical static functions
	 * at different addresses!!
	 *
	 * The Solution: have gtk2_ardour never include LuaBridge headers directly
	 * and always go via libardour function calls for classes that are registered
	 * in libardour. (calling lua itself is fine,  calling c-functions in the GUI
	 * which expand the template is not)
	 *
	 * (the actual cause: even static symbols in a .dll have no fixed address
	 * and are mapped when loading the dll. static functions in .exe do have a fixed
	 * address)
	 *
	 * libardour:
	 *  0000000000000000 I __imp__ZZN9luabridge9ClassInfoIN6ARDOUR7SessionEE11getClassKeyEvE5value
	 *  0000000000000000 I __nm__ZZN9luabridge9ClassInfoIN6ARDOUR7SessionEE11getClassKeyEvE5value
	 *  0000000000000000 T _ZN9luabridge9ClassInfoIN6ARDOUR7SessionEE11getClassKeyEv
	 *
	 * ardour.exe
	 *  000000000104f560 d .data$_ZZN9luabridge9ClassInfoIN6ARDOUR7SessionEE11getClassKeyEvE5value
	 *  000000000104f560 D _ZZN9luabridge9ClassInfoIN6ARDOUR7SessionEE11getClassKeyEvE5value
	 *  0000000000e9baf0 T _ZN9luabridge9ClassInfoIN6ARDOUR7SessionEE11getClassKeyEv
	 *
	 *
	 */
	luabridge::push <Session *> (L, s);
	lua_setglobal (L, "Session");

	if (s) {
		// call lua function.
		luabridge::LuaRef cb_ses = luabridge::getGlobal (L, "new_session");
		if (cb_ses.type() == LUA_TFUNCTION) { cb_ses(s->name()); } // TODO args
	}
}
