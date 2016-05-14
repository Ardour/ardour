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

#include <glibmm.h>

#include "timecode/bbt_time.h"
#include "pbd/stateful_diff_command.h"
#include "evoral/Control.hpp"
#include "evoral/ControlList.hpp"
#include "evoral/Range.hpp"

#include "ardour/amp.h"
#include "ardour/audioengine.h"
#include "ardour/audiosource.h"
#include "ardour/audio_backend.h"
#include "ardour/audio_buffer.h"
#include "ardour/audio_port.h"
#include "ardour/audio_track.h"
#include "ardour/buffer_set.h"
#include "ardour/chan_mapping.h"
#include "ardour/dB.h"
#include "ardour/dsp_filter.h"
#include "ardour/interthread_info.h"
#include "ardour/lua_api.h"
#include "ardour/luabindings.h"
#include "ardour/luaproc.h"
#include "ardour/meter.h"
#include "ardour/midi_track.h"
#include "ardour/midi_port.h"
#include "ardour/playlist.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/port_manager.h"
#include "ardour/runtime_functions.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/session_object.h"
#include "ardour/sidechain.h"
#include "ardour/track.h"
#include "ardour/tempo.h"

#include "LuaBridge/LuaBridge.h"

#ifdef PLATFORM_WINDOWS
/* luabridge uses addresses of static functions/variables to identify classes.
 *
 * Static symbols on windows (even identical symbols) are not
 * mapped to the same address when mixing .dll + .exe.
 * So we need a single point to define those static functions.
 * (normally they're header-only in libs/lua/LuaBridge/detail/ClassInfo.h)
 *
 * Really!! A static function with a static variable in a library header
 * should never ever be replicated, even if it is a template.
 * But then again this is windows... what else can go wrong.
 */

template <class T>
void const*
luabridge::ClassInfo<T>::getStaticKey ()
{
	static char value;
	return &value;
}

template <class T>
void const*
luabridge::ClassInfo<T>::getClassKey ()
{
	static char value;
	return &value;
}

template <class T>
void const*
luabridge::ClassInfo<T>::getConstKey ()
{
	static char value;
	return &value;
}

void*
luabridge::getIdentityKey ()
{
  static char value;
  return &value;
}

/* ...and this is the ugly part of it.
 *
 * We need to foward declare classes from gtk2_ardour
 * end explicily list classes which are used by gtk2_ardour's bindings.
 *
 * This is needed because some of the GUI classes use objects from libardour
 * as function parameters and the .exe would re-create symbols for libardour
 * objects.
 *
 * Classes which don't use libardour symbols could be moved to
 * gtk2_ardour/luainstance.cc, but keeping this here reduces code
 * duplication and does not give the compiler a chance to even think
 * about replicating the symbols.
 */

#define CLASSKEYS(CLS) \
	template void const* luabridge::ClassInfo< CLS >::getStaticKey(); \
	template void const* luabridge::ClassInfo< CLS >::getClassKey();  \
	template void const* luabridge::ClassInfo< CLS >::getConstKey();

#define CLASSINFO(CLS) \
	class CLS; \
	template void const* luabridge::ClassInfo< CLS >::getStaticKey(); \
	template void const* luabridge::ClassInfo< CLS >::getClassKey();  \
	template void const* luabridge::ClassInfo< CLS >::getConstKey();

CLASSINFO(MarkerSelection);
CLASSINFO(TrackSelection);
CLASSINFO(TrackViewList);
CLASSINFO(TimeSelection);
CLASSINFO(RegionSelection);
CLASSINFO(PublicEditor);
CLASSINFO(Selection);
CLASSINFO(ArdourMarker);

namespace Cairo {
	class Context;
}
CLASSKEYS(Cairo::Context);
CLASSKEYS(std::vector<double>);
CLASSKEYS(std::list<ArdourMarker*>);
CLASSKEYS(std::bitset<47ul>); // LuaSignal::LAST_SIGNAL
CLASSKEYS(ArdourMarker*);
CLASSKEYS(ARDOUR::RouteGroup);
CLASSKEYS(ARDOUR::LuaProc);
CLASSKEYS(ARDOUR::DataType);
CLASSKEYS(ARDOUR::ChanCount);
CLASSKEYS(boost::shared_ptr<ARDOUR::Processor>);
CLASSKEYS(ARDOUR::ParameterDescriptor);
CLASSKEYS(boost::shared_ptr<ARDOUR::AutomationList>);
CLASSKEYS(boost::shared_ptr<Evoral::ControlList>);
CLASSKEYS(ARDOUR::LuaOSC::Address);
CLASSKEYS(ARDOUR::Session);
CLASSKEYS(ARDOUR::BufferSet);
CLASSKEYS(ARDOUR::ChanMapping);
CLASSKEYS(ARDOUR::DSP::DspShm);
CLASSKEYS(PBD::ID);
CLASSKEYS(ARDOUR::Location);
CLASSKEYS(ARDOUR::PluginInfo);
CLASSKEYS(PBD::PropertyChange);
CLASSKEYS(std::vector<std::string>);
CLASSKEYS(std::list<boost::shared_ptr<ARDOUR::Route> >);
CLASSKEYS(std::list<boost::shared_ptr<ARDOUR::Port> >);
CLASSKEYS(boost::shared_ptr<ARDOUR::PluginInfo>);
CLASSKEYS(boost::shared_ptr<ARDOUR::Region>);
CLASSKEYS(boost::weak_ptr<ARDOUR::Route>);
CLASSKEYS(std::list<boost::shared_ptr<ARDOUR::Region> >);
CLASSKEYS(std::list<ARDOUR::AudioRange>);
CLASSKEYS(Evoral::Beats);
CLASSKEYS(ARDOUR::PortManager);
CLASSKEYS(ARDOUR::AudioEngine);
CLASSKEYS(void);
CLASSKEYS(float);

#endif // end windows special case

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
		.beginNamespace ("C")
		.beginStdList <std::string> ("StringList")
		.endClass ()

	// std::vector<std::string>
		.beginStdVector <std::string> ("StringVector")
		.endClass ()

	// register float array (float*)
		.registerArray <float> ("FloatArray")

	// register float array (int32_t*)
		.registerArray <int32_t> ("IntArray")

	// TODO std::set
		.endNamespace ();
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

		.beginClass <XMLNode> ("XMLNode")
		.addFunction ("name", &XMLNode::name)
		.endClass ()

		.beginClass <PBD::Stateful> ("Stateful")
		.addFunction ("id", &PBD::Stateful::id)
		.addFunction ("properties", &PBD::Stateful::properties)
		.addFunction ("clear_changes", &PBD::Stateful::clear_changes)
		.endClass ()

		.beginWSPtrClass <PBD::Stateful> ("StatefulPtr")
		.addFunction ("id", &PBD::Stateful::id)
		.addFunction ("properties", &PBD::Stateful::properties)
		.addFunction ("clear_changes", &PBD::Stateful::clear_changes)
		.endClass ()

		.deriveClass <PBD::StatefulDestructible, PBD::Stateful> ("StatefulDestructible")
		.endClass ()

		.deriveWSPtrClass <PBD::StatefulDestructible, PBD::Stateful> ("StatefulDestructiblePtr")
		.endClass ()

		.deriveClass <Command, PBD::StatefulDestructible> ("Command")
		.addFunction ("set_name", &Command::set_name)
		.addFunction ("name", &Command::name)
		.endClass ()

		/* UndoTransaction::add_command() subscribes to DropReferences()
		 * and deletes the object.
		 *
		 * This object cannot be constructed by lua because lua would manage lifetime
		 * and delete the object leading to a double free.
		 *
		 * use Session::add_stateful_diff_command()
		 * and Session::abort_reversible_command()
		 */
		.deriveClass <PBD::StatefulDiffCommand, Command> ("StatefulDiffCommand")
		.addFunction ("undo", &PBD::StatefulDiffCommand::undo)
		.addFunction ("empty", &PBD::StatefulDiffCommand::empty)
		.endClass ()

		.deriveWSPtrClass <PBD::Controllable, PBD::StatefulDestructible> ("Controllable")
		.addFunction ("name", &PBD::Controllable::name)
		.addFunction ("get_value", &PBD::Controllable::get_value)
		.endClass ()

		/* PBD enums */
		.beginNamespace ("GroupControlDisposition")
		.addConst ("InverseGroup", PBD::Controllable::GroupControlDisposition(PBD::Controllable::InverseGroup))
		.addConst ("NoGroup", PBD::Controllable::GroupControlDisposition(PBD::Controllable::NoGroup))
		.addConst ("UseGroup", PBD::Controllable::GroupControlDisposition(PBD::Controllable::UseGroup))
		.endNamespace ()

		.endNamespace (); // PBD

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Timecode")
		.beginClass <Timecode::BBT_Time> ("BBT_TIME")
		.addConstructor <void (*) (uint32_t, uint32_t, uint32_t)> ()
		.endClass ()
		.endNamespace ();

	luabridge::getGlobalNamespace (L)

		.beginNamespace ("Evoral")
		.beginClass <Evoral::Parameter> ("Parameter")
		.addConstructor <void (*) (uint32_t, uint8_t, uint32_t)> ()
		.addFunction ("type", &Evoral::Parameter::type)
		.addFunction ("channel", &Evoral::Parameter::channel)
		.addFunction ("id", &Evoral::Parameter::id)
		.endClass ()

		.beginWSPtrClass <Evoral::ControlList> ("ControlList")
		.addFunction ("add", &Evoral::ControlList::add)
		.addFunction ("thin", &Evoral::ControlList::thin)
		.addFunction ("eval", &Evoral::ControlList::eval)
		.addRefFunction ("rt_safe_eval", &Evoral::ControlList::rt_safe_eval)
		.addFunction ("interpolation", &Evoral::ControlList::interpolation)
		.addFunction ("set_interpolation", &Evoral::ControlList::set_interpolation)
		.addFunction ("truncate_end", &Evoral::ControlList::truncate_end)
		.addFunction ("truncate_start", &Evoral::ControlList::truncate_start)
		.addFunction ("clear", (void (Evoral::ControlList::*)(double, double))&Evoral::ControlList::clear)
		.addFunction ("in_write_pass", &Evoral::ControlList::in_write_pass)
		.endClass ()

		.beginWSPtrClass <Evoral::ControlSet> ("ControlSet")
		.endClass ()

		.beginWSPtrClass <Evoral::Control> ("Control")
		.addFunction ("list", (boost::shared_ptr<Evoral::ControlList>(Evoral::Control::*)())&Evoral::Control::list)
		.endClass ()

		.beginClass <Evoral::ParameterDescriptor> ("ParameterDescriptor")
		.addVoidConstructor ()
		.addData ("lower", &Evoral::ParameterDescriptor::lower)
		.addData ("upper", &Evoral::ParameterDescriptor::upper)
		.addData ("normal", &Evoral::ParameterDescriptor::normal)
		.addData ("toggled", &Evoral::ParameterDescriptor::toggled)
		.endClass ()

		.beginClass <Evoral::Range<framepos_t> > ("Range")
		.addConstructor <void (*) (framepos_t, framepos_t)> ()
		.addData ("from", &Evoral::Range<framepos_t>::from)
		.addData ("to", &Evoral::Range<framepos_t>::to)
		.endClass ()

		/* libevoral enums */
		.beginNamespace ("InterpolationStyle")
		.addConst ("Discrete", Evoral::ControlList::InterpolationStyle(Evoral::ControlList::Discrete))
		.addConst ("Linear", Evoral::ControlList::InterpolationStyle(Evoral::ControlList::Linear))
		.addConst ("Curved", Evoral::ControlList::InterpolationStyle(Evoral::ControlList::Curved))
		.endNamespace ()

		.endNamespace () // Evoral

		.beginNamespace ("ARDOUR")

		.beginClass <InterThreadInfo> ("InterThreadInfo")
		.addVoidConstructor ()
		.addData ("done", const_cast<bool InterThreadInfo::*>(&InterThreadInfo::done))
#if 0 // currently unused, lua is single-threaded, no custom UIs.
		.addData ("cancel", (bool InterThreadInfo::*)&InterThreadInfo::cancel)
#endif
		.addData ("progress", const_cast<float InterThreadInfo::*>(&InterThreadInfo::progress))
		.endClass ()

		.beginClass <AudioRange> ("AudioRange")
		.addConstructor <void (*) (framepos_t, framepos_t, uint32_t)> ()
		.addFunction ("length", &AudioRange::length)
		.addFunction ("equal", &AudioRange::equal)
		.addData ("start", &AudioRange::start)
		.addData ("_end", &AudioRange::end) // XXX "end" is a lua reserved word
		.addData ("id", &AudioRange::id)
		.endClass ()

		.beginWSPtrClass <PluginInfo> ("PluginInfo")
		.addVoidConstructor ()
		.endClass ()

		.beginNamespace ("Route")
		.beginClass <Route::ProcessorStreams> ("ProcessorStreams")
		.addVoidConstructor ()
		.endClass ()
		.endNamespace ()

		.beginClass <ChanMapping> ("ChanMapping")
		.addVoidConstructor ()
		.addFunction ("get", static_cast<uint32_t(ChanMapping::*)(DataType, uint32_t) const>(&ChanMapping::get))
		.addFunction ("set", &ChanMapping::set)
		.addConst ("Invalid", 4294967295U) // UINT32_MAX
		.endClass ()

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

		.beginWSPtrClass <AutomationList> ("AutomationList")
		.addCast<PBD::Stateful> ("to_stateful")
		.addCast<PBD::StatefulDestructible> ("to_statefuldestructible")
		.addCast<Evoral::ControlList> ("list")
		.addFunction ("get_state", &AutomationList::get_state)
		.addFunction ("memento_command", &AutomationList::memento_command)
		.addFunction ("touching", &AutomationList::touching)
		.addFunction ("writing", &AutomationList::writing)
		.addFunction ("touch_enabled", &AutomationList::touch_enabled)
		.endClass ()

		.deriveClass <Location, PBD::StatefulDestructible> ("Location")
		.addFunction ("locked", &Location::locked)
		.addFunction ("lock", &Location::lock)
		.addFunction ("start", &Location::start)
		.addFunction ("_end", &Location::end) // XXX "end" is a lua reserved word
		.addFunction ("length", &Location::length)
		.addFunction ("set_start", &Location::set_start)
		.addFunction ("set_end", &Location::set_end)
		.addFunction ("set_length", &Location::set)
		.addFunction ("move_to", &Location::move_to)
		.endClass ()

		.deriveClass <Locations, PBD::StatefulDestructible> ("Locations")
		.addFunction ("auto_loop_location", &Locations::auto_loop_location)
		.addFunction ("auto_punch_location", &Locations::auto_punch_location)
		.addFunction ("session_range_location", &Locations::session_range_location)
		.addFunction ("first_mark_after", &Locations::first_mark_after)
		.addFunction ("first_mark_before", &Locations::first_mark_before)
		.addFunction ("first_mark_at", &Locations::mark_at)
		.addRefFunction ("marks_either_side", &Locations::marks_either_side)
		.addRefFunction ("find_all_between", &Locations::find_all_between)
		.endClass ()

		.beginWSPtrClass <SessionObject> ("SessionObject")
		/* SessionObject is-a PBD::StatefulDestructible,
		 * but multiple inheritance is not covered by luabridge,
		 * we need explicit casts */
		.addCast<PBD::Stateful> ("to_stateful")
		.addCast<PBD::StatefulDestructible> ("to_statefuldestructible")
		.addFunction ("name", &SessionObject::name)
		.endClass ()

		.beginWSPtrClass <Port> ("Port")
		.addFunction ("name", &Port::name)
		.addFunction ("pretty_name", &Port::pretty_name)
		.addFunction ("receives_input", &Port::receives_input)
		.addFunction ("sends_output", &Port::sends_output)
		.addFunction ("connected", &Port::connected)
		.addFunction ("disconnect_all", &Port::disconnect_all)
		.addFunction ("connected_to_name", (bool (Port::*)(std::string const &)const)&Port::connected_to)
		.addFunction ("connect_by_name", (int (Port::*)(std::string const &))&Port::connect)
		.addFunction ("disconnect_by_name", (int (Port::*)(std::string const &))&Port::disconnect)
		.addFunction ("connected_to", (bool (Port::*)(Port*)const)&Port::connected_to)
		.addFunction ("connect", (int (Port::*)(Port*))&Port::connect)
		.addFunction ("disconnect", (int (Port::*)(Port*))&Port::disconnect)
		.endClass ()

		.deriveWSPtrClass <AudioPort, Port> ("AudioPort")
		.endClass ()

		.deriveWSPtrClass <MidiPort, Port> ("MidiPort")
		.addFunction ("input_active", &MidiPort::input_active)
		.addFunction ("set_input_active", &MidiPort::set_input_active)
		.endClass ()

		.beginWSPtrClass <PortSet> ("PortSet")
		.addFunction ("num_ports", (size_t (PortSet::*)(DataType)const)&PortSet::num_ports)
		.addFunction ("add", &PortSet::add)
		.addFunction ("remove", &PortSet::remove)
		.addFunction ("port", (boost::shared_ptr<Port> (PortSet::*)(DataType, size_t)const)&PortSet::port)
		.addFunction ("contains", &PortSet::contains)
		.addFunction ("clear", &PortSet::clear)
		.addFunction ("empty", &PortSet::empty)
		.endClass ()

		.deriveWSPtrClass <IO, SessionObject> ("IO")
		.addFunction ("active", &IO::active)
		.addFunction ("add_port", &IO::add_port)
		.addFunction ("remove_port", &IO::remove_port)
		.addFunction ("connect", &IO::connect)
		.addFunction ("disconnect", (int (IO::*)(boost::shared_ptr<Port>, std::string, void *))&IO::disconnect)
		.addFunction ("physically_connected", &IO::physically_connected)
		.addFunction ("has_port", &IO::has_port)
		.addFunction ("nth", &IO::nth)
		.addFunction ("audio", &IO::audio)
		.addFunction ("midi", &IO::midi)
		.addFunction ("port_by_name", &IO::nth)
		.addFunction ("n_ports", &IO::n_ports)
		.endClass ()

		.beginClass <RouteGroup> ("RouteGroup")
		// stub RouteGroup* is needed for new_audio_track()
		.endClass ()

		.deriveWSPtrClass <Route, SessionObject> ("Route")
		.addCast<Track> ("to_track")
		.addFunction ("set_name", &Route::set_name)
		.addFunction ("comment", &Route::comment)
		.addFunction ("active", &Route::active)
		.addFunction ("set_active", &Route::set_active)
		.addFunction ("nth_plugin", &Route::nth_plugin)
		.addFunction ("add_processor_by_index", &Route::add_processor_by_index)
		.addFunction ("remove_processor", &Route::remove_processor)
		.addFunction ("replace_processor", &Route::replace_processor)
		.addFunction ("n_inputs", &Route::n_inputs)
		.addFunction ("n_outputs", &Route::n_outputs)
		.addFunction ("set_comment", &Route::set_comment)
		.addFunction ("strict_io", &Route::strict_io)
		.addFunction ("set_strict_io", &Route::set_strict_io)
		.addFunction ("reset_plugin_insert", &Route::reset_plugin_insert)
		.addFunction ("customize_plugin_insert", &Route::customize_plugin_insert)
		.addFunction ("add_sidechain", &Route::add_sidechain)
		.addFunction ("remove_sidechain", &Route::remove_sidechain)
		.addFunction ("main_outs", &Route::main_outs)
		.addFunction ("muted", &Route::muted)
		.addFunction ("soloed", &Route::soloed)
		.addFunction ("amp", &Route::amp)
		.addFunction ("trim", &Route::trim)
		.endClass ()

		.deriveWSPtrClass <Playlist, SessionObject> ("Playlist")
		.addFunction ("region_by_id", &Playlist::region_by_id)
		.addFunction ("data_type", &Playlist::data_type)
		.addFunction ("n_regions", &Playlist::n_regions)
		//.addFunction ("get_extent", &Playlist::get_extent) // pair<framepos_t, framepos_t>
		//.addFunction ("region_list", &Playlist::region_list) // RegionListProperty&
		.addFunction ("add_region", &Playlist::add_region)
		.addFunction ("remove_region", &Playlist::remove_region)
		.addFunction ("regions_at", &Playlist::regions_at)
		.addFunction ("top_region_at", &Playlist::top_region_at)
		.addFunction ("top_unmuted_region_at", &Playlist::top_unmuted_region_at)
		.addFunction ("find_next_region", &Playlist::find_next_region)
		.addFunction ("find_next_region_boundary", &Playlist::find_next_region_boundary)
		.addFunction ("count_regions_at", &Playlist::count_regions_at)
		.addFunction ("regions_touched", &Playlist::regions_touched)
		.addFunction ("regions_with_start_within", &Playlist::regions_with_start_within)
		.addFunction ("regions_with_end_within", &Playlist::regions_with_end_within)
		.addFunction ("raise_region", &Playlist::raise_region)
		.addFunction ("lower_region", &Playlist::lower_region)
		.addFunction ("raise_region_to_top", &Playlist::raise_region_to_top)
		.addFunction ("lower_region_to_bottom", &Playlist::lower_region_to_bottom)
		.addFunction ("duplicate", (void (Playlist::*)(boost::shared_ptr<Region>, framepos_t, framecnt_t, float))&Playlist::duplicate)
		.addFunction ("duplicate_until", &Playlist::duplicate_until)
		.addFunction ("duplicate_range", &Playlist::duplicate_range)
		.addFunction ("combine", &Playlist::combine)
		.addFunction ("uncombine", &Playlist::uncombine)
		.addFunction ("split_region", &Playlist::split_region)
		.addFunction ("split", (void (Playlist::*)(framepos_t))&Playlist::split)
		.addFunction ("cut", (boost::shared_ptr<Playlist> (Playlist::*)(std::list<AudioRange>&, bool))&Playlist::cut)
#if 0
		.addFunction ("copy", &Playlist::copy)
		.addFunction ("paste", &Playlist::paste)
#endif
		.endClass ()

		.deriveWSPtrClass <Track, Route> ("Track")
		.addCast<AudioTrack> ("to_audio_track")
		.addCast<MidiTrack> ("to_midi_track")
		.addFunction ("set_name", &Track::set_name)
		.addFunction ("can_record", &Track::can_record)
		.addFunction ("record_enabled", &Track::record_enabled)
		.addFunction ("record_safe", &Track::record_safe)
		.addFunction ("set_record_enabled", &Track::set_record_enabled)
		.addFunction ("set_record_safe", &Track::set_record_safe)
		.addFunction ("bounceable", &Track::bounceable)
		.addFunction ("bounce", &Track::bounce)
		.addFunction ("bounce_range", &Track::bounce_range)
		.addFunction ("playlist", &Track::playlist)
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
		.addRefFunction ("sync_offset", &Region::sync_offset)
		.addFunction ("sync_position", &Region::sync_position)
		.addFunction ("hidden", &Region::hidden)
		.addFunction ("muted", &Region::muted)
		.addFunction ("opaque", &Region::opaque)
		.addFunction ("locked", &Region::locked)
		.addFunction ("position_locked", &Region::position_locked)
		.addFunction ("video_locked", &Region::video_locked)
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

		.beginClass <Plugin::PresetRecord> ("PresetRecord")
		.addVoidConstructor ()
		.addData ("uri", &Plugin::PresetRecord::uri, false)
		.addData ("label", &Plugin::PresetRecord::label, false)
		.addData ("user", &Plugin::PresetRecord::user, false)
		.addData ("valid", &Plugin::PresetRecord::valid, false)
		.endClass ()

		.deriveWSPtrClass <Automatable, Evoral::ControlSet> ("Automatable")
		.addFunction ("automation_control", (boost::shared_ptr<AutomationControl>(Automatable::*)(const Evoral::Parameter&, bool))&Automatable::automation_control)
		.endClass ()

		.deriveClass <ParameterDescriptor, Evoral::ParameterDescriptor> ("ParameterDescriptor")
		.addVoidConstructor ()
		.addData ("label", &ParameterDescriptor::label)
		.addData ("logarithmic", &ParameterDescriptor::logarithmic)
		.endClass ()

		.deriveWSPtrClass <Processor, SessionObject> ("Processor")
		.addCast<Automatable> ("to_automatable")
		.addCast<PluginInsert> ("to_insert")
		.addCast<SideChain> ("to_sidechain")
		.addCast<IOProcessor> ("to_ioprocessor")
		.addFunction ("display_name", &Processor::display_name)
		.addFunction ("active", &Processor::active)
		.addFunction ("activate", &Processor::activate)
		.addFunction ("deactivate", &Processor::deactivate)
		.addFunction ("control", (boost::shared_ptr<Evoral::Control>(Evoral::ControlSet::*)(const Evoral::Parameter&, bool))&Evoral::ControlSet::control)
		.addFunction ("automation_control", (boost::shared_ptr<AutomationControl>(Automatable::*)(const Evoral::Parameter&, bool))&Automatable::automation_control)
		.endClass ()

		.deriveWSPtrClass <IOProcessor, Processor> ("IOProcessor")
		.addFunction ("natural_input_streams", &IOProcessor::natural_input_streams)
		.addFunction ("natural_output_streams", &IOProcessor::natural_output_streams)
		.addFunction ("input", (boost::shared_ptr<IO>(IOProcessor::*)())&IOProcessor::input)
		.addFunction ("output", (boost::shared_ptr<IO>(IOProcessor::*)())&IOProcessor::output)
		.endClass ()

		.deriveWSPtrClass <SideChain, IOProcessor> ("SideChain")
		.endClass ()

		.deriveWSPtrClass <Delivery, IOProcessor> ("Delivery")
		.endClass ()

		.deriveWSPtrClass <Plugin, PBD::StatefulDestructible> ("Plugin")
		.addFunction ("label", &Plugin::label)
		.addFunction ("name", &Plugin::name)
		.addFunction ("maker", &Plugin::maker)
		.addFunction ("parameter_count", &Plugin::parameter_count)
		.addRefFunction ("nth_parameter", &Plugin::nth_parameter)
		.addFunction ("preset_by_label", &Plugin::preset_by_label)
		.addFunction ("preset_by_uri", &Plugin::preset_by_uri)
		.addFunction ("load_preset", &Plugin::load_preset)
		.addFunction ("parameter_is_input", &Plugin::parameter_is_input)
		.addFunction ("get_docs", &Plugin::get_docs)
		.addFunction ("get_parameter_docs", &Plugin::get_parameter_docs)
		.addRefFunction ("get_parameter_descriptor", &Plugin::get_parameter_descriptor)
		.endClass ()

		.deriveWSPtrClass <PluginInsert, Processor> ("PluginInsert")
		.addFunction ("plugin", &PluginInsert::plugin)
		.addFunction ("activate", &PluginInsert::activate)
		.addFunction ("deactivate", &PluginInsert::deactivate)
		.addFunction ("strict_io_configured", &PluginInsert::strict_io_configured)
		.addFunction ("input_map", (ARDOUR::ChanMapping (PluginInsert::*)(uint32_t) const)&PluginInsert::input_map)
		.addFunction ("output_map", (ARDOUR::ChanMapping (PluginInsert::*)(uint32_t) const)&PluginInsert::output_map)
		.addFunction ("set_input_map", &PluginInsert::set_input_map)
		.addFunction ("set_output_map", &PluginInsert::set_output_map)
		.addFunction ("sidechain_input", &PluginInsert::sidechain_input)
		.endClass ()

		.deriveWSPtrClass <AutomationControl, PBD::Controllable> ("AutomationControl")
		.addCast<Evoral::Control> ("to_ctrl")
		.addFunction ("automation_state", &AutomationControl::automation_state)
		.addFunction ("automation_style", &AutomationControl::automation_style)
		.addFunction ("set_automation_state", &AutomationControl::set_automation_state)
		.addFunction ("set_automation_style", &AutomationControl::set_automation_style)
		.addFunction ("start_touch", &AutomationControl::start_touch)
		.addFunction ("stop_touch", &AutomationControl::stop_touch)
		.addFunction ("get_value", &AutomationControl::get_value)
		.addFunction ("set_value", &AutomationControl::set_value)
		.addFunction ("writable", &AutomationControl::writable)
		.addFunction ("alist", &AutomationControl::alist)
		.endClass ()

		.deriveWSPtrClass <GainControl, AutomationControl> ("GainControl")
		.endClass ()

		.deriveWSPtrClass <Amp, Processor> ("Amp")
		.addFunction ("gain_control", (boost::shared_ptr<GainControl>(Amp::*)())&Amp::gain_control)
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

		// RouteList == std::list<boost::shared_ptr<Route> >
		.beginConstStdList <boost::shared_ptr<Route> > ("RouteList")
		.endClass ()

		// boost::shared_ptr<RouteList>
		.beginPtrStdList <boost::shared_ptr<Route> > ("RouteListPtr")
		.endClass ()

		// typedef std::list<boost::weak_ptr <Route> > WeakRouteList
		.beginConstStdList <boost::weak_ptr<Route> > ("WeakRouteList")
		.endClass ()

		// std::list< boost::weak_ptr <AudioSource> >
		.beginConstStdList <boost::weak_ptr<AudioSource> > ("WeakAudioSourceList")
		.endClass ()

		// typedef std::list<boost::shared_ptr<Region> > RegionList
		.beginConstStdList <boost::shared_ptr<Region> > ("RegionList")
		.endClass ()

		// boost::shared_ptr <std::list<boost::shared_ptr<Region> > >
		.beginPtrStdList <boost::shared_ptr<Region> > ("RegionListPtr")
		.endClass ()

		//std::list<boost::shared_ptr<Port> > PortList;
		.beginConstStdList <boost::shared_ptr<Port> > ("PortList")
		.endClass ()

		// used by Playlist::cut/copy
		.beginConstStdList <AudioRange> ("AudioRangeList")
		.endClass ()

		.beginConstStdList <Location*> ("LocationList")
		.endClass ()

#if 0  // depends on Evoal:: Note, Beats see note_fixer.h
	// typedef Evoral::Note<Evoral::Beats> Note;
	// std::set< boost::weak_ptr<Note> >
		.beginStdSet <boost::weak_ptr<Note> > ("WeakNoteSet")
		.endClass ()
#endif

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
		.addConstructor <void (*) (DataType, uint32_t)> ()
		.addFunction ("get", &ChanCount::get)
		.addFunction ("set", &ChanCount::set)
		.addFunction ("n_audio", &ChanCount::n_audio)
		.addFunction ("n_midi", &ChanCount::n_midi)
		.addFunction ("n_total", &ChanCount::n_total)
		.addFunction ("reset", &ChanCount::reset)
		.endClass()

		.beginClass <DataType> ("DataType")
		.addConstructor <void (*) (std::string)> ()
		.addStaticCFunction ("null",  &LuaAPI::datatype_ctor_null) // "nil" is a lua reseved word
		.addStaticCFunction ("audio", &LuaAPI::datatype_ctor_audio)
		.addStaticCFunction ("midi",  &LuaAPI::datatype_ctor_midi)
		.addFunction ("to_string",  &DataType::to_string) // TODO Lua __tostring
		// TODO add uint32_t cast, add operator==  !=
		.endClass()

		/* libardour enums */
		.beginNamespace ("PluginType")
		.addConst ("AudioUnit", ARDOUR::PluginType(AudioUnit))
		.addConst ("LADSPA", ARDOUR::PluginType(LADSPA))
		.addConst ("LV2", ARDOUR::PluginType(LV2))
		.addConst ("Windows_VST", ARDOUR::PluginType(Windows_VST))
		.addConst ("LXVST", ARDOUR::PluginType(LXVST))
		.addConst ("Lua", ARDOUR::PluginType(Lua))
		.endNamespace ()

		.beginNamespace ("AutoStyle")
		.addConst ("Absolute", ARDOUR::AutoStyle(Absolute))
		.addConst ("Trim", ARDOUR::AutoStyle(Trim))
		.endNamespace ()

		.beginNamespace ("AutoState")
		.addConst ("Off", ARDOUR::AutoState(Off))
		.addConst ("Write", ARDOUR::AutoState(Write))
		.addConst ("Touch", ARDOUR::AutoState(Touch))
		.addConst ("Play", ARDOUR::AutoState(Play))
		.endNamespace ()

		.beginNamespace ("AutomationType")
		.addConst ("PluginAutomation", ARDOUR::AutomationType(PluginAutomation))
		.addConst ("PluginAutomation", ARDOUR::AutomationType(GainAutomation))
		.addConst ("PluginAutomation", ARDOUR::AutomationType(TrimAutomation))
		.endNamespace ()

		.beginNamespace ("SrcQuality")
		.addConst ("SrcBest", ARDOUR::SrcQuality(SrcBest))
		.endNamespace ()

		.beginNamespace ("PortFlags")
		.addConst ("IsInput", ARDOUR::PortFlags(IsInput))
		.addConst ("IsOutput", ARDOUR::PortFlags(IsOutput))
		.addConst ("IsPhysical", ARDOUR::PortFlags(IsPhysical))
		.addConst ("CanMonitor", ARDOUR::PortFlags(CanMonitor))
		.addConst ("IsTerminal", ARDOUR::PortFlags(IsTerminal))
		.endNamespace ()

		.beginNamespace ("PlaylistDisposition")
		.addConst ("CopyPlaylist", ARDOUR::PlaylistDisposition(CopyPlaylist))
		.addConst ("NewPlaylist", ARDOUR::PlaylistDisposition(NewPlaylist))
		.addConst ("SharePlaylist", ARDOUR::PlaylistDisposition(SharePlaylist))
		.endNamespace ()

		.beginNamespace ("RegionPoint")
		.addConst ("Start", ARDOUR::RegionPoint(Start))
		.addConst ("End", ARDOUR::RegionPoint(End))
		.addConst ("SyncPoint", ARDOUR::RegionPoint(SyncPoint))
		.endNamespace ()

		.beginNamespace ("TrackMode")
		.addConst ("Normal", ARDOUR::TrackMode(Start))
		.addConst ("NonLayered", ARDOUR::TrackMode(NonLayered))
		.addConst ("Destructive", ARDOUR::TrackMode(Destructive))
		.endNamespace ()
		.endNamespace (); // end ARDOUR

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginClass <AudioBackendInfo> ("AudioBackendInfo")
		.addData ("name", &AudioBackendInfo::name)
		.endClass()
		.beginStdVector <const AudioBackendInfo*> ("BackendVector").endClass ()

		.beginClass <AudioBackend::DeviceStatus> ("DeviceStatus")
		.addData ("name", &AudioBackend::DeviceStatus::name)
		.addData ("available", &AudioBackend::DeviceStatus::available)
		.endClass()
		.beginStdVector <AudioBackend::DeviceStatus> ("DeviceStatusVector").endClass ()

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

		.beginClass <PortManager> ("PortManager")
		.addFunction ("port_engine", &PortManager::port_engine)
		.addFunction ("connected", &PortManager::connected)
		.addFunction ("connect", &PortManager::connect)
		.addFunction ("physically_connected", &PortManager::physically_connected)
		.addFunction ("disconnect", (int (PortManager::*)(const std::string&, const std::string&))&PortManager::disconnect)
		.addFunction ("disconnect_port", (int (PortManager::*)(boost::shared_ptr<Port>))&PortManager::disconnect)
		.addFunction ("get_port_by_name", &PortManager::get_port_by_name)
		.addFunction ("get_pretty_name_by_name", &PortManager::get_pretty_name_by_name)
		.addFunction ("port_is_physical", &PortManager::port_is_physical)
		.addFunction ("get_physical_outputs", &PortManager::get_physical_outputs)
		.addFunction ("get_physical_inputs", &PortManager::get_physical_inputs)
		.addFunction ("n_physical_outputs", &PortManager::n_physical_outputs)
		.addFunction ("n_physical_inputs", &PortManager::n_physical_inputs)
		.addRefFunction ("get_connections", &PortManager::get_connections)
		.addRefFunction ("get_ports", (int (PortManager::*)(DataType, PortManager::PortList&))&PortManager::get_ports)
		.addRefFunction ("get_backend_ports", (int (PortManager::*)(const std::string&, DataType, PortFlags, std::vector<std::string>&))&PortManager::get_ports)
		.endClass()

		.deriveClass <AudioEngine, PortManager> ("AudioEngine")
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
		.addFunction ("new_audio_track", &Session::new_audio_track)
		.addFunction ("new_audio_route", &Session::new_audio_route)
		.addFunction ("new_midi_track", &Session::new_midi_track)
		.addFunction ("new_midi_route", &Session::new_midi_route)
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
		.addFunction ("monitor_out", &Session::monitor_out)
		.addFunction ("master_out", &Session::master_out)
		.addFunction ("tempo_map", (TempoMap& (Session::*)())&Session::tempo_map)
		.addFunction ("locations", &Session::locations)
		.addFunction ("begin_reversible_command", (void (Session::*)(const std::string&))&Session::begin_reversible_command)
		.addFunction ("commit_reversible_command", &Session::commit_reversible_command)
		.addFunction ("abort_reversible_command", &Session::abort_reversible_command)
		.addFunction ("add_command", &Session::add_command)
		.addFunction ("add_stateful_diff_command", &Session::add_stateful_diff_command)
		.addFunction ("engine", (AudioEngine& (Session::*)())&Session::engine)
		.addFunction ("get_block_size", &Session::get_block_size)
		.addFunction ("worst_output_latency", &Session::worst_output_latency)
		.addFunction ("worst_input_latency", &Session::worst_input_latency)
		.addFunction ("worst_track_latency", &Session::worst_track_latency)
		.addFunction ("worst_playback_latency", &Session::worst_playback_latency)
		.endClass ()

		.beginClass <RegionFactory> ("RegionFactory")
		.addStaticFunction ("region_by_id", &RegionFactory::region_by_id)
		.endClass ()

		/* session enums (rt-safe, common) */
		.beginNamespace ("Session")

		.beginNamespace ("RecordState")
		.addConst ("Disabled", ARDOUR::Session::RecordState(Session::Disabled))
		.addConst ("Enabled", ARDOUR::Session::RecordState(Session::Enabled))
		.addConst ("Recording", ARDOUR::Session::RecordState(Session::Recording))
		.endNamespace ()

		.endNamespace () // end Session enums

		/* ardour enums (rt-safe, common) */
		.beginNamespace ("LocationFlags")
		.addConst ("IsMark", ARDOUR::Location::Flags(Location::IsMark))
		.addConst ("IsAutoPunch", ARDOUR::Location::Flags(Location::IsAutoPunch))
		.addConst ("IsAutoLoop", ARDOUR::Location::Flags(Location::IsAutoLoop))
		.addConst ("IsHidden", ARDOUR::Location::Flags(Location::IsHidden))
		.addConst ("IsCDMarker", ARDOUR::Location::Flags(Location::IsCDMarker))
		.addConst ("IsRangeMarker", ARDOUR::Location::Flags(Location::IsRangeMarker))
		.addConst ("IsSessionRange", ARDOUR::Location::Flags(Location::IsSessionRange))
		.addConst ("IsSkip", ARDOUR::Location::Flags(Location::IsSkip))
		.addConst ("IsSkipping", ARDOUR::Location::Flags(Location::IsSkipping))
		.endNamespace ()

		.beginNamespace ("LuaAPI")
		.addFunction ("nil_proc", ARDOUR::LuaAPI::nil_processor)
		.addFunction ("new_luaproc", ARDOUR::LuaAPI::new_luaproc)
		.addFunction ("new_plugin_info", ARDOUR::LuaAPI::new_plugin_info)
		.addFunction ("new_plugin", ARDOUR::LuaAPI::new_plugin)
		.addFunction ("set_processor_param", ARDOUR::LuaAPI::set_processor_param)
		.addFunction ("set_plugin_insert_param", ARDOUR::LuaAPI::set_plugin_insert_param)
		.addCFunction ("plugin_automation", ARDOUR::LuaAPI::plugin_automation)
		.addFunction ("usleep", Glib::usleep)
		.endNamespace () // end LuaAPI
		.endNamespace ();// end ARDOUR
}

void
LuaBindings::dsp (lua_State* L)
{
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")

		.beginClass <AudioBuffer> ("AudioBuffer")
		.addEqualCheck ()
		.addFunction ("data", (Sample*(AudioBuffer::*)(framecnt_t))&AudioBuffer::data)
		.addFunction ("silence", &AudioBuffer::silence)
		.addFunction ("apply_gain", &AudioBuffer::apply_gain)
		.addFunction ("check_silence", &AudioBuffer::check_silence)
		.addFunction ("read_from", (void (AudioBuffer::*)(const Sample*, framecnt_t, framecnt_t, framecnt_t))&AudioBuffer::check_silence)
		.endClass()

		.beginClass <MidiBuffer> ("MidiBuffer")
		.addEqualCheck ()
		.addFunction ("silence", &MidiBuffer::silence)
		.addFunction ("empty", &MidiBuffer::empty)
		// TODO iterators..
		.endClass()

		.beginClass <BufferSet> ("BufferSet")
		.addEqualCheck ()
		.addFunction ("get_audio", static_cast<AudioBuffer&(BufferSet::*)(size_t)>(&BufferSet::get_audio))
		.addFunction ("count", static_cast<const ChanCount&(BufferSet::*)()const>(&BufferSet::count))
		.endClass()
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Evoral")
		.beginClass <Evoral::Event<framepos_t> > ("Event")
		.addFunction ("clear", &Evoral::Event<framepos_t>::clear)
		.addFunction ("size", &Evoral::Event<framepos_t>::size)
		.addFunction ("set_buffer", &Evoral::Event<framepos_t>::set_buffer)
		.addFunction ("buffer", (uint8_t*(Evoral::Event<framepos_t>::*)())&Evoral::Event<framepos_t>::buffer)
		.endClass ()

		.beginClass <Evoral::Beats> ("Beats")
		.addFunction ("to_double", &Evoral::Beats::to_double)
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

		/* DSP enums */
		.beginNamespace ("BiQuadType")
		.addConst ("LowPass", ARDOUR::DSP::BiQuad::LowPass)
		.addConst ("HighPass", ARDOUR::DSP::BiQuad::HighPass)
		.addConst ("BandPassSkirt", ARDOUR::DSP::BiQuad::BandPassSkirt)
		.addConst ("BandPass0dB", ARDOUR::DSP::BiQuad::BandPass0dB)
		.addConst ("Notch", ARDOUR::DSP::BiQuad::Notch)
		.addConst ("AllPass", ARDOUR::DSP::BiQuad::AllPass)
		.addConst ("Peaking", ARDOUR::DSP::BiQuad::Peaking)
		.addConst ("LowShelf", ARDOUR::DSP::BiQuad::LowShelf)
		.addConst ("HighShelf", ARDOUR::DSP::BiQuad::HighShelf)
		.endNamespace ()

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
		.beginNamespace ("ARDOUR")
		.beginNamespace ("LuaOSC")
		.beginClass<LuaOSC::Address> ("Address")
		.addConstructor<void (*) (std::string)> ()
		.addCFunction ("send", &LuaOSC::Address::send)
		.endClass ()
		.endNamespace ()
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
