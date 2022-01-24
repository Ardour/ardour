/*
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2019 Johannes Mueller <github@johannes-mueller.org>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017-2018 Ben Loftis <ben@harrisonconsoles.com>
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

#include <glibmm.h>

#include "pbd/stateful_diff_command.h"
#include "pbd/openuri.h"

#include "temporal/bbt_time.h"
#include "temporal/range.h"

#include "evoral/Control.h"
#include "evoral/ControlList.h"

#include "ardour/amp.h"
#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/audio_backend.h"
#include "ardour/audio_buffer.h"
#include "ardour/audio_port.h"
#include "ardour/audio_track.h"
#include "ardour/audioplaylist.h"
#include "ardour/audiorom.h"
#include "ardour/buffer_set.h"
#include "ardour/bundle.h"
#include "ardour/chan_mapping.h"
#include "ardour/convolver.h"
#include "ardour/dB.h"
#include "ardour/delayline.h"
#include "ardour/disk_reader.h"
#include "ardour/disk_writer.h"
#include "ardour/dsp_filter.h"
#include "ardour/file_source.h"
#include "ardour/filesystem_paths.h"
#include "ardour/fluid_synth.h"
#include "ardour/internal_send.h"
#include "ardour/internal_return.h"
#include "ardour/interthread_info.h"
#include "ardour/ltc_file_reader.h"
#include "ardour/lua_api.h"
#include "ardour/luabindings.h"
#include "ardour/luaproc.h"
#include "ardour/meter.h"
#include "ardour/midi_model.h"
#include "ardour/midi_track.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_port.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/monitor_control.h"
#include "ardour/panner_shell.h"
#include "ardour/phase_control.h"
#include "ardour/playlist.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/plugin_manager.h"
#include "ardour/polarity_processor.h"
#include "ardour/port_manager.h"
#include "ardour/progress.h"
#include "ardour/raw_midi_parser.h"
#include "ardour/runtime_functions.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/return.h"
#include "ardour/revision.h"
#include "ardour/route_group.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/session_object.h"
#include "ardour/session_playlists.h"
#include "ardour/sidechain.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/solo_safe_control.h"
#include "ardour/stripable.h"
#include "ardour/track.h"
#include "ardour/tempo.h"
#include "ardour/user_bundle.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "LuaBridge/LuaBridge.h"

/* lambda function to use for Lua metatable methods.
 * A generic C++ template won't work here because
 * operators cannot be used as template parameters.
 */
#define CPPOPERATOR2(RTYPE, TYPE1, TYPE2, OP)                         \
  [] (lua_State* L) {                                                 \
    TYPE1* const t0 = luabridge::Userdata::get <TYPE1> (L, 1, false); \
    TYPE2* const t1 = luabridge::Userdata::get <TYPE2> (L, 2, false); \
    luabridge::Stack <RTYPE>::push (L, (*t0 OP *t1));                 \
    return 1;                                                         \
  }

#define CPPCOMPERATOR(TYPE, OP) CPPOPERATOR2 (bool, TYPE, TYPE, OP)
#define CPPOPERATOR(TYPE, OP) CPPOPERATOR2 (TYPE, TYPE, TYPE, OP)


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
 * But then again this is windows... what else can go wrong ?!
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
 * AND explicily list classes which are used by gtk2_ardour's bindings.
 *
 * This is required because some of the GUI classes use objects from libardour
 * as function parameters or return values and the .exe would re-create
 * symbols for libardour objects.
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

CLASSINFO(ArdourMarker);
CLASSINFO(AxisView);
CLASSINFO(MarkerSelection);
CLASSINFO(PublicEditor);
CLASSINFO(RegionSelection);
CLASSINFO(RegionView);
CLASSINFO(StripableTimeAxisView);
CLASSINFO(RouteTimeAxisView);
CLASSINFO(RouteUI);
CLASSINFO(Selectable);
CLASSINFO(Selection);
CLASSINFO(TimeAxisView);
CLASSINFO(TimeAxisViewItem);
CLASSINFO(TimeSelection);
CLASSINFO(TrackSelection);
CLASSINFO(TrackViewList);
CLASSINFO(UIConfiguration);


/* this needs to match gtk2_ardour/luasignal.h */
CLASSKEYS(std::bitset<49ul>); // LuaSignal::LAST_SIGNAL

CLASSKEYS(void);
CLASSKEYS(float);
CLASSKEYS(double);
CLASSKEYS(unsigned char);

CLASSKEYS(ArdourMarker*);
CLASSKEYS(Selectable*);
CLASSKEYS(std::list<Selectable*>);

CLASSKEYS(ARDOUR::AudioEngine);
CLASSKEYS(ARDOUR::BufferSet);
CLASSKEYS(ARDOUR::ChanCount);
CLASSKEYS(ARDOUR::ChanMapping);
CLASSKEYS(ARDOUR::DSP::DspShm);
CLASSKEYS(ARDOUR::DataType);
CLASSKEYS(ARDOUR::FluidSynth);
CLASSKEYS(ARDOUR::InternalSend);
CLASSKEYS(ARDOUR::Latent);
CLASSKEYS(ARDOUR::Location);
CLASSKEYS(ARDOUR::LuaAPI::Vamp);
CLASSKEYS(ARDOUR::LuaOSC::Address);
CLASSKEYS(ARDOUR::LuaProc);
CLASSKEYS(ARDOUR::LuaTableRef);
CLASSKEYS(ARDOUR::MidiModel::NoteDiffCommand);
CLASSKEYS(ARDOUR::MonitorProcessor);
CLASSKEYS(ARDOUR::RouteGroup);
CLASSKEYS(ARDOUR::ParameterDescriptor);
CLASSKEYS(ARDOUR::PeakMeter);
CLASSKEYS(ARDOUR::PluginInfo);
CLASSKEYS(ARDOUR::Plugin::PresetRecord);
CLASSKEYS(ARDOUR::PortEngine);
CLASSKEYS(ARDOUR::PortManager);
CLASSKEYS(ARDOUR::PresentationInfo);
CLASSKEYS(ARDOUR::RCConfiguration);
CLASSKEYS(ARDOUR::Session);
CLASSKEYS(ARDOUR::SessionConfiguration);
CLASSKEYS(ARDOUR::Slavable);
CLASSKEYS(ARDOUR::Source);
CLASSKEYS(ARDOUR::VCA);
CLASSKEYS(ARDOUR::VCAManager);

CLASSKEYS(Temporal::timepos_t)
CLASSKEYS(Temporal::timecnt_t)
CLASSKEYS(Temporal::superclock_t)

CLASSKEYS(PBD::ID);
CLASSKEYS(PBD::Configuration);
CLASSKEYS(PBD::PropertyChange);
CLASSKEYS(PBD::StatefulDestructible);

CLASSKEYS(Temporal::Beats);
CLASSKEYS(Evoral::Event<samplepos_t>);
CLASSKEYS(Evoral::ControlEvent);


CLASSKEYS(std::vector<std::string>);
CLASSKEYS(std::vector<uint8_t>);
CLASSKEYS(std::vector<float>);
CLASSKEYS(std::vector<float*>);
CLASSKEYS(std::vector<double>);
CLASSKEYS(std::list<int64_t>);
CLASSKEYS(std::vector<samplepos_t>);

CLASSKEYS(std::list<Evoral::ControlEvent*>);

CLASSKEYS(std::vector<ARDOUR::Plugin::PresetRecord>);
CLASSKEYS(std::vector<boost::shared_ptr<ARDOUR::Processor> >);
CLASSKEYS(std::vector<boost::shared_ptr<ARDOUR::Source> >);
CLASSKEYS(std::vector<boost::shared_ptr<ARDOUR::AudioReadable> >);
CLASSKEYS(std::vector<Evoral::Parameter>);
CLASSKEYS(std::list<boost::shared_ptr<ARDOUR::PluginInfo> >); // PluginInfoList

CLASSKEYS(std::list<ArdourMarker*>);
CLASSKEYS(std::list<TimeAxisView*>);
CLASSKEYS(std::list<ARDOUR::TimelineRange>);

CLASSKEYS(std::list<boost::shared_ptr<ARDOUR::Port> >);
CLASSKEYS(std::list<boost::shared_ptr<ARDOUR::Region> >);
CLASSKEYS(std::list<boost::shared_ptr<ARDOUR::Route> >);
CLASSKEYS(std::list<boost::shared_ptr<ARDOUR::Stripable> >);
CLASSKEYS(boost::shared_ptr<std::list<boost::shared_ptr<ARDOUR::Route> > >);
CLASSKEYS(boost::shared_ptr<std::vector<boost::shared_ptr<ARDOUR::Bundle> > >);

CLASSKEYS(boost::shared_ptr<ARDOUR::AudioRegion>);
CLASSKEYS(boost::shared_ptr<ARDOUR::AudioRom>);
CLASSKEYS(boost::shared_ptr<ARDOUR::AudioSource>);
CLASSKEYS(boost::shared_ptr<ARDOUR::Automatable>);
CLASSKEYS(boost::shared_ptr<ARDOUR::AutomatableSequence<Temporal::Beats> >);
CLASSKEYS(boost::shared_ptr<ARDOUR::AutomationList>);
CLASSKEYS(boost::shared_ptr<ARDOUR::FileSource>);
CLASSKEYS(boost::shared_ptr<ARDOUR::MidiModel>);
CLASSKEYS(boost::shared_ptr<ARDOUR::MidiPlaylist>);
CLASSKEYS(boost::shared_ptr<ARDOUR::MidiRegion>);
CLASSKEYS(boost::shared_ptr<ARDOUR::MidiSource>);
CLASSKEYS(boost::shared_ptr<ARDOUR::PluginInfo>);
CLASSKEYS(boost::shared_ptr<ARDOUR::Processor>);
CLASSKEYS(boost::shared_ptr<ARDOUR::AudioReadable>);
CLASSKEYS(boost::shared_ptr<ARDOUR::Region>);
CLASSKEYS(boost::shared_ptr<ARDOUR::SessionPlaylists>);
CLASSKEYS(boost::shared_ptr<Evoral::ControlList>);
CLASSKEYS(boost::shared_ptr<Evoral::Note<Temporal::Beats> >);
CLASSKEYS(boost::shared_ptr<Evoral::Sequence<Temporal::Beats> >);

CLASSKEYS(boost::shared_ptr<ARDOUR::Playlist>);
CLASSKEYS(boost::shared_ptr<ARDOUR::Bundle>);
CLASSKEYS(boost::shared_ptr<ARDOUR::Route>);
CLASSKEYS(boost::shared_ptr<ARDOUR::VCA>);
CLASSKEYS(boost::weak_ptr<ARDOUR::Bundle>);
CLASSKEYS(boost::weak_ptr<ARDOUR::Route>);
CLASSKEYS(boost::weak_ptr<ARDOUR::VCA>);

CLASSKEYS(boost::shared_ptr<ARDOUR::RegionList>);

CLASSKEYS(Vamp::RealTime);
CLASSKEYS(Vamp::PluginBase);
CLASSKEYS(Vamp::PluginBase::ParameterDescriptor);
CLASSKEYS(Vamp::Plugin);
CLASSKEYS(Vamp::Plugin::OutputDescriptor);
CLASSKEYS(Vamp::Plugin::Feature);
CLASSKEYS(Vamp::Plugin::OutputList);
CLASSKEYS(Vamp::Plugin::FeatureList);
CLASSKEYS(Vamp::Plugin::FeatureSet);

namespace LuaCairo {
	class ImageSurface;
	class PangoLayout;
}

namespace LuaDialog {
	class Message;
	class Dialog;
	class ProgressWindow;
}

namespace Cairo {
	class Context;
}

CLASSKEYS(Cairo::Context);
CLASSKEYS(LuaCairo::ImageSurface);
CLASSKEYS(LuaCairo::PangoLayout);

CLASSKEYS(LuaDialog::Message);
CLASSKEYS(LuaDialog::Dialog);
CLASSKEYS(LuaDialog::ProgressWindow);

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

/** Access libardour global configuration */
static RCConfiguration* _libardour_config () {
	return ARDOUR::Config;
}

void
LuaBindings::stddef (lua_State* L)
{
	// std::list<std::string>
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("C")
		.beginStdList <std::string> ("StringList")
		.endClass ()

		.beginStdVector <std::string> ("StringVector")
		.endClass ()

		.beginStdVector <float> ("FloatVector")
		.endClass ()

		.beginStdVector <uint8_t> ("ByteVector")
		.endClass ()

		.beginStdVector <float*> ("FloatArrayVector")
		.endClass ()

		.registerArray <uint8_t> ("ByteArray")
		.registerArray <float> ("FloatArray")
		.registerArray <int32_t> ("IntArray")

		// samplepos_t, sampleoffset_t lists e.g. AnalysisFeatureList
		.beginStdList <int64_t> ("Int64List")
		.endClass ()

		// TODO std::set
		.endNamespace ();
}

void
LuaBindings::common (lua_State* L)
{
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("PBD")

		.addFunction ("open_uri", (bool (*) (const std::string&))&PBD::open_uri)
		.addFunction ("open_uri", &PBD::open_folder)

		.beginClass <PBD::ID> ("ID")
		.addConstructor <void (*) (std::string)> ()
		.addFunction ("to_s", &PBD::ID::to_s) // TODO special case LUA __tostring ?
		.endClass ()

		.beginStdVector <PBD::ID> ("IdVector").endClass ()

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

		.deriveClass <PBD::Configuration, PBD::Stateful> ("Configuration")
		.endClass()

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

		.beginClass <PBD::RingBufferNPT <uint8_t> > ("RingBuffer8")
		.addConstructor <void (*) (size_t)> ()
		.addFunction ("reset", &PBD::RingBufferNPT<uint8_t>::reset)
		.addFunction ("read", &PBD::RingBufferNPT<uint8_t>::read)
		.addFunction ("write", &PBD::RingBufferNPT<uint8_t>::write)
		.addFunction ("write_one", &PBD::RingBufferNPT<uint8_t>::write_one)
		.addFunction ("write_space", &PBD::RingBufferNPT<uint8_t>::write_space)
		.addFunction ("read_space", &PBD::RingBufferNPT<uint8_t>::read_space)
		.addFunction ("increment_read_ptr", &PBD::RingBufferNPT<uint8_t>::increment_read_ptr)
		.addFunction ("increment_write_ptr", &PBD::RingBufferNPT<uint8_t>::increment_write_ptr)
		.endClass ()

		.beginClass <PBD::RingBufferNPT <float> > ("RingBufferF")
		.addConstructor <void (*) (size_t)> ()
		.addFunction ("reset", &PBD::RingBufferNPT<float>::reset)
		.addFunction ("read", &PBD::RingBufferNPT<float>::read)
		.addFunction ("write", &PBD::RingBufferNPT<float>::write)
		.addFunction ("write_one", &PBD::RingBufferNPT<float>::write_one)
		.addFunction ("write_space", &PBD::RingBufferNPT<float>::write_space)
		.addFunction ("read_space", &PBD::RingBufferNPT<float>::read_space)
		.addFunction ("increment_read_ptr", &PBD::RingBufferNPT<float>::increment_read_ptr)
		.addFunction ("increment_write_ptr", &PBD::RingBufferNPT<float>::increment_write_ptr)
		.endClass ()

		.beginClass <PBD::RingBufferNPT <int> > ("RingBufferI")
		.addConstructor <void (*) (size_t)> ()
		.addFunction ("reset", &PBD::RingBufferNPT<int>::reset)
		.addFunction ("read", &PBD::RingBufferNPT<int>::read)
		.addFunction ("write", &PBD::RingBufferNPT<int>::write)
		.addFunction ("write_one", &PBD::RingBufferNPT<int>::write_one)
		.addFunction ("write_space", &PBD::RingBufferNPT<int>::write_space)
		.addFunction ("read_space", &PBD::RingBufferNPT<int>::read_space)
		.addFunction ("increment_read_ptr", &PBD::RingBufferNPT<int>::increment_read_ptr)
		.addFunction ("increment_write_ptr", &PBD::RingBufferNPT<int>::increment_write_ptr)
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

		.beginClass <Timecode::Time> ("Time")
		.addConstructor <void (*) (double)> ()
		.addData ("negative", &Timecode::Time::negative)
		.addData ("hours", &Timecode::Time::hours)
		.addData ("minutes", &Timecode::Time::minutes)
		.addData ("seconds", &Timecode::Time::seconds)
		.addData ("frames", &Timecode::Time::frames)
		.addData ("subframes", &Timecode::Time::subframes)
		.addData ("rate", &Timecode::Time::rate)
		.addData ("drop", &Timecode::Time::drop)
		.endClass ()

		// TODO add increment, decrement; push it into the class

		/* libtimecode enums */
		.beginNamespace ("TimecodeFormat")
		.addConst ("TC23976", Timecode::TimecodeFormat(Timecode::timecode_23976))
		.addConst ("TC24", Timecode::TimecodeFormat(Timecode::timecode_24))
		.addConst ("TC24976", Timecode::TimecodeFormat(Timecode::timecode_24976))
		.addConst ("TC25", Timecode::TimecodeFormat(Timecode::timecode_25))
		.addConst ("TC2997", Timecode::TimecodeFormat(Timecode::timecode_2997))
		.addConst ("TC2997DF", Timecode::TimecodeFormat(Timecode::timecode_2997drop))
		.addConst ("TC2997000", Timecode::TimecodeFormat(Timecode::timecode_2997000))
		.addConst ("TC2997000DF", Timecode::TimecodeFormat(Timecode::timecode_2997000drop))
		.addConst ("TC30", Timecode::TimecodeFormat(Timecode::timecode_30))
		.addConst ("TC5994", Timecode::TimecodeFormat(Timecode::timecode_5994))
		.addConst ("TC60", Timecode::TimecodeFormat(Timecode::timecode_60))
		.endNamespace ()

		.endNamespace (); /* Timecode */

	luabridge::getGlobalNamespace (L)

		.beginNamespace ("Temporal")

		.addConst ("superclock_ticks_per_second", Temporal::superclock_ticks_per_second)
		.addConst ("ticks_per_beat", Temporal::ticks_per_beat)

		.beginClass <Temporal::ratio_t> ("ratio")
		.addConstructor <void (*) (int64_t, int64_t)> ()
		.addFunction ("is_unity", &Temporal::ratio_t::is_unity)
		.addFunction ("is_zero", &Temporal::ratio_t::is_zero)
		.endClass ()

		.beginClass <Temporal::Beats> ("Beats")
		.addConstructor <void (*) (int32_t, int32_t)> ()
		.addStaticFunction ("from_double", &Temporal::Beats::from_double)
		.addStaticFunction ("beats", &Temporal::Beats::beats)
		.endClass ()

		/* TODO */
		// * superclock_to_samples
		// * samples_to_superclock
		// add wrappers to construct timepos_t from samples

		.beginClass <Temporal::timepos_t> ("timepos_t")
		.addConstructor <void (*) (Temporal::samplepos_t)> ()
		.addOperator ("__add", CPPOPERATOR(Temporal::timepos_t, +))
		//.addOperator ("__mod", CPPOPERATOR2(Temporal::timepos_t, Temporal::timepos_t, Temporal::timecnt_t, %))
		.addOperator ("__mul", CPPOPERATOR2(Temporal::timepos_t, Temporal::timepos_t, Temporal::ratio_t , *))
		.addOperator ("__div", CPPOPERATOR2(Temporal::timepos_t, Temporal::timepos_t, Temporal::ratio_t , /))
		.addOperator ("__lt", CPPCOMPERATOR(Temporal::timepos_t, <))
		.addOperator ("__le", CPPCOMPERATOR(Temporal::timepos_t, <=))
		.addOperator ("__eq", CPPCOMPERATOR(Temporal::timepos_t, ==))
		.addStaticFunction ("zero", &Temporal::timepos_t::zero)
		.addStaticFunction ("from_superclock", &Temporal::timepos_t::from_superclock)
		.addStaticFunction ("from_ticks", &Temporal::timepos_t::from_ticks)
		.addFunction ("is_positive", &Temporal::timepos_t::is_positive)
		.addFunction ("is_negative", &Temporal::timepos_t::is_negative)
		.addFunction ("is_zero", &Temporal::timepos_t::is_zero)
		.addFunction ("is_beats", &Temporal::timepos_t::is_beats)
		.addFunction ("is_superclock", &Temporal::timepos_t::is_superclock)
		.addFunction ("superclocks", &Temporal::timepos_t::superclocks)
		.addFunction ("samples", &Temporal::timepos_t::samples)
		.addFunction ("ticks", &Temporal::timepos_t::ticks)
		.addFunction ("beats", &Temporal::timepos_t::beats)
		.addFunction ("str", &Temporal::timepos_t::str)
		.addMetamethod ("__tostring", &Temporal::timepos_t::str)
		.endClass ()

		.beginClass <timecnt_t> ("timecnt_t")
		.addConstructor <void (*) (Temporal::samplepos_t)> ()
		.addOperator ("__add", CPPOPERATOR(Temporal::timecnt_t, +))
		.addOperator ("__sub", CPPOPERATOR(Temporal::timecnt_t, -))
		.addOperator ("__mod", CPPOPERATOR(Temporal::timecnt_t, %))
		.addOperator ("__mul", CPPOPERATOR2(Temporal::timecnt_t, Temporal::timecnt_t, Temporal::ratio_t , *))
		.addOperator ("__div", CPPOPERATOR2(Temporal::timecnt_t, Temporal::timecnt_t, Temporal::ratio_t , /))
		.addOperator ("__lt", CPPCOMPERATOR(Temporal::timecnt_t, <))
		.addOperator ("__le", CPPCOMPERATOR(Temporal::timecnt_t, <=))
		.addOperator ("__eq", CPPCOMPERATOR(Temporal::timecnt_t, ==))
#if 0 // TODO these methods are, despite the static_cast<>, ambiguous
		.addStaticFunction ("zero", &Temporal::timecnt_t::zero)
		.addStaticFunction ("from_superclock", static_cast<Temporal::timecnt_t(Temporal::timecnt_t::*)(Temporal::superclock_t)>(&Temporal::timecnt_t::from_superclock))
		.addStaticFunction ("from_samples", static_cast<Temporal::timecnt_t(Temporal::timecnt_t::*)(Temporal::samplepos_t)>(&Temporal::timecnt_t::from_samples))
		.addStaticFunction ("from_ticks", static_cast<Temporal::timecnt_t(Temporal::timecnt_t::*)(int64_t)>(&Temporal::timecnt_t::from_ticks))
#endif
		.addFunction ("magnitude", &Temporal::timecnt_t::magnitude)
		.addFunction ("position", &Temporal::timecnt_t::position)
		.addFunction ("origin", &Temporal::timecnt_t::origin)
		.addFunction ("set_position", &Temporal::timecnt_t::set_position)
		.addFunction ("is_positive", &Temporal::timecnt_t::is_positive)
		.addFunction ("is_negative", &Temporal::timecnt_t::is_negative)
		.addFunction ("is_zero", &Temporal::timecnt_t::is_zero)
		.addFunction ("abs", &Temporal::timecnt_t::abs)
		.addFunction ("time_domain", &Temporal::timecnt_t::time_domain)
		.addFunction ("set_time_domain", &Temporal::timecnt_t::set_time_domain)
		.addFunction ("superclocks", &Temporal::timecnt_t::superclocks)
		.addFunction ("samples", &Temporal::timecnt_t::samples)
		.addFunction ("beats", &Temporal::timecnt_t::beats)
		.addFunction ("ticks", &Temporal::timecnt_t::ticks)
		.addFunction ("str", &Temporal::timecnt_t::str)
		.addMetamethod ("__tostring", &Temporal::timecnt_t::str)
		.endClass ()

		.beginClass <Temporal::BBT_Time> ("BBT_TIME")
		.addConstructor <void (*) (uint32_t, uint32_t, uint32_t)> ()
		.addData ("bars", &Temporal::BBT_Time::bars)
		.addData ("beats", &Temporal::BBT_Time::beats)
		.addData ("ticks", &Temporal::BBT_Time::ticks)
		// .addStaticData ("ticks_per_beat", &Temporal::ticks_per_beat, false)
		.endClass ()

		.beginClass <Temporal::Tempo> ("Tempo")
		.addConstructor <void (*) (double, double, int)> ()
		.addFunction ("note_type", &Temporal::Tempo::note_type)
		.addFunction ("note_types_per_minute",  (double (Temporal::Tempo::*)() const)&Temporal::Tempo::note_types_per_minute)
		.addFunction ("quarter_notes_per_minute", &Temporal::Tempo::quarter_notes_per_minute)
		.addFunction ("samples_per_quarter_note", &Temporal::Tempo::samples_per_quarter_note)
		.addFunction ("samples_per_note_type", &Temporal::Tempo::samples_per_note_type)
		.endClass ()

		.beginClass <Temporal::Meter> ("Meter")
		.addConstructor <void (*) (double, double)> ()
		.addFunction ("divisions_per_bar", &Temporal::Meter::divisions_per_bar)
		.addFunction ("note_value", &Temporal::Meter::note_value)
		.endClass ()

		.beginClass <Temporal::Point> ("Point")
		.addFunction ("sclock", &Temporal::Point::sclock)
		.addFunction ("beats", &Temporal::Point::beats)
		.addFunction ("sample", &Temporal::Point::sample)
		.addFunction ("bbt", &Temporal::Point::bbt)
		.addFunction ("time", &Temporal::Point::time)
		.endClass ()

		.deriveClass <Temporal::TempoPoint, Temporal::Tempo> ("TempoPoint")
		.addCast<Temporal::Point> ("to_point")
		.endClass ()

		.deriveClass <Temporal::MeterPoint, Temporal::Meter> ("MeterPoint")
		.addCast<Temporal::Point> ("to_point")
		.endClass ()

		.beginWSPtrClass <Temporal::TempoMap> ("TempoMap")
		.addStaticFunction ("use", &Temporal::TempoMap::use)
		.addStaticFunction ("fetch", &Temporal::TempoMap::fetch)
		.addStaticFunction ("fetch_writable", &Temporal::TempoMap::fetch_writable)
		.addStaticFunction ("write_copy", &Temporal::TempoMap::write_copy)
		.addStaticFunction ("update", &Temporal::TempoMap::update)
		.addStaticFunction ("abort_update", &Temporal::TempoMap::abort_update)
		.addFunction ("set_tempo", (Temporal::TempoPoint& (Temporal::TempoMap::*)(Temporal::Tempo const &,Temporal::timepos_t const &)) &Temporal::TempoMap::set_tempo)
		.addFunction ("set_meter", (Temporal::MeterPoint& (Temporal::TempoMap::*)(Temporal::Meter const &,Temporal::timepos_t const &)) &Temporal::TempoMap::set_meter)
		.addFunction ("tempo_at", (Temporal::TempoPoint const & (Temporal::TempoMap::*)(Temporal::timepos_t const &) const) &Temporal::TempoMap::tempo_at)
		.addFunction ("meter_at", (Temporal::MeterPoint const & (Temporal::TempoMap::*)(Temporal::timepos_t const &) const) &Temporal::TempoMap::meter_at)
		.addFunction ("bbt_at", (Temporal::BBT_Time (Temporal::TempoMap::*)(Temporal::timepos_t const &) const) &Temporal::TempoMap::bbt_at)
		.addFunction ("quarters_at", (Temporal::Beats (Temporal::TempoMap::*)(Temporal::timepos_t const &) const) &Temporal::TempoMap::quarters_at)
		.addFunction ("sample_at", (samplepos_t (Temporal::TempoMap::*)(Temporal::timepos_t const &) const) &Temporal::TempoMap::sample_at)
		.endClass ()

		/* libtemporal enums */
		.beginNamespace ("TimeDomain")
		.addConst ("AudioTime", Temporal::AudioTime)
		.addConst ("BeatTime", Temporal::BeatTime)
		.endNamespace ()

		.beginNamespace ("Tempo")
		.beginNamespace ("Type")
		.addConst ("Ramp", Temporal::Tempo::Type(Temporal::Tempo::Ramped))
		.addConst ("Constant", Temporal::Tempo::Type(Temporal::Tempo::Constant))
		.endNamespace ()
		.endNamespace ()

		.endNamespace () /* end of Temporal namespace */

		.beginNamespace ("Evoral")
		.beginClass <Evoral::Event<samplepos_t> > ("Event")
		.addFunction ("clear", &Evoral::Event<samplepos_t>::clear)
		.addFunction ("size", &Evoral::Event<samplepos_t>::size)
		.addFunction ("set_buffer", &Evoral::Event<samplepos_t>::set_buffer)
		.addFunction ("buffer", (uint8_t*(Evoral::Event<samplepos_t>::*)())&Evoral::Event<samplepos_t>::buffer)
		.addFunction ("time", (samplepos_t (Evoral::Event<samplepos_t>::*)())&Evoral::Event<samplepos_t>::time)
		.endClass ()

		.beginClass <Evoral::Parameter> ("Parameter")
		.addConstructor <void (*) (uint32_t, uint8_t, uint32_t)> ()
		.addFunction ("type", &Evoral::Parameter::type)
		.addFunction ("channel", &Evoral::Parameter::channel)
		.addFunction ("id", &Evoral::Parameter::id)
		.endClass ()

		.beginClass <Evoral::ControlEvent> ("ControlEvent")
		.addData ("when", &Evoral::ControlEvent::when)
		.addData ("value", &Evoral::ControlEvent::value)
		.endClass ()

		.beginWSPtrClass <Evoral::ControlList> ("ControlList")
		.addCast<AutomationList> ("to_automationlist")
		.addFunction ("add", &Evoral::ControlList::add)
		.addFunction ("editor_add", &Evoral::ControlList::editor_add)
		.addFunction ("thin", &Evoral::ControlList::thin)
		.addFunction ("eval", &Evoral::ControlList::eval)
		.addRefFunction ("rt_safe_eval", &Evoral::ControlList::rt_safe_eval)
		.addFunction ("interpolation", &Evoral::ControlList::interpolation)
		.addFunction ("set_interpolation", &Evoral::ControlList::set_interpolation)
		.addFunction ("truncate_end", &Evoral::ControlList::truncate_end)
		.addFunction ("truncate_start", &Evoral::ControlList::truncate_start)
		.addFunction ("clear", (void (Evoral::ControlList::*)(Temporal::timepos_t const &, timepos_t const &))&Evoral::ControlList::clear)
		.addFunction ("clear_list", (void (Evoral::ControlList::*)())&Evoral::ControlList::clear)
		.addFunction ("in_write_pass", &Evoral::ControlList::in_write_pass)
		.addFunction ("events", &Evoral::ControlList::events)
		.addFunction ("size", &Evoral::ControlList::size)
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
		.addData ("logarithmic", &Evoral::ParameterDescriptor::logarithmic)
		.addData ("rangesteps", &Evoral::ParameterDescriptor::rangesteps)
		.endClass ()

		.beginClass <Temporal::Range> ("Range")
		.addConstructor <void (*) (Temporal::timepos_t, Temporal::timepos_t)> ()
		.addFunction ("start", &Temporal::Range::start)
		/* "end is a reserved Lua word */
		.addFunction ("_end", &Temporal::Range::end)
		.endClass ()

		.deriveWSPtrClass <Evoral::Sequence<Temporal::Beats>, Evoral::ControlSet> ("Sequence")
		.endClass ()

		.beginWSPtrClass <Evoral::Note<Temporal::Beats> > ("NotePtr")
		.addFunction ("time", &Evoral::Note<Temporal::Beats>::time)
		.addFunction ("note", &Evoral::Note<Temporal::Beats>::note)
		.addFunction ("velocity", &Evoral::Note<Temporal::Beats>::velocity)
		.addFunction ("off_velocity", &Evoral::Note<Temporal::Beats>::off_velocity)
		.addFunction ("length", &Evoral::Note<Temporal::Beats>::length)
		.addFunction ("channel", &Evoral::Note<Temporal::Beats>::channel)
		.endClass ()

		/* libevoral enums */
		.beginNamespace ("InterpolationStyle")
		.addConst ("Discrete", Evoral::ControlList::InterpolationStyle(Evoral::ControlList::Discrete))
		.addConst ("Linear", Evoral::ControlList::InterpolationStyle(Evoral::ControlList::Linear))
		.addConst ("Curved", Evoral::ControlList::InterpolationStyle(Evoral::ControlList::Curved))
		.endNamespace ()

		.beginNamespace ("EventType")
		.addConst ("NO_EVENT", Evoral::EventType(Evoral::NO_EVENT))
		.addConst ("MIDI_EVENT", Evoral::EventType(Evoral::MIDI_EVENT))
		.addConst ("LIVE_MIDI_EVENT", Evoral::EventType(Evoral::LIVE_MIDI_EVENT))
		.endNamespace ()

		.endNamespace (); // Evoral

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Vamp")

		.beginClass<Vamp::RealTime> ("RealTime")
		.addConstructor <void (*) (int, int)> ()
		.addData ("sec", &Vamp::RealTime::sec, false)
		.addData ("nsec", &Vamp::RealTime::nsec, false)
		.addFunction ("usec", &Vamp::RealTime::usec)
		.addFunction ("msec", &Vamp::RealTime::msec)
		.addFunction ("toString", &Vamp::RealTime::toString)
		.addStaticFunction ("realTime2Frame", &Vamp::RealTime::realTime2Frame)
		.addStaticFunction ("frame2RealTime", &Vamp::RealTime::frame2RealTime)
		.endClass ()

		.beginClass<Vamp::PluginBase> ("PluginBase")
		.addFunction ("getIdentifier", &Vamp::PluginBase::getIdentifier)
		.addFunction ("getName", &Vamp::PluginBase::getName)
		.addFunction ("getDescription", &Vamp::PluginBase::getDescription)
		.addFunction ("getMaker", &Vamp::PluginBase::getMaker)
		.addFunction ("getCopyright", &Vamp::PluginBase::getCopyright)
		.addFunction ("getPluginVersion", &Vamp::PluginBase::getPluginVersion)
		.addFunction ("getParameterDescriptors", &Vamp::PluginBase::getParameterDescriptors)
		.addFunction ("getParameter", &Vamp::PluginBase::getParameter)
		.addFunction ("setParameter", &Vamp::PluginBase::setParameter)
		.addFunction ("getPrograms", &Vamp::PluginBase::getPrograms)
		.addFunction ("getCurrentProgram", &Vamp::PluginBase::getCurrentProgram)
		.addFunction ("selectProgram", &Vamp::PluginBase::selectProgram)
		.addFunction ("getType", &Vamp::PluginBase::getType)
		.endClass ()

		.beginNamespace ("PluginBase")
		.beginClass<Vamp::PluginBase::ParameterDescriptor> ("ParameterDescriptor")
		.addData ("identifier", &Vamp::PluginBase::ParameterDescriptor::identifier)
		.addData ("name", &Vamp::PluginBase::ParameterDescriptor::name)
		.addData ("description", &Vamp::PluginBase::ParameterDescriptor::description)
		.addData ("unit", &Vamp::PluginBase::ParameterDescriptor::unit)
		.addData ("minValue", &Vamp::PluginBase::ParameterDescriptor::minValue)
		.addData ("maxValue", &Vamp::PluginBase::ParameterDescriptor::maxValue)
		.addData ("defaultValue", &Vamp::PluginBase::ParameterDescriptor::defaultValue)
		.addData ("isQuantized", &Vamp::PluginBase::ParameterDescriptor::isQuantized)
		.addData ("quantizeStep", &Vamp::PluginBase::ParameterDescriptor::quantizeStep)
		.addData ("valueNames", &Vamp::PluginBase::ParameterDescriptor::valueNames)
		.endClass ()

		.beginStdVector <Vamp::PluginBase::ParameterDescriptor> ("ParameterList")
		.endClass ()
		.endNamespace () // Vamp::PluginBase

		.deriveClass<Vamp::Plugin, Vamp::PluginBase> ("Plugin")
		//.addFunction ("process", &Vamp::Plugin::process) // unusable due to  float*const* -> LuaAPI::Vamp::process
		.addFunction ("initialise", &Vamp::Plugin::initialise)
		.addFunction ("reset", &Vamp::Plugin::reset)
		.addFunction ("getInputDomain", &Vamp::Plugin::getInputDomain)
		.addFunction ("getPreferredBlockSize", &Vamp::Plugin::getPreferredBlockSize)
		.addFunction ("getPreferredStepSize", &Vamp::Plugin::getPreferredStepSize)
		.addFunction ("getMinChannelCount", &Vamp::Plugin::getMinChannelCount)
		.addFunction ("getMaxChannelCount", &Vamp::Plugin::getMaxChannelCount)
		.addFunction ("getOutputDescriptors", &Vamp::Plugin::getOutputDescriptors)
		.addFunction ("getRemainingFeatures", &Vamp::Plugin::getRemainingFeatures)
		.addFunction ("getType", &Vamp::Plugin::getType)
		.endClass ()

		.beginNamespace ("Plugin")
		.beginClass<Vamp::Plugin::OutputDescriptor> ("OutputDescriptor")
		.addData ("identifier", &Vamp::Plugin::OutputDescriptor::identifier)
		.addData ("description", &Vamp::Plugin::OutputDescriptor::description)
		.addData ("unit", &Vamp::Plugin::OutputDescriptor::unit)
		.addData ("hasFixedBinCount", &Vamp::Plugin::OutputDescriptor::hasFixedBinCount)
		.addData ("binCount", &Vamp::Plugin::OutputDescriptor::binCount)
		.addData ("binNames", &Vamp::Plugin::OutputDescriptor::binNames)
		.addData ("hasKnownExtents", &Vamp::Plugin::OutputDescriptor::hasKnownExtents)
		.addData ("minValue", &Vamp::Plugin::OutputDescriptor::minValue)
		.addData ("maxValue", &Vamp::Plugin::OutputDescriptor::maxValue)
		.addData ("isQuantized", &Vamp::Plugin::OutputDescriptor::isQuantized)
		.addData ("quantizeStep", &Vamp::Plugin::OutputDescriptor::quantizeStep)
		.addData ("sampleType", &Vamp::Plugin::OutputDescriptor::sampleType)
		.addData ("sampleRate", &Vamp::Plugin::OutputDescriptor::sampleRate)
		.addData ("hasDuration", &Vamp::Plugin::OutputDescriptor::hasDuration)
		.endClass ()

		/* Vamp::Plugin enums */
		.beginNamespace ("InputDomain")
		.addConst ("TimeDomain", Vamp::Plugin::InputDomain(Vamp::Plugin::TimeDomain))
		.addConst ("FrequencyDomain", Vamp::Plugin::InputDomain(Vamp::Plugin::FrequencyDomain))
		.endNamespace ()

		/* Vamp::Plugin::OutputDescriptor enum */
		.beginNamespace ("OutputDescriptor")
		.beginNamespace ("SampleType")
		.addConst ("OneSamplePerStep", Vamp::Plugin::OutputDescriptor::SampleType(Vamp::Plugin::OutputDescriptor::OneSamplePerStep))
		.addConst ("FixedSampleRate", Vamp::Plugin::OutputDescriptor::SampleType(Vamp::Plugin::OutputDescriptor::FixedSampleRate))
		.addConst ("VariableSampleRate", Vamp::Plugin::OutputDescriptor::SampleType(Vamp::Plugin::OutputDescriptor::VariableSampleRate))
		.endNamespace ()
		.endNamespace () /* Vamp::Plugin::OutputDescriptor */

		.beginClass<Vamp::Plugin::Feature> ("Feature")
		.addData ("hasTimestamp", &Vamp::Plugin::Feature::hasTimestamp, false)
		.addData ("timestamp", &Vamp::Plugin::Feature::timestamp, false)
		.addData ("hasDuration", &Vamp::Plugin::Feature::hasDuration, false)
		.addData ("duration", &Vamp::Plugin::Feature::duration, false)
		.addData ("values", &Vamp::Plugin::Feature::values, false)
		.addData ("label", &Vamp::Plugin::Feature::label, false)
		.endClass ()

		.beginStdVector <Vamp::Plugin::OutputDescriptor> ("OutputList")
		.endClass ()

		.beginStdVector <Vamp::Plugin::Feature> ("FeatureList")
		.endClass ()

		.beginStdMap <int, Vamp::Plugin::FeatureList> ("FeatureSet")
		.endClass ()

		.endNamespace () // Vamp::Plugin
		.endNamespace ();// Vamp

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")

		.addConst ("revision", ARDOUR::revision)

		.beginClass <InterThreadInfo> ("InterThreadInfo")
		.addVoidConstructor ()
		.addData ("done", const_cast<bool InterThreadInfo::*>(&InterThreadInfo::done))
#if 0 // currently unused, lua is single-threaded, no custom UIs.
		.addData ("cancel", (bool InterThreadInfo::*)&InterThreadInfo::cancel)
#endif
		.addData ("progress", const_cast<float InterThreadInfo::*>(&InterThreadInfo::progress))
		.endClass ()

		.beginClass <Progress> ("Progress")
		.endClass ()

		.beginClass <TimelineRange> ("TimelineRange")
		.addConstructor <void (*) (Temporal::timepos_t, Temporal::timepos_t, uint32_t)> ()
		.addFunction ("length", &TimelineRange::length)
		.addFunction ("equal", &TimelineRange::equal)
		.addFunction ("start", &TimelineRange::start)
		.addFunction ("_end", &TimelineRange::end) // XXX "end" is a lua reserved word
		.addData ("id", &TimelineRange::id)
		.endClass ()

		.beginWSPtrClass <PluginInfo> ("PluginInfo")
		.addNilPtrConstructor ()
		.addData ("name", &PluginInfo::name, false)
		.addData ("category", &PluginInfo::category, false)
		.addData ("creator", &PluginInfo::creator, false)
		.addData ("path", &PluginInfo::path, false)
		.addData ("n_inputs", &PluginInfo::n_inputs, false)
		.addData ("n_outputs", &PluginInfo::n_outputs, false)
		.addData ("type", &PluginInfo::type, false)
		.addData ("unique_id", &PluginInfo::unique_id, false)
		.addFunction ("is_instrument", &PluginInfo::is_instrument)
		.addFunction ("get_presets", &PluginInfo::get_presets)
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
		.addFunction ("count", &ChanMapping::count)
		.addFunction ("n_total", &ChanMapping::n_total)
		.addFunction ("is_monotonic", &ChanMapping::is_monotonic)
		.addConst ("Invalid", 4294967295U) // UINT32_MAX
		.endClass ()

		.beginNamespace ("Properties")
		// templated class definitions
		.beginClass <PBD::PropertyDescriptor<bool> > ("BoolProperty").endClass ()
		.beginClass <PBD::PropertyDescriptor<float> > ("FloatProperty").endClass ()
		.beginClass <PBD::PropertyDescriptor<samplepos_t> > ("SampleposProperty").endClass ()
		// actual references (TODO: also expose GQuark for std::set)
		//   ardour/region.h
		.addConst ("Start", &ARDOUR::Properties::start)
		.addConst ("Length", &ARDOUR::Properties::length)
		.endNamespace ()

		.beginClass <PBD::PropertyChange> ("PropertyChange")
		// TODO add special handling (std::set<PropertyID>), PropertyID is a GQuark.
		// -> direct map to lua table  beginStdSet()
		//
		// expand templated PropertyDescriptor<T>
		.addFunction ("containsBool", &PBD::PropertyChange::contains<bool>)
		.addFunction ("containsFloat", &PBD::PropertyChange::contains<float>)
		.addFunction ("containsSamplePos", &PBD::PropertyChange::contains<samplepos_t>)
		.endClass ()

		.beginClass <PBD::PropertyList> ("PropertyList")
		// is-a  std::map<PropertyID, PropertyBase*>
		.endClass ()

		.deriveClass <PBD::OwnedPropertyList, PBD::PropertyList> ("OwnedPropertyList")
		.endClass ()

		.deriveWSPtrClass <AutomationList, Evoral::ControlList> ("AutomationList")
		.addCast<PBD::Stateful> ("to_stateful")
		.addCast<PBD::StatefulDestructible> ("to_statefuldestructible")
		.addCast<Evoral::ControlList> ("list") // deprecated
		.addFunction ("get_state", &AutomationList::get_state)
		.addFunction ("memento_command", &AutomationList::memento_command)
		.addFunction ("touching", &AutomationList::touching)
		.addFunction ("writing", &AutomationList::writing)
		.addFunction ("touch_enabled", &AutomationList::touch_enabled)
		.endClass ()

		.deriveClass <Location, PBD::StatefulDestructible> ("Location")
		.addFunction ("name", &Location::name)
		.addFunction ("locked", &Location::locked)
		.addFunction ("lock", &Location::lock)
		.addFunction ("unlock", &Location::unlock)
		.addFunction ("start", &Location::start)
		.addFunction ("_end", &Location::end) // XXX "end" is a lua reserved word
		.addFunction ("length", &Location::length)
		.addFunction ("set_start", &Location::set_start)
		.addFunction ("set_end", &Location::set_end)
		.addFunction ("set", &Location::set)
		.addFunction ("set_length", &Location::set)
		.addFunction ("set_name", &Location::set_name)
		.addFunction ("move_to", &Location::move_to)
		.addFunction ("matches", &Location::matches)
		.addFunction ("flags", &Location::flags)
		.addFunction ("is_auto_punch", &Location::is_auto_punch)
		.addFunction ("is_auto_loop", &Location::is_auto_loop)
		.addFunction ("is_mark", &Location::is_mark)
		.addFunction ("is_hidden", &Location::is_hidden)
		.addFunction ("is_cd_marker", &Location::is_cd_marker)
		.addFunction ("is_cue_marker", &Location::is_cd_marker)
		.addFunction ("is_session_range", &Location::is_session_range)
		.addFunction ("is_range_marker", &Location::is_range_marker)
		.endClass ()

		.deriveClass <Locations, PBD::StatefulDestructible> ("Locations")
		.addFunction ("list", static_cast<Locations::LocationList (Locations::*)()>(&Locations::list))
		.addFunction ("auto_loop_location", &Locations::auto_loop_location)
		.addFunction ("auto_punch_location", &Locations::auto_punch_location)
		.addFunction ("session_range_location", &Locations::session_range_location)
		.addFunction ("first_mark_after", &Locations::first_mark_after)
		.addFunction ("first_mark_before", &Locations::first_mark_before)
		.addFunction ("first_mark_at", &Locations::mark_at)
		.addFunction ("mark_at", &Locations::mark_at)
		.addFunction ("range_starts_at", &Locations::range_starts_at)
		.addFunction ("add_range", &Locations::add_range)
		.addFunction ("remove", &Locations::remove)
		.addRefFunction ("marks_either_side", &Locations::marks_either_side)
		.addRefFunction ("find_all_between", &Locations::find_all_between)
		.endClass ()

		.beginWSPtrClass <SessionObject> ("SessionObjectPtr")
		/* SessionObject is-a PBD::StatefulDestructible,
		 * but multiple inheritance is not covered by luabridge,
		 * we need explicit casts */
		.addCast<PBD::Stateful> ("to_stateful")
		.addCast<PBD::StatefulDestructible> ("to_statefuldestructible")
		.addFunction ("name", &SessionObject::name)
		.endClass ()

		.beginClass <SessionObject> ("SessionObject")
		.addFunction ("name", &SessionObject::name)
		.addCast<PBD::Stateful> ("to_stateful")
		.endClass ()

		.beginWSPtrClass <Port> ("Port")
		.addCast<MidiPort> ("to_midiport")
		.addCast<AsyncMIDIPort> ("to_asyncmidiport")
		.addCast<AudioPort> ("to_audioport")
		.addFunction ("name", &Port::name)
		.addFunction ("pretty_name", &Port::pretty_name)
		.addFunction ("flags", &Port::flags)
		.addFunction ("receives_input", &Port::receives_input)
		.addFunction ("sends_output", &Port::sends_output)
		.addFunction ("connected", &Port::connected)
		.addFunction ("disconnect_all", &Port::disconnect_all)
		.addFunction ("connected_to", (bool (Port::*)(std::string const &)const)&Port::connected_to)
		.addFunction ("connect", (int (Port::*)(std::string const &))&Port::connect)
		.addFunction ("disconnect", (int (Port::*)(std::string const &))&Port::disconnect)
		.addFunction ("physically_connected", &Port::physically_connected)
		.addFunction ("private_latency_range", &Port::private_latency_range)
		.addFunction ("public_latency_range", &Port::public_latency_range)
		.addRefFunction ("get_connected_latency_range", &Port::get_connected_latency_range)
		//.addStaticFunction ("port_offset", &Port::port_offset) // static
		.endClass ()

		.deriveWSPtrClass <AudioPort, Port> ("AudioPort")
		.endClass ()

		.deriveWSPtrClass <MidiPort, Port> ("MidiPort")
		.addCast<AsyncMIDIPort> ("to_asyncmidiport")
		.addFunction ("input_active", &MidiPort::input_active)
		.addFunction ("set_input_active", &MidiPort::set_input_active)
		.addFunction ("get_midi_buffer", &MidiPort::get_midi_buffer) // DSP only
		.endClass ()

		.deriveWSPtrClass <AsyncMIDIPort, MidiPort> ("AsyncMIDIPort")
		.addFunction ("write", &AsyncMIDIPort::write)
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
		.addFunction ("disconnect_all", (int (IO::*)(void *))&IO::disconnect)
		.addFunction ("physically_connected", &IO::physically_connected)
		.addFunction ("has_port", &IO::has_port)
		.addFunction ("nth", &IO::nth)
		.addFunction ("audio", &IO::audio)
		.addFunction ("midi", &IO::midi)
		.addFunction ("port_by_name", &IO::nth)
		.addFunction ("n_ports", &IO::n_ports)
		.addFunction ("latency", &IO::latency)
		.addFunction ("public_latency", &IO::latency)
		.endClass ()

		.deriveWSPtrClass <PannerShell, SessionObject> ("PannerShell")
		.addFunction ("bypassed", &PannerShell::bypassed)
		.addFunction ("set_bypassed", &PannerShell::set_bypassed)
		.endClass ()

		.deriveClass <RouteGroup, SessionObject> ("RouteGroup")
		.addFunction ("is_active", &RouteGroup::is_active)
		.addFunction ("is_relative", &RouteGroup::is_relative)
		.addFunction ("is_hidden", &RouteGroup::is_hidden)
		.addFunction ("is_gain", &RouteGroup::is_gain)
		.addFunction ("is_mute", &RouteGroup::is_mute)
		.addFunction ("is_solo", &RouteGroup::is_solo)
		.addFunction ("is_recenable", &RouteGroup::is_recenable)
		.addFunction ("is_select", &RouteGroup::is_select)
		.addFunction ("is_route_active", &RouteGroup::is_route_active)
		.addFunction ("is_color", &RouteGroup::is_color)
		.addFunction ("is_monitoring", &RouteGroup::is_monitoring)
		.addFunction ("group_master_number", &RouteGroup::group_master_number)
		.addFunction ("empty", &RouteGroup::empty)
		.addFunction ("size", &RouteGroup::size)
		.addFunction ("set_active", &RouteGroup::set_active)
		.addFunction ("set_relative", &RouteGroup::set_relative)
		.addFunction ("set_hidden", &RouteGroup::set_hidden)
		.addFunction ("set_gain", &RouteGroup::set_gain)
		.addFunction ("set_mute", &RouteGroup::set_mute)
		.addFunction ("set_solo", &RouteGroup::set_solo)
		.addFunction ("set_recenable", &RouteGroup::set_recenable)
		.addFunction ("set_select", &RouteGroup::set_select)
		.addFunction ("set_route_active", &RouteGroup::set_route_active)
		.addFunction ("set_color", &RouteGroup::set_color)
		.addFunction ("set_monitoring", &RouteGroup::set_monitoring)
		.addFunction ("add", &RouteGroup::add)
		.addFunction ("remove", &RouteGroup::remove)
		.addFunction ("clear", &RouteGroup::clear)
		.addFunction ("set_rgba", &RouteGroup::set_rgba)
		.addFunction ("rgba", &RouteGroup::rgba)
		.addFunction ("has_subgroup", &RouteGroup::has_subgroup)
		.addFunction ("make_subgroup", &RouteGroup::make_subgroup)
		.addFunction ("destroy_subgroup", &RouteGroup::destroy_subgroup)
		.addFunction ("route_list", &RouteGroup::route_list)
		.endClass ()

		.deriveClass <PresentationInfo, PBD::Stateful> ("PresentationInfo")
		.addFunction ("color", &PresentationInfo::color)
		.addFunction ("set_color", &PresentationInfo::set_color)
		.addFunction ("order", &PresentationInfo::order)
		.addFunction ("special", &PresentationInfo::special)
		.addFunction ("flags", &PresentationInfo::flags)
		.addConst ("max_order", ARDOUR::PresentationInfo::max_order)
		.endClass ()

		.beginWSPtrClass <Slavable> ("Slavable")
		.addFunction ("assign", &Slavable::assign)
		.addFunction ("unassign", &Slavable::unassign)
		.addFunction ("masters", &Slavable::masters)
		.addFunction ("assigned_to", &Slavable::assigned_to)
		.endClass ()

		.deriveWSPtrClass <Stripable, SessionObject> ("Stripable")
		.addCast<Route> ("to_route")
		.addCast<VCA> ("to_vca")
		.addCast<Slavable> ("to_slavable")
		.addCast<Automatable> ("to_automatable")
		.addFunction ("is_auditioner", &Stripable::is_auditioner)
		.addFunction ("is_private_route", &Stripable::is_private_route)
		.addFunction ("is_master", &Stripable::is_master)
		.addFunction ("is_monitor", &Stripable::is_monitor)
		.addFunction ("is_hidden", &Stripable::is_hidden)
		.addFunction ("is_selected", &Stripable::is_selected)
		.addFunction ("gain_control", &Stripable::gain_control)
		.addFunction ("solo_control", &Stripable::solo_control)
		.addFunction ("solo_isolate_control", &Stripable::solo_isolate_control)
		.addFunction ("solo_safe_control", &Stripable::solo_safe_control)
		.addFunction ("mute_control", &Stripable::mute_control)
		.addFunction ("phase_control", &Stripable::phase_control)
		.addFunction ("trim_control", &Stripable::trim_control)
		.addFunction ("rec_enable_control", &Stripable::rec_enable_control)
		.addFunction ("rec_safe_control", &Stripable::rec_safe_control)
		.addFunction ("pan_azimuth_control", &Stripable::pan_azimuth_control)
		.addFunction ("pan_elevation_control", &Stripable::pan_elevation_control)
		.addFunction ("pan_width_control", &Stripable::pan_width_control)
		.addFunction ("pan_frontback_control", &Stripable::pan_frontback_control)
		.addFunction ("pan_lfe_control", &Stripable::pan_lfe_control)
		.addFunction ("send_level_controllable", &Stripable::send_level_controllable)
		.addFunction ("send_enable_controllable", &Stripable::send_enable_controllable)
		.addFunction ("send_pan_azimuth_controllable", &Stripable::send_pan_azimuth_controllable)
		.addFunction ("send_pan_azimuth_enable_controllable", &Stripable::send_pan_azimuth_enable_controllable)
		.addFunction ("send_name", &Stripable::send_name)
		.addFunction ("monitor_control", &Stripable::monitor_control)
		.addFunction ("master_send_enable_controllable", &Stripable::master_send_enable_controllable)
		.addFunction ("comp_enable_controllable", &Stripable::comp_enable_controllable)
		.addFunction ("comp_threshold_controllable", &Stripable::comp_threshold_controllable)
		.addFunction ("comp_speed_controllable", &Stripable::comp_speed_controllable)
		.addFunction ("comp_mode_controllable", &Stripable::comp_mode_controllable)
		.addFunction ("comp_makeup_controllable", &Stripable::comp_makeup_controllable)
		.addFunction ("comp_redux_controllable", &Stripable::comp_redux_controllable)
		.addFunction ("comp_mode_name", &Stripable::comp_mode_name)
		.addFunction ("comp_speed_name", &Stripable::comp_speed_name)
		.addFunction ("eq_band_cnt", &Stripable::eq_band_cnt)
		.addFunction ("eq_enable_controllable", &Stripable::eq_enable_controllable)
		.addFunction ("eq_band_name", &Stripable::eq_band_name)
		.addFunction ("eq_gain_controllable", &Stripable::eq_gain_controllable)
		.addFunction ("eq_freq_controllable", &Stripable::eq_freq_controllable)
		.addFunction ("eq_q_controllable", &Stripable::eq_q_controllable)
		.addFunction ("eq_shape_controllable", &Stripable::eq_shape_controllable)
		.addFunction ("filter_freq_controllable", &Stripable::filter_freq_controllable)
		.addFunction ("filter_slope_controllable", &Stripable::filter_slope_controllable)
		.addFunction ("filter_enable_controllable", &Stripable::filter_enable_controllable)
		.addFunction ("set_presentation_order", &Stripable::set_presentation_order)
		.addFunction ("presentation_info_ptr", &Stripable::presentation_info_ptr)
		.addFunction ("slaved_to", &Stripable::slaved_to)
		.addFunction ("slaved", &Stripable::slaved)

		.endClass ()

		.deriveWSPtrClass <VCA, Stripable> ("VCA")
		.addFunction ("full_name", &VCA::full_name)
		.addFunction ("number", &VCA::number)
		.addFunction ("gain_control", &VCA::gain_control)
		.addFunction ("solo_control", &VCA::solo_control)
		.addFunction ("mute_control", &VCA::mute_control)
		.endClass ()

		.deriveWSPtrClass <Route, Stripable> ("Route")
		.addCast<Track> ("to_track")
		.addFunction ("set_name", &Route::set_name)
		.addFunction ("comment", &Route::comment)
		.addFunction ("active", &Route::active)
		.addFunction ("data_type", &Route::data_type)
		.addFunction ("set_active", &Route::set_active)
		.addFunction ("nth_plugin", &Route::nth_plugin)
		.addFunction ("nth_processor", &Route::nth_processor)
		.addFunction ("nth_send", &Route::nth_send)
		.addFunction ("add_foldback_send", &Route::add_foldback_send)
		.addFunction ("add_processor_by_index", &Route::add_processor_by_index)
		.addFunction ("remove_processor", &Route::remove_processor)
		.addFunction ("remove_processors", &Route::remove_processors)
		.addFunction ("replace_processor", &Route::replace_processor)
		.addFunction ("reorder_processors", &Route::reorder_processors)
		.addFunction ("the_instrument", &Route::the_instrument)
		.addFunction ("n_inputs", &Route::n_inputs)
		.addFunction ("n_outputs", &Route::n_outputs)
		.addFunction ("input", &Route::input)
		.addFunction ("output", &Route::output)
		.addFunction ("panner_shell", &Route::panner_shell)
		.addFunction ("set_comment", &Route::set_comment)
		.addFunction ("strict_io", &Route::strict_io)
		.addFunction ("set_strict_io", &Route::set_strict_io)
		.addFunction ("reset_plugin_insert", &Route::reset_plugin_insert)
		.addFunction ("customize_plugin_insert", &Route::customize_plugin_insert)
		.addFunction ("add_sidechain", &Route::add_sidechain)
		.addFunction ("add_aux_send", &Route::add_aux_send)
		.addFunction ("remove_sidechain", &Route::remove_sidechain)
		.addFunction ("main_outs", &Route::main_outs)
		.addFunction ("muted", &Route::muted)
		.addFunction ("soloed", &Route::soloed)
		.addFunction ("amp", &Route::amp)
		.addFunction ("trim", &Route::trim)
		.addFunction ("peak_meter", (boost::shared_ptr<PeakMeter> (Route::*)())&Route::peak_meter)
		.addFunction ("set_meter_point", &Route::set_meter_point)
		.addFunction ("signal_latency", &Route::signal_latency)
		.addFunction ("playback_latency", &Route::playback_latency)
		.addFunction ("monitoring_state", &Route::monitoring_state)
		.addFunction ("monitoring_control", &Route::monitoring_control)
		.endClass ()

		.deriveWSPtrClass <Playlist, SessionObject> ("Playlist")
		.addCast<AudioPlaylist> ("to_audioplaylist")
		.addCast<MidiPlaylist> ("to_midiplaylist")
		.addFunction ("set_name", &Playlist::set_name)
		.addFunction ("region_by_id", &Playlist::region_by_id)
		.addFunction ("data_type", &Playlist::data_type)
		.addFunction ("n_regions", &Playlist::n_regions)
		//.addFunction ("get_extent", &Playlist::get_extent) // pair<samplepos_t, samplepos_t>
		.addFunction ("region_list", &Playlist::region_list)
		.addFunction ("add_region", &Playlist::add_region)
		.addFunction ("remove_region", &Playlist::remove_region)
		.addFunction ("regions_at", &Playlist::regions_at)
		.addFunction ("top_region_at", &Playlist::top_region_at)
		.addFunction ("top_unmuted_region_at", &Playlist::top_unmuted_region_at)
		.addFunction ("find_next_transient", &Playlist::find_next_transient)
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
		.addFunction ("duplicate", (void (Playlist::*)(boost::shared_ptr<Region>, Temporal::timepos_t &, timecnt_t const &, float))&Playlist::duplicate)
		.addFunction ("duplicate_until", &Playlist::duplicate_until)
		.addFunction ("duplicate_range", &Playlist::duplicate_range)
		.addFunction ("combine", &Playlist::combine)
		.addFunction ("uncombine", &Playlist::uncombine)
		.addFunction ("used", &Playlist::used)
		.addFunction ("hidden", &Playlist::hidden)
		.addFunction ("empty", &Playlist::empty)
		.addFunction ("shared", &Playlist::shared)
		.addFunction ("split_region", &Playlist::split_region)
		.addFunction ("get_orig_track_id", &Playlist::get_orig_track_id)
		//.addFunction ("split", &Playlist::split) // XXX needs MusicSample
		.addFunction ("cut", (boost::shared_ptr<Playlist> (Playlist::*)(std::list<TimelineRange>&, bool))&Playlist::cut)
#if 0
		.addFunction ("copy", &Playlist::copy)
		.addFunction ("paste", &Playlist::paste)
#endif
		.endClass ()

		.beginWSPtrClass <Bundle> ("Bundle")
		.addCast<UserBundle> ("to_userbundle")
		.addFunction ("name", &Bundle::name)
		.addFunction ("n_total", &Bundle::n_total)
		.addFunction ("nchannels", &Bundle::nchannels)
		.addFunction ("channel_name", &Bundle::channel_name)
		.addFunction ("ports_are_inputs", &Bundle::ports_are_inputs)
		.addFunction ("ports_are_outputs", &Bundle::ports_are_outputs)
		.endClass ()

		.deriveWSPtrClass <UserBundle, Bundle> ("UserBundle")
		.endClass ()

		.deriveWSPtrClass <AudioPlaylist, Playlist> ("AudioPlaylist")
		.addFunction ("read", &AudioPlaylist::read)
		.endClass ()

		.deriveWSPtrClass <MidiPlaylist, Playlist> ("MidiPlaylist")
		.addFunction ("set_note_mode", &MidiPlaylist::set_note_mode)
		.endClass ()

		.beginWSPtrClass <SessionPlaylists> ("SessionPlaylists")
		.addFunction ("by_name", &SessionPlaylists::by_name)
		.addFunction ("by_id", &SessionPlaylists::by_id)
		.addFunction ("source_use_count", &SessionPlaylists::source_use_count)
		.addFunction ("region_use_count", &SessionPlaylists::region_use_count)
		.addFunction ("playlists_for_track", &SessionPlaylists::playlists_for_track)
		.addFunction ("get_used", &SessionPlaylists::get_used)
		.addFunction ("get_unused", &SessionPlaylists::get_unused)
		.addFunction ("n_playlists", &SessionPlaylists::n_playlists)
		.endClass ()

		.deriveWSPtrClass <Track, Route> ("Track")
		.addCast<AudioTrack> ("to_audio_track")
		.addCast<MidiTrack> ("to_midi_track")
		.addFunction ("set_name", &Track::set_name)
		.addFunction ("can_record", &Track::can_record)
		.addFunction ("bounceable", &Track::bounceable)
		.addFunction ("bounce", &Track::bounce)
		.addFunction ("bounce_range", &Track::bounce_range)
		.addFunction ("playlist", &Track::playlist)
		.addFunction ("use_playlist", &Track::use_playlist)
		.addFunction ("use_copy_playlist", &Track::use_copy_playlist)
		.addFunction ("use_new_playlist", &Track::use_new_playlist)
		.addFunction ("find_and_use_playlist", &Track::find_and_use_playlist)
		.endClass ()

		.deriveWSPtrClass <AudioTrack, Track> ("AudioTrack")
		.endClass ()

		.deriveWSPtrClass <MidiTrack, Track> ("MidiTrack")
		.addFunction ("write_immediate_event", &MidiTrack::write_immediate_event)
		.addFunction ("set_input_active", &MidiTrack::set_input_active)
		.addFunction ("input_active", &MidiTrack::input_active)
		.endClass ()

		.beginWSPtrClass <AudioReadable> ("Readable")
		.addFunction ("read", &AudioReadable::read)
		.addFunction ("readable_length", &AudioReadable::readable_length_samples)
		.addFunction ("n_channels", &AudioReadable::n_channels)
		.addStaticFunction ("load", &AudioReadable::load)
		.endClass ()

		.deriveWSPtrClass <AudioRom, AudioReadable> ("AudioRom")
		.addStaticFunction ("new_rom", &AudioRom::new_rom)
		.endClass ()

		.deriveWSPtrClass <Region, SessionObject> ("Region")
		.addCast<MidiRegion> ("to_midiregion")
		.addCast<AudioRegion> ("to_audioregion")

		.addFunction ("playlist", &Region::playlist)
		.addFunction ("set_name", &Region::set_name)
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
		.addFunction ("covers", (bool (Region::*)(Temporal::timepos_t const &) const) &Region::covers)
		.addFunction ("at_natural_position", &Region::at_natural_position)
		.addFunction ("is_compound", &Region::is_compound)
		.addFunction ("captured_xruns", &Region::captured_xruns)

		.addFunction ("has_transients", &Region::has_transients)
		.addFunction ("transients", (AnalysisFeatureList (Region::*)())&Region::transients)

		/* editing operations */
		.addFunction ("set_length", &Region::set_length)
		.addFunction ("set_start", &Region::set_start)
		.addFunction ("set_position", &Region::set_position)
		.addFunction ("set_initial_position", &Region::set_initial_position)
		.addFunction ("nudge_position", &Region::nudge_position)
		.addFunction ("move_to_natural_position", &Region::move_to_natural_position)
		.addFunction ("move_start", &Region::move_start)
		.addFunction ("master_sources", &Region::master_sources)
		.addFunction ("master_source_names", &Region::master_source_names)
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
		.addFunction ("source", &Region::source)
		.addFunction ("control", static_cast<boost::shared_ptr<Evoral::Control>(Region::*)(const Evoral::Parameter&, bool)>(&Region::control))
		.endClass ()

		.deriveWSPtrClass <MidiRegion, Region> ("MidiRegion")
		.addFunction ("do_export", &MidiRegion::do_export)
		.addFunction ("midi_source", &MidiRegion::midi_source)
		.addFunction ("model", (boost::shared_ptr<MidiModel> (MidiRegion::*)())&MidiRegion::model)
		.endClass ()

		.deriveWSPtrClass <AudioRegion, Region> ("AudioRegion")
		.addCast<AudioReadable> ("to_readable")
		.addFunction ("n_channels", &AudioRegion::n_channels)
		.addFunction ("audio_source", &AudioRegion::audio_source)
		.addFunction ("set_scale_amplitude", &AudioRegion::set_scale_amplitude)
		.addFunction ("scale_amplitude", &AudioRegion::scale_amplitude)
		.addFunction ("maximum_amplitude", &AudioRegion::maximum_amplitude)
		.addFunction ("rms", &AudioRegion::rms)
		.addFunction ("envelope", &AudioRegion::envelope)
		.addFunction ("envelope_active", &AudioRegion::envelope_active)
		.addFunction ("fade_in_active", &AudioRegion::fade_in_active)
		.addFunction ("fade_out_active", &AudioRegion::fade_out_active)
		.addFunction ("set_envelope_active", &AudioRegion::set_envelope_active)
		.addFunction ("set_fade_in_active", &AudioRegion::set_fade_in_active)
		.addFunction ("set_fade_in_shape", &AudioRegion::set_fade_in_shape)
		.addFunction ("set_fade_in_length", &AudioRegion::set_fade_in_length)
		.addFunction ("set_fade_out_active", &AudioRegion::set_fade_out_active)
		.addFunction ("set_fade_out_shape", &AudioRegion::set_fade_out_shape)
		.addFunction ("set_fade_out_length", &AudioRegion::set_fade_out_length)
		.addRefFunction ("separate_by_channel", &AudioRegion::separate_by_channel)
		.endClass ()

		.deriveWSPtrClass <Source, SessionObject> ("Source")
		.addCast<AudioSource> ("to_audiosource")
		.addCast<MidiSource> ("to_midisource")
		.addCast<FileSource> ("to_filesource")
		.addFunction ("timestamp", &Source::timestamp)
		.addFunction ("empty", &Source::empty)
		.addFunction ("length", &Source::length)
		.addFunction ("natural_position", &Source::natural_position)
		.addFunction ("writable", &Source::writable)
		.addFunction ("has_been_analysed", &Source::has_been_analysed)
		.addFunction ("can_be_analysed", &Source::can_be_analysed)
		.addFunction ("timeline_position", &Source::natural_position) /* duplicate */
		.addFunction ("use_count", &Source::use_count)
		.addFunction ("used", &Source::used)
		.addFunction ("ancestor_name", &Source::ancestor_name)
		.addFunction ("captured_xruns", &Source::captured_xruns)
		.endClass ()

		.deriveWSPtrClass <FileSource, Source> ("FileSource")
		.addFunction ("path", &FileSource::path)
		.addFunction ("within_session", &FileSource::within_session)
		.addFunction ("channel", &FileSource::channel)
		.addFunction ("origin", &FileSource::origin)
		.addFunction ("take_id", &FileSource::take_id)
		.addFunction ("gain", &FileSource::gain)
		.endClass ()

		.deriveWSPtrClass <MidiSource, Source> ("MidiSource")
		.addFunction ("empty", &MidiSource::empty)
		.addFunction ("length", &MidiSource::length)
		.addFunction ("model", &MidiSource::model)
		.endClass ()

		.deriveWSPtrClass <AudioSource, Source> ("AudioSource")
		.addCast<AudioReadable> ("to_readable")
		.addFunction ("readable_length", &AudioSource::readable_length_samples)
		.addFunction ("n_channels", &AudioSource::n_channels)
		.addFunction ("empty", &Source::empty)
		.addFunction ("length", &Source::length)
		.addFunction ("read", &AudioSource::read)
		.addFunction ("sample_rate", &AudioSource::sample_rate)
		.addFunction ("captured_for", &AudioSource::captured_for)
		.endClass ()

		.beginWSPtrClass <Latent> ("Latent")
		.addFunction ("effective_latency", &Latent::effective_latency)
		.addFunction ("user_latency", &Latent::user_latency)
		.addFunction ("unset_user_latency", &Latent::unset_user_latency)
		.addFunction ("set_user_latency", &Latent::set_user_latency)
		.endClass ()

		.beginClass <Latent> ("PDC")
		/* cannot reuse "Latent"; weak/shared-ptr refs cannot have static member functions */
		.addStaticFunction ("zero_latency", &Latent::zero_latency)
		.addStaticFunction ("force_zero_latency", &Latent::force_zero_latency)
		.endClass ()

		.deriveWSPtrClass <Automatable, Evoral::ControlSet> ("Automatable")
		.addCast<Slavable> ("to_slavable")
		.addFunction ("automation_control", (boost::shared_ptr<AutomationControl>(Automatable::*)(const Evoral::Parameter&, bool))&Automatable::automation_control)
		.addFunction ("all_automatable_params", &Automatable::all_automatable_params)
		.endClass ()

		.deriveWSPtrClass <AutomatableSequence<Temporal::Beats>, Automatable> ("AutomatableSequence")
		.addCast<Evoral::Sequence<Temporal::Beats> > ("to_sequence")
		.endClass ()

		.deriveWSPtrClass <MidiModel, AutomatableSequence<Temporal::Beats> > ("MidiModel")
		.addFunction ("apply_command", (void (MidiModel::*)(Session*, Command*))&MidiModel::apply_command)
		.addFunction ("new_note_diff_command", &MidiModel::new_note_diff_command)
		.endClass ()

		.beginNamespace ("MidiModel")
		.deriveClass<ARDOUR::MidiModel::DiffCommand, Command> ("DiffCommand")
		.endClass ()

		.deriveClass<ARDOUR::MidiModel::NoteDiffCommand, ARDOUR::MidiModel::DiffCommand> ("NoteDiffCommand")
		.addFunction ("add", &ARDOUR::MidiModel::NoteDiffCommand::add)
		.addFunction ("remove", &ARDOUR::MidiModel::NoteDiffCommand::remove)
		.endClass ()

		.endNamespace () /* ARDOUR::MidiModel */

		.beginClass <Plugin::PresetRecord> ("PresetRecord")
		.addVoidConstructor ()
		.addData ("uri", &Plugin::PresetRecord::uri, false)
		.addData ("label", &Plugin::PresetRecord::label, false)
		.addData ("user", &Plugin::PresetRecord::user, false)
		.addData ("valid", &Plugin::PresetRecord::valid, false)
		.endClass ()

		.beginStdVector <Plugin::PresetRecord> ("PresetVector").endClass ()
		.beginStdList <boost::shared_ptr<ARDOUR::PluginInfo> > ("PluginInfoList").endClass ()

		.deriveClass <ParameterDescriptor, Evoral::ParameterDescriptor> ("ParameterDescriptor")
		.addVoidConstructor ()
		.addData ("label", &ParameterDescriptor::label)
		.addData ("print_fmt", &ParameterDescriptor::print_fmt)
		.addData ("step", &ParameterDescriptor::step)
		.addData ("smallstep", &ParameterDescriptor::smallstep)
		.addData ("largestep", &ParameterDescriptor::largestep)
		.addData ("integer_step", &ParameterDescriptor::integer_step)
		.addData ("sr_dependent", &ParameterDescriptor::sr_dependent)
		.addData ("enumeration", &ParameterDescriptor::enumeration)
		.addData ("inline_ctrl", &ParameterDescriptor::inline_ctrl)
		.addData ("display_priority", &ParameterDescriptor::display_priority)
		.addStaticFunction ("midi_note_name", &ParameterDescriptor::midi_note_name)
		.endClass ()

		.beginStdVector <boost::shared_ptr<ARDOUR::Processor> > ("ProcessorVector").endClass ()

		.deriveWSPtrClass <Processor, SessionObject> ("Processor")
		.addCast<Automatable> ("to_automatable")
		.addCast<Latent> ("to_latent")
		.addCast<PluginInsert> ("to_insert") // deprecated
		.addCast<PluginInsert> ("to_plugininsert")
		.addCast<SideChain> ("to_sidechain")
		.addCast<IOProcessor> ("to_ioprocessor")
		.addCast<UnknownProcessor> ("to_unknownprocessor")
		.addCast<Amp> ("to_amp")
		.addCast<DiskIOProcessor> ("to_diskioprocessor")
		.addCast<DiskReader> ("to_diskreader")
		.addCast<DiskWriter> ("to_diskwriter")
		.addCast<PeakMeter> ("to_peakmeter")
		.addCast<MonitorProcessor> ("to_monitorprocessor")
		.addCast<Send> ("to_send")
		.addCast<InternalSend> ("to_internalsend")
		.addCast<PolarityProcessor> ("to_polarityprocessor")
		.addCast<DelayLine> ("to_delayline")
#if 0 // those objects are not yet bound
		.addCast<CapturingProcessor> ("to_capturingprocessor")
#endif
		.addCast<PeakMeter> ("to_meter")
		.addFunction ("display_name", &Processor::display_name)
		.addFunction ("display_to_user", &Processor::display_to_user)
		.addFunction ("active", &Processor::active)
		.addFunction ("activate", &Processor::activate)
		.addFunction ("deactivate", &Processor::deactivate)
		.addFunction ("input_latency", &Processor::input_latency)
		.addFunction ("output_latency", &Processor::output_latency)
		.addFunction ("capture_offset", &Processor::capture_offset)
		.addFunction ("playback_offset", &Processor::playback_offset)
		.addFunction ("output_streams", &Processor::output_streams)
		.addFunction ("input_streams", &Processor::input_streams)
		.addFunction ("signal_latency", &Processor::signal_latency)
		.endClass ()

		.deriveWSPtrClass <DiskIOProcessor, Processor> ("DiskIOProcessor")
		.endClass ()

		.deriveWSPtrClass <DiskReader, DiskIOProcessor> ("DiskReader")
		.endClass ()

		.deriveWSPtrClass <DiskWriter, DiskIOProcessor> ("DiskWriter")
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
		.addFunction ("panner_shell", &Route::panner_shell)
		.endClass ()

		.deriveWSPtrClass <Send, Delivery> ("Send")
		.addCast<InternalSend> ("to_internalsend")
		.addFunction ("get_delay_in", &Send::get_delay_in)
		.addFunction ("get_delay_out", &Send::get_delay_out)
		.addFunction ("gain_control", &Send::gain_control)
		.addFunction ("is_foldback", &Send::is_foldback)
		.addFunction ("set_remove_on_disconnect", &Send::set_remove_on_disconnect)
		.endClass ()

		.deriveWSPtrClass <InternalSend, Send> ("InternalSend")
		.addFunction ("set_name", &InternalSend::set_name)
		.addFunction ("display_name", &InternalSend::display_name)
		.addFunction ("source_route", &InternalSend::source_route)
		.addFunction ("target_route", &InternalSend::target_route)
		.addFunction ("allow_feedback", &InternalSend::allow_feedback)
		.addFunction ("set_allow_feedback", &InternalSend::set_allow_feedback)
		.addFunction ("feeds", &InternalSend::feeds)
		.endClass ()

		.deriveWSPtrClass <Return, IOProcessor> ("Return")
		.endClass ()

		.deriveWSPtrClass <InternalReturn, Return> ("InternalReturn")
		.endClass ()
		.endNamespace (); // end ARDOUR

	/* take a breath */
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")

		.beginNamespace ("Plugin")
		.beginClass <Plugin::IOPortDescription> ("IOPortDescription")
		.addData ("name", &Plugin::IOPortDescription::name)
		.addData ("is_sidechain", &Plugin::IOPortDescription::is_sidechain)
		.addData ("group_name", &Plugin::IOPortDescription::group_name)
		.addData ("group_channel", &Plugin::IOPortDescription::group_channel)
		.endClass ()
		.endNamespace ()

		.deriveWSPtrClass <Plugin, PBD::StatefulDestructible> ("Plugin")
		.addCast<LuaProc> ("to_luaproc")
		.addFunction ("unique_id", &Plugin::unique_id)
		.addFunction ("label", &Plugin::label)
		.addFunction ("name", &Plugin::name)
		.addFunction ("maker", &Plugin::maker)
		.addFunction ("parameter_count", &Plugin::parameter_count)
		.addFunction ("parameter_label", &Plugin::parameter_label)
		.addRefFunction ("nth_parameter", &Plugin::nth_parameter)
		.addFunction ("preset_by_label", &Plugin::preset_by_label)
		.addFunction ("preset_by_uri", &Plugin::preset_by_uri)
		.addFunction ("load_preset", &Plugin::load_preset)
		.addFunction ("last_preset", &Plugin::last_preset)
		.addFunction ("parameter_is_input", &Plugin::parameter_is_input)
		.addFunction ("parameter_is_output", &Plugin::parameter_is_output)
		.addFunction ("parameter_is_control", &Plugin::parameter_is_control)
		.addFunction ("parameter_is_audio", &Plugin::parameter_is_audio)
		.addFunction ("get_docs", &Plugin::get_docs)
		.addFunction ("get_info", &Plugin::get_info)
		.addFunction ("get_parameter_docs", &Plugin::get_parameter_docs)
		.addFunction ("describe_io_port", &Plugin::describe_io_port)
		.addRefFunction ("get_parameter_descriptor", &Plugin::get_parameter_descriptor)
		.endClass ()

		.deriveWSPtrClass <LuaProc, Plugin> ("LuaProc")
		.addFunction ("shmem", &LuaProc::instance_shm)
		.addFunction ("table", &LuaProc::instance_ref)
		.endClass ()

		.deriveWSPtrClass <PluginInsert, Processor> ("PluginInsert")
		.addFunction ("plugin", &PluginInsert::plugin)
		.addFunction ("activate", &PluginInsert::activate)
		.addFunction ("deactivate", &PluginInsert::deactivate)
		.addFunction ("enable", &PluginInsert::enable)
		.addFunction ("enabled", &PluginInsert::enabled)
		.addFunction ("strict_io_configured", &PluginInsert::strict_io_configured)
		.addFunction ("write_immediate_event", &PluginInsert::write_immediate_event)
		.addFunction ("thru_map", &PluginInsert::thru_map)
		.addFunction ("input_map", (ARDOUR::ChanMapping (PluginInsert::*)(uint32_t) const)&PluginInsert::input_map)
		.addFunction ("output_map", (ARDOUR::ChanMapping (PluginInsert::*)(uint32_t) const)&PluginInsert::output_map)
		.addFunction ("set_thru_map", &PluginInsert::set_thru_map)
		.addFunction ("set_input_map", &PluginInsert::set_input_map)
		.addFunction ("set_output_map", &PluginInsert::set_output_map)
		.addFunction ("natural_output_streams", &PluginInsert::natural_output_streams)
		.addFunction ("natural_input_streams", &PluginInsert::natural_input_streams)
		.addFunction ("reset_parameters_to_default", &PluginInsert::reset_parameters_to_default)
		.addFunction ("has_sidechain", &PluginInsert::has_sidechain)
		.addFunction ("sidechain_input", &PluginInsert::sidechain_input)
		.addFunction ("is_instrument", &PluginInsert::is_instrument)
		.addFunction ("type", &PluginInsert::type)
		.addFunction ("signal_latency", &PluginInsert::signal_latency)
		.addFunction ("get_count", &PluginInsert::get_count)
		.addFunction ("is_channelstrip", &PluginInsert::is_channelstrip)
		.addFunction ("clear_stats", &PluginInsert::clear_stats)
		.addRefFunction ("get_stats", &PluginInsert::get_stats)
		.endClass ()

		.deriveWSPtrClass <ReadOnlyControl, PBD::StatefulDestructible> ("ReadOnlyControl")
		.addFunction ("get_parameter", &ReadOnlyControl::get_parameter)
		.addFunction ("describe_parameter", &ReadOnlyControl::describe_parameter)
		.addFunction ("desc", &ReadOnlyControl::desc)
		.endClass ()

		.deriveWSPtrClass <AutomationControl, PBD::Controllable> ("AutomationControl")
		.addCast<Evoral::Control> ("to_ctrl")
		.addCast<SlavableAutomationControl> ("to_slavable")
		.addFunction ("automation_state", &AutomationControl::automation_state)
		.addFunction ("set_automation_state", &AutomationControl::set_automation_state)
		.addFunction ("start_touch", &AutomationControl::start_touch)
		.addFunction ("stop_touch", &AutomationControl::stop_touch)
		.addFunction ("get_value", &AutomationControl::get_value)
		.addFunction ("set_value", &AutomationControl::set_value)
		.addFunction ("writable", &AutomationControl::writable)
		.addFunction ("alist", &AutomationControl::alist)
		.addFunction ("desc", &AutomationControl::desc)
		.addFunction ("lower", &AutomationControl::lower)
		.addFunction ("upper", &AutomationControl::upper)
		.addFunction ("normal", &AutomationControl::normal)
		.addFunction ("toggled", &AutomationControl::toggled)
		.endClass ()

		.deriveWSPtrClass <SlavableAutomationControl, AutomationControl> ("SlavableAutomationControl")
		.addFunction ("add_master", &SlavableAutomationControl::add_master)
		.addFunction ("remove_master", &SlavableAutomationControl::remove_master)
		.addFunction ("clear_masters", &SlavableAutomationControl::clear_masters)
		.addFunction ("slaved_to", &SlavableAutomationControl::slaved_to)
		.addFunction ("slaved", &SlavableAutomationControl::slaved)
		.addFunction ("get_masters_value", &SlavableAutomationControl::get_masters_value)
		.addFunction ("get_boolean_masters", &SlavableAutomationControl::get_boolean_masters)
		//.addFunction ("masters", &SlavableAutomationControl::masters) // not implemented
		.endClass ()

		.deriveWSPtrClass <PhaseControl, AutomationControl> ("PhaseControl")
		.addFunction ("set_phase_invert", (void(PhaseControl::*)(uint32_t, bool))&PhaseControl::set_phase_invert)
		.addFunction ("inverted", &PhaseControl::inverted)
		.endClass ()

		.deriveWSPtrClass <GainControl, SlavableAutomationControl> ("GainControl")
		.endClass ()

		.deriveWSPtrClass <MonitorControl, SlavableAutomationControl> ("MonitorControl")
		.addFunction ("monitoring_choice", &MonitorControl::monitoring_choice)
		.endClass ()

		.deriveWSPtrClass <SoloControl, SlavableAutomationControl> ("SoloControl")
		.addFunction ("can_solo", &SoloControl::can_solo)
		.addFunction ("soloed", &SoloControl::soloed)
		.addFunction ("self_soloed", &SoloControl::self_soloed)
		.endClass ()

		.deriveWSPtrClass <MuteControl, SlavableAutomationControl> ("MuteControl")
		.addFunction ("muted", &MuteControl::muted)
		.addFunction ("muted_by_self", &MuteControl::muted_by_self)
		.endClass ()

		.deriveWSPtrClass <SoloIsolateControl, SlavableAutomationControl> ("SoloIsolateControl")
		.addFunction ("solo_isolated", &SoloIsolateControl::solo_isolated)
		.addFunction ("self_solo_isolated", &SoloIsolateControl::self_solo_isolated)
		.endClass ()

		.deriveWSPtrClass <SoloSafeControl, SlavableAutomationControl> ("SoloSafeControl")
		.addFunction ("solo_safe", &SoloSafeControl::solo_safe)
		.endClass ()

		.deriveWSPtrClass <Amp, Processor> ("Amp")
		.addFunction ("gain_control", (boost::shared_ptr<GainControl>(Amp::*)())&Amp::gain_control)
		.addStaticFunction ("apply_gain", static_cast<gain_t (*)(AudioBuffer&, samplecnt_t, samplecnt_t, gain_t, gain_t, sampleoffset_t)>(&Amp::apply_gain))
		.endClass ()

		.deriveWSPtrClass <PeakMeter, Processor> ("PeakMeter")
		.addFunction ("meter_level", &PeakMeter::meter_level)
		.addFunction ("set_meter_type", &PeakMeter::set_meter_type)
		.addFunction ("meter_type", &PeakMeter::meter_type)
		.addFunction ("reset_max", &PeakMeter::reset_max)
		.endClass ()

		.deriveWSPtrClass <MonitorProcessor, Processor> ("MonitorProcessor")
		.addFunction ("set_cut_all", &MonitorProcessor::set_cut_all)
		.addFunction ("set_dim_all", &MonitorProcessor::set_dim_all)
		.addFunction ("set_polarity", &MonitorProcessor::set_polarity)
		.addFunction ("set_cut", &MonitorProcessor::set_cut)
		.addFunction ("set_dim", &MonitorProcessor::set_dim)
		.addFunction ("set_solo", &MonitorProcessor::set_solo)
		.addFunction ("set_mono", &MonitorProcessor::set_mono)
		.addFunction ("dim_level", &MonitorProcessor::dim_level)
		.addFunction ("solo_boost_level", &MonitorProcessor::solo_boost_level)
		.addFunction ("dimmed", &MonitorProcessor::dimmed)
		.addFunction ("soloed", &MonitorProcessor::soloed)
		.addFunction ("inverted", &MonitorProcessor::inverted)
		.addFunction ("cut", &MonitorProcessor::cut)
		.addFunction ("cut_all", &MonitorProcessor::cut_all)
		.addFunction ("dim_all", &MonitorProcessor::dim_all)
		.addFunction ("mono", &MonitorProcessor::mono)
		.addFunction ("monitor_active", &MonitorProcessor::monitor_active)
		.addFunction ("channel_cut_control", &MonitorProcessor::channel_cut_control)
		.addFunction ("channel_dim_control", &MonitorProcessor::channel_dim_control)
		.addFunction ("channel_polarity_control", &MonitorProcessor::channel_polarity_control)
		.addFunction ("channel_solo_control", &MonitorProcessor::channel_solo_control)
		.addFunction ("dim_control", &MonitorProcessor::dim_control)
		.addFunction ("cut_control", &MonitorProcessor::cut_control)
		.addFunction ("mono_control", &MonitorProcessor::mono_control)
		.addFunction ("dim_level_control", &MonitorProcessor::dim_level_control)
		.addFunction ("solo_boost_control", &MonitorProcessor::solo_boost_control)
		.endClass ()

		.deriveWSPtrClass <UnknownProcessor, Processor> ("UnknownProcessor")
		.endClass ()

		.deriveWSPtrClass <PolarityProcessor, Processor> ("PolarityProcessor")
		.endClass ()

		.deriveWSPtrClass <DelayLine, Processor> ("DelayLine")
		.addFunction ("delay", &DelayLine::delay)
		.endClass ()

		.deriveWSPtrClass <PluginInsert::PluginControl, AutomationControl> ("PluginControl")
		.endClass ()

		.beginClass <RawMidiParser> ("RawMidiParser")
		.addVoidConstructor ()
		.addFunction ("reset", &RawMidiParser::reset)
		.addFunction ("process_byte", &RawMidiParser::process_byte)
		.addFunction ("buffer_size", &RawMidiParser::buffer_size)
		.addFunction ("midi_buffer", &RawMidiParser::midi_buffer)
		.endClass ()

		.deriveWSPtrClass <AudioSource, Source> ("AudioSource")
		.addFunction ("readable_length", &AudioSource::readable_length_samples)
		.addFunction ("n_channels", &AudioSource::n_channels)
		.endClass ()

		// <std::list<boost::shared_ptr <AudioTrack> >
		.beginStdList <boost::shared_ptr<AudioTrack> > ("AudioTrackList")
		.endClass ()

		.beginStdList <TimelineRange> ("TimelineRangeList")
		.endClass ()

		// std::list<boost::shared_ptr <MidiTrack> >
		.beginStdList <boost::shared_ptr<MidiTrack> > ("MidiTrackList")
		.endClass ()

		// RouteList == std::list<boost::shared_ptr<Route> >
		.beginConstStdList <boost::shared_ptr<Route> > ("RouteList")
		.endClass ()

		// StripableList == std::list<boost::shared_ptr<Stripable> >
		.beginConstStdList <boost::shared_ptr<Stripable> > ("StripableList")
		.endClass ()

		// VCAList == std::list<boost::shared_ptr<VCA> >
		.beginConstStdList <boost::shared_ptr<VCA> > ("VCAList")
		.endClass ()

		// VCAVector == std::vector<boost::shared_ptr<VCA> >
		.beginConstStdVector <boost::shared_ptr<VCA> > ("VCAVector")
		.endClass ()

		// boost::shared_ptr<RouteList>
		.beginPtrStdList <boost::shared_ptr<Route> > ("RouteListPtr")
		.addVoidPtrConstructor<std::list<boost::shared_ptr <Route> > > ()
		.endClass ()

		// boost::shared_ptr<BundleList>
		.beginPtrStdVector <boost::shared_ptr<Bundle> > ("BundleListPtr")
		.addVoidPtrConstructor<std::vector<boost::shared_ptr <Bundle> > > ()
		.endClass ()

		// typedef std::list<boost::weak_ptr <Route> > WeakRouteList
		.beginConstStdList <boost::weak_ptr<Route> > ("WeakRouteList")
		.endClass ()

		// RouteGroupList == std::list<RouteGroup*>
		.beginConstStdCPtrList <RouteGroup> ("RouteGroupList")
		.endClass ()

		// typedef std::vector<boost::shared_ptr<Source> > Region::SourceList
		.beginStdVector <boost::shared_ptr<Source> > ("SourceList")
		.endClass ()

		// typedef std::vector<boost::shared_ptr<AudioReadable> >
		.beginStdVector <boost::shared_ptr<AudioReadable> > ("ReadableList")
		.endClass ()

		// from SessionPlaylists: std::vector<boost::shared_ptr<Playlist > >
		.beginStdVector <boost::shared_ptr<Playlist> > ("PlaylistList")
		.endClass ()

		// std::list< boost::weak_ptr <AudioSource> >
		.beginConstStdList <boost::weak_ptr<AudioSource> > ("WeakAudioSourceList")
		.endClass ()

		// typedef std::vector<boost::shared_ptr<Region> > RegionVector
		.beginStdVector <boost::shared_ptr<Region> > ("RegionVector")
		.endClass ()

		// typedef std::vector<samplepos_t> XrunPositions
		.beginStdVector <samplepos_t> ("XrunPositions")
		.endClass ()

		// typedef std::list<boost::shared_ptr<Region> > RegionList
		.beginConstStdList <boost::shared_ptr<Region> > ("RegionList")
		.endClass ()

		// boost::shared_ptr <std::list<boost::shared_ptr<Region> > >
		.beginPtrStdList <boost::shared_ptr<Region> > ("RegionListPtr")
		.addVoidPtrConstructor<std::list<boost::shared_ptr <Region> > > ()
		.endClass ()

		// RegionFactory::RegionMap
		.beginStdMap <PBD::ID,boost::shared_ptr<Region> > ("RegionMap")
		.endClass ()

		// typedef std::map<std::string, DPM> PortManager::AudioPortMeters;
		.beginStdMap <std::string, PortManager::DPM> ("AudioPortMeters")
		.endClass ()

		// typedef std::map<std::string, MPM> PortManager::MIDIPortMeters;
		.beginStdMap <std::string, PortManager::MPM> ("MIDIPortMeters")
		.endClass ()

		// typedef std::list<boost::shared_ptr<Processor> > ProcessorList
		.beginStdList <boost::shared_ptr<Processor> > ("ProcessorList")
		.endClass ()

		//std::list<boost::shared_ptr<Port> > PortList
		.beginConstStdList <boost::shared_ptr<Port> > ("PortList")
		.endClass ()

		.beginConstStdCPtrList <Location> ("LocationList")
		.endClass ()

		.beginConstStdVector <Evoral::Parameter> ("ParameterList")
		.endClass ()

		.beginStdList <boost::shared_ptr<AutomationControl> > ("ControlList")
		.endClass ()

		.beginPtrStdList <boost::shared_ptr<AutomationControl> > ("ControlListPtr")
		.addVoidPtrConstructor<std::list<boost::shared_ptr <AutomationControl> > > ()
		.endClass ()

		.beginStdList <boost::shared_ptr<Evoral::Note<Temporal::Beats> > > ("NotePtrList")
		.endClass ()

		.beginConstStdCPtrList <Evoral::ControlEvent> ("EventList")
		.endClass ()

#if 0  // depends on Evoal:: Note, Beats see note_fixer.h
	// typedef Evoral::Note<Temporal::Beats> Note
	// std::set< boost::weak_ptr<Note> >
		.beginStdSet <boost::weak_ptr<Note> > ("WeakNoteSet")
		.endClass ()
#endif

	// std::list<boost::weak_ptr<Source> >
		.beginConstStdList <boost::weak_ptr<Source> > ("WeakSourceList")
		.endClass ()

		.beginClass <ChanCount> ("ChanCount")
		.addConstructor <void (*) (DataType, uint32_t)> ()
		.addFunction ("get", &ChanCount::get)
		.addFunction ("set", &ChanCount::set)
		.addFunction ("set_audio", &ChanCount::set_audio)
		.addFunction ("set_midi", &ChanCount::set_midi)
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
		.addFunction ("name", &PluginManager::plugin_type_name)
		.addConst ("AudioUnit", ARDOUR::PluginType(AudioUnit))
		.addConst ("LADSPA", ARDOUR::PluginType(LADSPA))
		.addConst ("LV2", ARDOUR::PluginType(LV2))
		.addConst ("Windows_VST", ARDOUR::PluginType(Windows_VST))
		.addConst ("LXVST", ARDOUR::PluginType(LXVST))
		.addConst ("MacVST", ARDOUR::PluginType(MacVST))
		.addConst ("Lua", ARDOUR::PluginType(Lua))
		.addConst ("VST3", ARDOUR::PluginType(VST3))
		.endNamespace ()

		.beginNamespace ("PresentationInfo")
		.beginNamespace ("Flag")
		.addConst ("AudioTrack", ARDOUR::PresentationInfo::Flag(PresentationInfo::AudioTrack))
		.addConst ("MidiTrack", ARDOUR::PresentationInfo::Flag(PresentationInfo::MidiTrack))
		.addConst ("AudioBus", ARDOUR::PresentationInfo::Flag(PresentationInfo::AudioBus))
		.addConst ("MidiBus", ARDOUR::PresentationInfo::Flag(PresentationInfo::MidiBus))
		.addConst ("VCA", ARDOUR::PresentationInfo::Flag(PresentationInfo::VCA))
		.addConst ("MasterOut", ARDOUR::PresentationInfo::Flag(PresentationInfo::MasterOut))
		.addConst ("MonitorOut", ARDOUR::PresentationInfo::Flag(PresentationInfo::MonitorOut))
		.addConst ("Auditioner", ARDOUR::PresentationInfo::Flag(PresentationInfo::Auditioner))
		.addConst ("Hidden", ARDOUR::PresentationInfo::Flag(PresentationInfo::Hidden))
		.addConst ("GroupOrderSet", ARDOUR::PresentationInfo::Flag(PresentationInfo::OrderSet))
		.addConst ("TriggerTrack", ARDOUR::PresentationInfo::Flag(PresentationInfo::TriggerTrack))
		.addConst ("StatusMask", ARDOUR::PresentationInfo::Flag(PresentationInfo::StatusMask))
		.addConst ("TypeMask", ARDOUR::PresentationInfo::Flag(PresentationInfo::TypeMask))
		.endNamespace ()
		.endNamespace ()

		.beginNamespace ("AutoState")
		.addConst ("Off", ARDOUR::AutoState(Off))
		.addConst ("Write", ARDOUR::AutoState(Write))
		.addConst ("Touch", ARDOUR::AutoState(Touch))
		.addConst ("Play", ARDOUR::AutoState(Play))
		.addConst ("Latch", ARDOUR::AutoState(Latch))
		.endNamespace ()

		.beginNamespace ("AutomationType")
		.addConst ("GainAutomation", ARDOUR::AutomationType(GainAutomation))
		.addConst ("BusSendLevel", ARDOUR::AutomationType(BusSendLevel))
		.addConst ("PluginAutomation", ARDOUR::AutomationType(PluginAutomation))
		.addConst ("SoloAutomation", ARDOUR::AutomationType(SoloAutomation))
		.addConst ("SoloIsolateAutomation", ARDOUR::AutomationType(SoloIsolateAutomation))
		.addConst ("SoloSafeAutomation", ARDOUR::AutomationType(SoloSafeAutomation))
		.addConst ("MuteAutomation", ARDOUR::AutomationType(MuteAutomation))
		.addConst ("RecEnableAutomation", ARDOUR::AutomationType(RecEnableAutomation))
		.addConst ("RecSafeAutomation", ARDOUR::AutomationType(RecSafeAutomation))
		.addConst ("TrimAutomation", ARDOUR::AutomationType(TrimAutomation))
		.addConst ("PhaseAutomation", ARDOUR::AutomationType(PhaseAutomation))
		.addConst ("MidiCCAutomation", ARDOUR::AutomationType(MidiCCAutomation))
		.addConst ("MidiPgmChangeAutomation", ARDOUR::AutomationType(MidiPgmChangeAutomation))
		.addConst ("MidiPitchBenderAutomation", ARDOUR::AutomationType(MidiPitchBenderAutomation))
		.addConst ("MidiChannelPressureAutomation", ARDOUR::AutomationType(MidiChannelPressureAutomation))
		.addConst ("MidiNotePressureAutomation", ARDOUR::AutomationType(MidiNotePressureAutomation))
		.addConst ("MidiSystemExclusiveAutomation", ARDOUR::AutomationType(MidiSystemExclusiveAutomation))
		.endNamespace ()

		.beginNamespace ("SrcQuality")
		.addConst ("SrcBest", ARDOUR::SrcQuality(SrcBest))
		.endNamespace ()

		.beginNamespace ("MeterType")
		.addConst ("MeterMaxSignal", ARDOUR::MeterType(MeterMaxSignal))
		.addConst ("MeterMaxPeak", ARDOUR::MeterType(MeterMaxPeak))
		.addConst ("MeterPeak", ARDOUR::MeterType(MeterPeak))
		.addConst ("MeterKrms", ARDOUR::MeterType(MeterKrms))
		.addConst ("MeterK20", ARDOUR::MeterType(MeterK20))
		.addConst ("MeterK14", ARDOUR::MeterType(MeterK14))
		.addConst ("MeterIEC1DIN", ARDOUR::MeterType(MeterIEC1DIN))
		.addConst ("MeterIEC1NOR", ARDOUR::MeterType(MeterIEC1NOR))
		.addConst ("MeterIEC2BBC", ARDOUR::MeterType(MeterIEC2BBC))
		.addConst ("MeterIEC2EBU", ARDOUR::MeterType(MeterIEC2EBU))
		.addConst ("MeterVU", ARDOUR::MeterType(MeterVU))
		.addConst ("MeterK12", ARDOUR::MeterType(MeterK12))
		.addConst ("MeterPeak0dB", ARDOUR::MeterType(MeterPeak0dB))
		.addConst ("MeterMCP", ARDOUR::MeterType(MeterMCP))
		.endNamespace ()

		.beginNamespace ("MeterPoint")
		.addConst ("MeterInput", ARDOUR::MeterPoint(MeterInput))
		.addConst ("MeterPreFader", ARDOUR::MeterPoint(MeterPreFader))
		.addConst ("MeterPostFader", ARDOUR::MeterPoint(MeterPostFader))
		.addConst ("MeterOutput", ARDOUR::MeterPoint(MeterOutput))
		.addConst ("MeterCustom", ARDOUR::MeterPoint(MeterCustom))
		.endNamespace ()

		.beginNamespace ("Placement")
		.addConst ("PreFader", ARDOUR::Placement(PreFader))
		.addConst ("PostFader", ARDOUR::Placement(PostFader))
		.endNamespace ()

		.beginNamespace ("MonitorChoice")
		.addConst ("MonitorAuto", ARDOUR::MonitorChoice(MonitorAuto))
		.addConst ("MonitorInput", ARDOUR::MonitorChoice(MonitorInput))
		.addConst ("MonitorDisk", ARDOUR::MonitorChoice(MonitorDisk))
		.addConst ("MonitorCue", ARDOUR::MonitorChoice(MonitorCue))
		.endNamespace ()

		.beginNamespace ("MonitorState")
		.addConst ("MonitoringSilence", ARDOUR::MonitorState(MonitoringSilence))
		.addConst ("MonitoringInput", ARDOUR::MonitorState(MonitoringInput))
		.addConst ("MonitoringDisk", ARDOUR::MonitorState(MonitoringDisk))
		.addConst ("MonitoringCue", ARDOUR::MonitorState(MonitoringCue))
		.endNamespace ()

		.beginNamespace ("NoteMode")
		.addConst ("Sustained", ARDOUR::NoteMode(Sustained))
		.addConst ("Percussive", ARDOUR::NoteMode(Percussive))
		.endNamespace ()

		.beginNamespace ("PortFlags")
		.addConst ("IsInput", ARDOUR::PortFlags(IsInput))
		.addConst ("IsOutput", ARDOUR::PortFlags(IsOutput))
		.addConst ("IsPhysical", ARDOUR::PortFlags(IsPhysical))
		.addConst ("CanMonitor", ARDOUR::PortFlags(CanMonitor))
		.addConst ("IsTerminal", ARDOUR::PortFlags(IsTerminal))
		.endNamespace ()

		.beginNamespace ("MidiPortFlags")
		.addConst ("MidiPortMusic", ARDOUR::MidiPortFlags(MidiPortMusic))
		.addConst ("MidiPortControl", ARDOUR::MidiPortFlags(MidiPortControl))
		.addConst ("MidiPortSelection", ARDOUR::MidiPortFlags(MidiPortSelection))
		.addConst ("MidiPortVirtual", ARDOUR::MidiPortFlags(MidiPortVirtual))
		.endNamespace ()

		.beginNamespace ("PlaylistDisposition")
		.addConst ("CopyPlaylist", ARDOUR::PlaylistDisposition(CopyPlaylist))
		.addConst ("NewPlaylist", ARDOUR::PlaylistDisposition(NewPlaylist))
		.addConst ("SharePlaylist", ARDOUR::PlaylistDisposition(SharePlaylist))
		.endNamespace ()

		.beginNamespace ("MidiTrackNameSource")
		.addConst ("SMFTrackNumber", ARDOUR::MidiTrackNameSource(SMFTrackNumber))
		.addConst ("SMFTrackName", ARDOUR::MidiTrackNameSource(SMFTrackName))
		.addConst ("SMFInstrumentName", ARDOUR::MidiTrackNameSource(SMFInstrumentName))
		.endNamespace ()

		.beginNamespace ("MidiTempoMapDisposition")
		.addConst ("SMFTempoIgnore", ARDOUR::MidiTempoMapDisposition(SMFTempoIgnore))
		.addConst ("SMFTempoUse", ARDOUR::MidiTempoMapDisposition(SMFTempoUse))
		.endNamespace ()

		.beginNamespace ("RegionEquivalence")
		.addConst ("Exact", ARDOUR::RegionEquivalence(Exact))
		.addConst ("Enclosed", ARDOUR::RegionEquivalence(Enclosed))
		.addConst ("Overlap", ARDOUR::RegionEquivalence(Overlap))
		.addConst ("LayerTime", ARDOUR::RegionEquivalence(LayerTime))
		.endNamespace ()

		.beginNamespace ("RegionPoint")
		.addConst ("Start", ARDOUR::RegionPoint(Start))
		.addConst ("End", ARDOUR::RegionPoint(End))
		.addConst ("SyncPoint", ARDOUR::RegionPoint(SyncPoint))
		.endNamespace ()

		.beginNamespace ("TrackMode")
		.addConst ("Normal", ARDOUR::TrackMode(Start))
		.addConst ("NonLayered", ARDOUR::TrackMode(NonLayered))
		.endNamespace ()

		.beginNamespace ("TransportRequestSource")
		.addConst ("TRS_Engine", ARDOUR::TransportRequestSource(TRS_Engine))
		.addConst ("TRS_UI", ARDOUR::TransportRequestSource(TRS_UI))
		.endNamespace ()

		.beginNamespace ("LocateTransportDisposition")
		.addConst ("MustRoll", ARDOUR::LocateTransportDisposition(MustRoll))
		.addConst ("MustStop", ARDOUR::LocateTransportDisposition(MustStop))
		.addConst ("RollIfAppropriate", ARDOUR::LocateTransportDisposition(RollIfAppropriate))
		.endNamespace ()

		.beginNamespace ("SampleFormat")
		.addConst ("Float", ARDOUR::SampleFormat(FormatFloat))
		.addConst ("Int24", ARDOUR::SampleFormat(FormatInt24))
		.addConst ("Int16", ARDOUR::SampleFormat(FormatInt16))
		.endNamespace ()

		.beginNamespace ("HeaderFormat")
		.addConst ("BWF", ARDOUR::HeaderFormat(BWF))
		.addConst ("WAVE", ARDOUR::HeaderFormat(WAVE))
		.addConst ("WAVE64", ARDOUR::HeaderFormat(WAVE64))
		.addConst ("CAF", ARDOUR::HeaderFormat(CAF))
		.addConst ("AIFF", ARDOUR::HeaderFormat(AIFF))
		.addConst ("iXML", ARDOUR::HeaderFormat(iXML))
		.addConst ("RF64", ARDOUR::HeaderFormat(RF64))
		.addConst ("RF64_WAV", ARDOUR::HeaderFormat(RF64_WAV))
		.addConst ("MBWF", ARDOUR::HeaderFormat(MBWF))
		.addConst ("FLAC", ARDOUR::HeaderFormat(FLAC))
		.endNamespace ()

		.beginNamespace ("InsertMergePolicy")
		.addConst ("Reject", ARDOUR::InsertMergePolicy(InsertMergeReject))
		.addConst ("Relax", ARDOUR::InsertMergePolicy(InsertMergeRelax))
		.addConst ("Replace", ARDOUR::InsertMergePolicy(InsertMergeReplace))
		.addConst ("TruncateExisting", ARDOUR::InsertMergePolicy(InsertMergeTruncateExisting))
		.addConst ("TruncateAddition", ARDOUR::InsertMergePolicy(InsertMergeTruncateAddition))
		.addConst ("Extend", ARDOUR::InsertMergePolicy(InsertMergeExtend))
		.endNamespace ()

		.beginNamespace ("AFLPosition")
		.addConst ("AFLFromBeforeProcessors", ARDOUR::AFLPosition(AFLFromBeforeProcessors))
		.addConst ("AFLFromAfterProcessors", ARDOUR::AFLPosition(AFLFromAfterProcessors))
		.endNamespace ()

		.beginNamespace ("PFLPosition")
		.addConst ("PFLFromBeforeProcessors", ARDOUR::PFLPosition(PFLFromBeforeProcessors))
		.addConst ("PFLFromAfterProcessors", ARDOUR::PFLPosition(PFLFromAfterProcessors))
		.endNamespace ()

		.beginNamespace ("AutoReturnTarget")
		.addConst ("LastLocate", ARDOUR::AutoReturnTarget(LastLocate))
		.addConst ("RangeSelectionStart", ARDOUR::AutoReturnTarget(RangeSelectionStart))
		.addConst ("Loop", ARDOUR::AutoReturnTarget(Loop))
		.addConst ("RegionSelectionStart", ARDOUR::AutoReturnTarget(RegionSelectionStart))
		.endNamespace ()

		.beginNamespace ("FadeShape")
		.addConst ("FadeLinear", ARDOUR::FadeShape(FadeLinear))
		.addConst ("FadeFast", ARDOUR::FadeShape(FadeFast))
		.addConst ("FadeSlow", ARDOUR::FadeShape(FadeSlow))
		.addConst ("FadeConstantPower", ARDOUR::FadeShape(FadeConstantPower))
		.addConst ("FadeSymmetric", ARDOUR::FadeShape(FadeSymmetric))
		.endNamespace ()

		.beginNamespace ("LoopFadeChoice")
		.addConst ("NoLoopFade", ARDOUR::LoopFadeChoice(NoLoopFade))
		.addConst ("EndLoopFade", ARDOUR::LoopFadeChoice(EndLoopFade))
		.addConst ("BothLoopFade", ARDOUR::LoopFadeChoice(BothLoopFade))
		.addConst ("XFadeLoop", ARDOUR::LoopFadeChoice(XFadeLoop))
		.endNamespace ()

		.beginNamespace ("DenormalModel")
		.addConst ("DenormalNone", ARDOUR::DenormalModel(DenormalNone))
		.addConst ("DenormalFTZ", ARDOUR::DenormalModel(DenormalFTZ))
		.addConst ("DenormalDAZ", ARDOUR::DenormalModel(DenormalDAZ))
		.addConst ("DenormalFTZDAZ", ARDOUR::DenormalModel(DenormalFTZDAZ))
		.endNamespace ()

		.beginNamespace ("BufferingPreset")
		.addConst ("Small", ARDOUR::BufferingPreset(Small))
		.addConst ("Medium", ARDOUR::BufferingPreset(Medium))
		.addConst ("Large", ARDOUR::BufferingPreset(Large))
		.addConst ("Custom", ARDOUR::BufferingPreset(Custom))
		.endNamespace ()

		.beginNamespace ("EditMode")
		.addConst ("Slide", ARDOUR::EditMode(Slide))
		.addConst ("Ripple", ARDOUR::EditMode(Ripple))
		.addConst ("Lock", ARDOUR::EditMode(Lock))
		.endNamespace ()

		.beginNamespace ("AutoConnectOption")
		.addConst ("ManualConnect", ARDOUR::AutoConnectOption(ManualConnect))
		.addConst ("AutoConnectPhysical", ARDOUR::AutoConnectOption(AutoConnectPhysical))
		.addConst ("AutoConnectMaster", ARDOUR::AutoConnectOption(AutoConnectMaster))
		.endNamespace ()

		.beginNamespace ("LayerModel")
		.addConst ("LaterHigher", ARDOUR::LayerModel(LaterHigher))
		.addConst ("Manual", ARDOUR::LayerModel(Manual))
		.endNamespace ()

		.beginNamespace ("ListenPosition")
		.addConst ("AfterFaderListen", ARDOUR::ListenPosition(AfterFaderListen))
		.addConst ("PreFaderListen", ARDOUR::ListenPosition(PreFaderListen))
		.endNamespace ()

		.beginNamespace ("MonitorModel")
		.addConst ("HardwareMonitoring", ARDOUR::MonitorModel(HardwareMonitoring))
		.addConst ("SoftwareMonitoring", ARDOUR::MonitorModel(SoftwareMonitoring))
		.addConst ("ExternalMonitoring", ARDOUR::MonitorModel(ExternalMonitoring))
		.endNamespace ()

		.beginNamespace ("RegionSelectionAfterSplit")
		.addConst ("None", ARDOUR::RegionSelectionAfterSplit(None))
		.addConst ("NewlyCreatedLeft", ARDOUR::RegionSelectionAfterSplit(NewlyCreatedLeft))
		.addConst ("NewlyCreatedRight", ARDOUR::RegionSelectionAfterSplit(NewlyCreatedRight))
		.addConst ("NewlyCreatedBoth", ARDOUR::RegionSelectionAfterSplit(NewlyCreatedBoth))
		.addConst ("Existing", ARDOUR::RegionSelectionAfterSplit(Existing))
		.addConst ("ExistingNewlyCreatedLeft", ARDOUR::RegionSelectionAfterSplit(ExistingNewlyCreatedLeft))
		.addConst ("ExistingNewlyCreatedRight", ARDOUR::RegionSelectionAfterSplit(ExistingNewlyCreatedRight))
		.addConst ("ExistingNewlyCreatedBoth", ARDOUR::RegionSelectionAfterSplit(ExistingNewlyCreatedBoth))
		.endNamespace ()

		.beginNamespace ("RangeSelectionAfterSplit")
		.addConst ("ClearSel", ARDOUR::RangeSelectionAfterSplit(ClearSel))
		.addConst ("PreserveSel", ARDOUR::RangeSelectionAfterSplit(PreserveSel))
		.addConst ("ForceSel", ARDOUR::RangeSelectionAfterSplit(ForceSel))
		.endNamespace ()

		.beginNamespace ("ScreenSaverMode")
		.addConst ("InhibitNever", ARDOUR::ScreenSaverMode(InhibitNever))
		.addConst ("InhibitWhileRecording", ARDOUR::ScreenSaverMode(InhibitWhileRecording))
		.addConst ("InhibitAlways", ARDOUR::ScreenSaverMode(InhibitAlways))
		.endNamespace ()

		.beginNamespace ("ClockDeltaMode")
		.addConst ("NoDelta", ARDOUR::ClockDeltaMode(NoDelta))
		.addConst ("DeltaEditPoint", ARDOUR::ClockDeltaMode(DeltaEditPoint))
		.addConst ("DeltaOriginMarker", ARDOUR::ClockDeltaMode(DeltaOriginMarker))
		.endNamespace ()

		.beginNamespace ("WaveformScale")
		.addConst ("Linear", ARDOUR::WaveformScale(Linear))
		.addConst ("Logarithmic", ARDOUR::WaveformScale(Logarithmic))
		.endNamespace ()

		.beginNamespace ("WaveformShape")
		.addConst ("Traditional", ARDOUR::WaveformShape(Traditional))
		.addConst ("Rectified", ARDOUR::WaveformShape(Rectified))
		.endNamespace ()

		.beginNamespace ("MeterLineUp")
		.addConst ("MeteringLineUp24", ARDOUR::MeterLineUp(MeteringLineUp24))
		.addConst ("MeteringLineUp20", ARDOUR::MeterLineUp(MeteringLineUp20))
		.addConst ("MeteringLineUp18", ARDOUR::MeterLineUp(MeteringLineUp18))
		.addConst ("MeteringLineUp15", ARDOUR::MeterLineUp(MeteringLineUp15))
		.endNamespace ()

		.beginNamespace ("InputMeterLayout")
		.addConst ("LayoutVertical", ARDOUR::InputMeterLayout(LayoutVertical))
		.addConst ("LayoutHorizontal", ARDOUR::InputMeterLayout(LayoutHorizontal))
		.addConst ("LayoutAutomatic", ARDOUR::InputMeterLayout(LayoutAutomatic))
		.addConst ("MeteringLineUp15", ARDOUR::MeterLineUp(MeteringLineUp15))
		.endNamespace ()

		.beginNamespace ("VUMeterStandard")
		.addConst ("MeteringVUfrench", ARDOUR::VUMeterStandard(MeteringVUfrench))
		.addConst ("MeteringVUamerican", ARDOUR::VUMeterStandard(MeteringVUamerican))
		.addConst ("MeteringVUstandard", ARDOUR::VUMeterStandard(MeteringVUstandard))
		.addConst ("MeteringVUeight", ARDOUR::VUMeterStandard(MeteringVUeight))
		.endNamespace ()

		.beginNamespace ("ShuttleUnits")
		.addConst ("Percentage", ARDOUR::ShuttleUnits(Percentage))
		.addConst ("Semitones", ARDOUR::ShuttleUnits(Semitones))
		.endNamespace ()

		.beginNamespace ("SyncSource")
		.addConst ("Engine", ARDOUR::SyncSource(Engine))
		.addConst ("MTC", ARDOUR::SyncSource(MTC))
		.addConst ("MIDIClock", ARDOUR::SyncSource(MIDIClock))
		.addConst ("LTC", ARDOUR::SyncSource(LTC))
		.endNamespace ()

		.beginNamespace ("TracksAutoNamingRule")
		.addConst ("UseDefaultNames", ARDOUR::TracksAutoNamingRule(UseDefaultNames))
		.addConst ("NameAfterDriver", ARDOUR::TracksAutoNamingRule(NameAfterDriver))
		.endNamespace ()

		.endNamespace (); // end ARDOUR

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.addFunction ("user_config_directory", &ARDOUR::user_config_directory)
		.addFunction ("user_cache_directory", &ARDOUR::user_cache_directory)
		.endNamespace (); // end ARDOUR

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginClass <AudioBackendInfo> ("AudioBackendInfo")
		.addData ("name", &AudioBackendInfo::name)
		.endClass()
		.beginConstStdVector <const AudioBackendInfo*> ("BackendVector").endClass ()

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

		.beginClass <PortEngine> ("PortEngine")
		.endClass()

		.beginClass <LatencyRange> ("LatencyRange")
		.addVoidConstructor ()
		.addData ("min", &LatencyRange::min)
		.addData ("max", &LatencyRange::max)
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
		.addFunction ("reset_input_meters", &PortManager::reset_input_meters)
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
		.addFunction ("freewheeling", &AudioEngine::freewheeling)
		.addFunction ("running", &AudioEngine::running)
		.addFunction ("processed_samples", &AudioEngine::processed_samples)
		.endClass()

		.deriveClass <VCAManager, PBD::StatefulDestructible> ("VCAManager")
		.addFunction ("create_vca", &VCAManager::create_vca)
		.addFunction ("remove_vca", &VCAManager::remove_vca)
		.addFunction ("vca_by_number", &VCAManager::vca_by_number)
		.addFunction ("vca_by_name", &VCAManager::vca_by_name)
		.addFunction ("vcas", &VCAManager::vcas)
		.addFunction ("n_vcas", &VCAManager::n_vcas)
		.endClass()

		.deriveClass <RCConfiguration, PBD::Configuration> ("RCConfiguration")
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(Type,var,name,value) \
		.addFunction ("get_" # var, &RCConfiguration::get_##var) \
		.addFunction ("set_" # var, &RCConfiguration::set_##var) \
		.addProperty (#var, &RCConfiguration::get_##var, &RCConfiguration::set_##var)

#define CONFIG_VARIABLE_SPECIAL(Type,var,name,value,mutator) \
		.addFunction ("get_" # var, &RCConfiguration::get_##var) \
		.addFunction ("set_" # var, &RCConfiguration::set_##var) \
		.addProperty (#var, &RCConfiguration::get_##var, &RCConfiguration::set_##var)

#include "ardour/rc_configuration_vars.h"

#undef CONFIG_VARIABLE
#undef CONFIG_VARIABLE_SPECIAL
		.endClass()

		.deriveClass <SessionConfiguration, PBD::Configuration> ("SessionConfiguration")
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(Type,var,name,value) \
		.addFunction ("get_" # var, &SessionConfiguration::get_##var) \
		.addFunction ("set_" # var, &SessionConfiguration::set_##var) \
		.addProperty (#var, &SessionConfiguration::get_##var, &SessionConfiguration::set_##var)

#define CONFIG_VARIABLE_SPECIAL(Type,var,name,value,mutator) \
		.addFunction ("get_" # var, &SessionConfiguration::get_##var) \
		.addFunction ("set_" # var, &SessionConfiguration::set_##var) \
		.addProperty (#var, &SessionConfiguration::get_##var, &SessionConfiguration::set_##var)

#include "ardour/session_configuration_vars.h"

#undef CONFIG_VARIABLE
#undef CONFIG_VARIABLE_SPECIAL
		.endClass()

		// we could use addProperty ()
		.addFunction ("config", &_libardour_config)

		.endNamespace ();

	// basic representation of Session
	// functions which can be used from realtime and non-realtime contexts
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginClass <Session> ("Session")
		.addFunction ("scripts_changed", &Session::scripts_changed) // used internally
		.addFunction ("engine_speed", &Session::engine_speed)
		.addFunction ("actual_speed", &Session::actual_speed)
		.addFunction ("transport_speed", &Session::transport_speed)
		.addFunction ("transport_stopped", &Session::transport_stopped)
		.addFunction ("transport_stopped_or_stopping", &Session::transport_stopped_or_stopping)
		.addFunction ("transport_state_rolling", &Session::transport_state_rolling)
		.addFunction ("transport_rolling", &Session::transport_rolling)
		.addFunction ("transport_will_roll_forwards", &Session::transport_will_roll_forwards)
		.addFunction ("request_transport_speed", &Session::request_transport_speed)
		.addFunction ("transport_sample", &Session::transport_sample)
		.addFunction ("sample_rate", &Session::sample_rate)
		.addFunction ("nominal_sample_rate", &Session::nominal_sample_rate)
		.addFunction ("samples_per_timecode_frame", &Session::samples_per_timecode_frame)
		.addFunction ("timecode_frames_per_hour", &Session::timecode_frames_per_hour)
		.addFunction ("timecode_frames_per_second", &Session::timecode_frames_per_second)
		.addFunction ("timecode_drop_frames", &Session::timecode_drop_frames)
		.addFunction ("request_locate", &Session::request_locate)
		.addFunction ("request_roll", &Session::request_roll)
		.addFunction ("request_stop", &Session::request_stop)
		.addFunction ("request_play_loop", &Session::request_play_loop)
		.addFunction ("request_bounded_roll", &Session::request_bounded_roll)
		.addFunction ("get_play_loop", &Session::get_play_loop)
		.addFunction ("get_xrun_count", &Session::get_xrun_count)
		.addFunction ("reset_xrun_count", &Session::reset_xrun_count)
		.addFunction ("last_transport_start", &Session::last_transport_start)
		.addFunction ("goto_start", &Session::goto_start)
		.addFunction ("goto_end", &Session::goto_end)
		.addFunction ("current_start_sample", &Session::current_start_sample)
		.addFunction ("current_end_sample", &Session::current_end_sample)
		.addFunction ("actively_recording", &Session::actively_recording)
		.addFunction ("new_audio_track", &Session::new_audio_track)
		.addFunction ("new_audio_route", &Session::new_audio_route)
		.addFunction ("new_midi_track", &Session::new_midi_track)
		.addFunction ("new_midi_route", &Session::new_midi_route)

		.addFunction ("add_master_bus", &Session::add_master_bus)

		.addFunction ("get_routes", &Session::get_routes)
		.addFunction ("get_tracks", &Session::get_tracks)
		.addFunction ("get_stripables", (StripableList (Session::*)() const)&Session::get_stripables)
		.addFunction ("get_routelist", &Session::get_routelist)
		.addFunction ("plot_process_graph", &Session::plot_process_graph)

		.addFunction ("bundles", &Session::bundles)

		.addFunction ("name", &Session::name)
		.addFunction ("path", &Session::path)
		.addFunction ("record_status", &Session::record_status)
		.addFunction ("maybe_enable_record", &Session::maybe_enable_record)
		.addFunction ("disable_record", &Session::disable_record)
		.addFunction ("route_by_id", &Session::route_by_id)
		.addFunction ("route_by_name", &Session::route_by_name)
		.addFunction ("stripable_by_id", &Session::stripable_by_id)
		.addFunction ("get_remote_nth_stripable", &Session::get_remote_nth_stripable)
		.addFunction ("get_remote_nth_route", &Session::get_remote_nth_route)
		.addFunction ("route_by_selected_count", &Session::route_by_selected_count)
		.addFunction ("source_by_id", &Session::source_by_id)
		.addFunction ("controllable_by_id", &Session::controllable_by_id)
		.addFunction ("processor_by_id", &Session::processor_by_id)
		.addFunction ("snap_name", &Session::snap_name)
		.addFunction ("monitor_out", &Session::monitor_out)
		.addFunction ("master_out", &Session::master_out)
		.addFunction ("add_internal_send", (void (Session::*)(boost::shared_ptr<Route>, boost::shared_ptr<Processor>, boost::shared_ptr<Route>))&Session::add_internal_send)
		.addFunction ("add_internal_sends", &Session::add_internal_sends)
		.addFunction ("locations", &Session::locations)
		.addFunction ("soloing", &Session::soloing)
		.addFunction ("listening", &Session::listening)
		.addFunction ("solo_isolated", &Session::solo_isolated)
		.addFunction ("cancel_all_solo", &Session::cancel_all_solo)
		.addFunction ("clear_all_solo_state", &Session::clear_all_solo_state)
		.addFunction ("set_controls", &Session::set_controls)
		.addFunction ("set_control", &Session::set_control)
		.addFunction ("set_exclusive_input_active", &Session::set_exclusive_input_active)
		.addFunction ("begin_reversible_command", (void (Session::*)(const std::string&))&Session::begin_reversible_command)
		.addFunction ("commit_reversible_command", &Session::commit_reversible_command)
		.addFunction ("abort_reversible_command", &Session::abort_reversible_command)
		.addFunction ("collected_undo_commands", &Session::collected_undo_commands)
		.addFunction ("abort_empty_reversible_command", &Session::abort_empty_reversible_command)
		.addFunction ("add_command", &Session::add_command)
		.addFunction ("add_stateful_diff_command", &Session::add_stateful_diff_command)
		.addFunction ("playlists", &Session::playlists)
		.addFunction ("engine", (AudioEngine& (Session::*)())&Session::engine)
		.addFunction ("get_block_size", &Session::get_block_size)
		.addFunction ("worst_output_latency", &Session::worst_output_latency)
		.addFunction ("worst_input_latency", &Session::worst_input_latency)
		.addFunction ("worst_route_latency", &Session::worst_route_latency)
		.addFunction ("io_latency", &Session::io_latency)
		.addFunction ("worst_latency_preroll", &Session::worst_latency_preroll)
		.addFunction ("worst_latency_preroll_buffer_size_ceil", &Session::worst_latency_preroll_buffer_size_ceil)
		.addFunction ("cfg", &Session::cfg)
		.addFunction ("route_groups", &Session::route_groups)
		.addFunction ("new_route_group", &Session::new_route_group)
		.addFunction ("session_range_is_free", &Session::session_range_is_free)
		.addFunction ("set_session_range_is_free", &Session::set_session_range_is_free)
		.addFunction ("remove_route_group", (void (Session::*)(RouteGroup*))&Session::remove_route_group)
		.addFunction ("vca_manager", &Session::vca_manager_ptr)
		.addExtCFunction ("timecode_to_sample_lua", ARDOUR::LuaAPI::timecode_to_sample_lua)
		.addExtCFunction ("sample_to_timecode_lua", ARDOUR::LuaAPI::sample_to_timecode_lua)
		.endClass ()

		.beginClass <RegionFactory> ("RegionFactory")
		.addStaticFunction ("region_by_id", &RegionFactory::region_by_id)
		.addStaticFunction ("regions", &RegionFactory::regions)
		.addStaticFunction ("clone_region", static_cast<boost::shared_ptr<Region> (*)(boost::shared_ptr<Region>, bool, bool)>(&RegionFactory::create))
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
		.addConst ("IsCueMarker", ARDOUR::Location::Flags(Location::IsCueMarker))
		.addConst ("IsRangeMarker", ARDOUR::Location::Flags(Location::IsRangeMarker))
		.addConst ("IsSessionRange", ARDOUR::Location::Flags(Location::IsSessionRange))
		.addConst ("IsSkip", ARDOUR::Location::Flags(Location::IsSkip))
		.addConst ("IsSkipping", ARDOUR::Location::Flags(Location::IsSkipping))
		.endNamespace ()

		.beginNamespace ("LuaAPI")
		.addFunction ("nil_proc", ARDOUR::LuaAPI::nil_processor)
		.addFunction ("new_luaproc", ARDOUR::LuaAPI::new_luaproc)
		.addFunction ("new_send", ARDOUR::LuaAPI::new_send)
		.addFunction ("new_luaproc_with_time_domain", ARDOUR::LuaAPI::new_luaproc_with_time_domain)
		.addFunction ("list_plugins", ARDOUR::LuaAPI::list_plugins)
		.addFunction ("dump_untagged_plugins", ARDOUR::LuaAPI::dump_untagged_plugins)
		.addFunction ("new_plugin_info", ARDOUR::LuaAPI::new_plugin_info)
		.addFunction ("new_plugin", ARDOUR::LuaAPI::new_plugin)
		.addFunction ("new_plugin_with_time_domain", ARDOUR::LuaAPI::new_plugin_with_time_domain)
		.addFunction ("set_processor_param", ARDOUR::LuaAPI::set_processor_param)
		.addFunction ("set_plugin_insert_param", ARDOUR::LuaAPI::set_plugin_insert_param)
		.addFunction ("reset_processor_to_default", ARDOUR::LuaAPI::reset_processor_to_default)
		.addRefFunction ("get_processor_param", ARDOUR::LuaAPI::get_processor_param)
		.addRefFunction ("get_plugin_insert_param", ARDOUR::LuaAPI::get_plugin_insert_param)
		.addCFunction ("desc_scale_points", ARDOUR::LuaAPI::desc_scale_points)
		.addCFunction ("plugin_automation", ARDOUR::LuaAPI::plugin_automation)
		.addCFunction ("hsla_to_rgba", ARDOUR::LuaAPI::hsla_to_rgba)
		.addCFunction ("color_to_rgba", ARDOUR::LuaAPI::color_to_rgba)
		.addFunction ("ascii_dtostr", ARDOUR::LuaAPI::ascii_dtostr)
		.addFunction ("usleep", Glib::usleep)
		.addFunction ("file_test", Glib::file_test)
		.addFunction ("file_get_contents", Glib::file_get_contents)
		.addFunction ("path_get_basename", Glib::path_get_basename)
		.addFunction ("monotonic_time", ::g_get_monotonic_time)
		.addCFunction ("build_filename", ARDOUR::LuaAPI::build_filename)
		.addFunction ("new_noteptr", ARDOUR::LuaAPI::new_noteptr)
		.addFunction ("note_list", ARDOUR::LuaAPI::note_list)
		.addCFunction ("sample_to_timecode", ARDOUR::LuaAPI::sample_to_timecode)
		.addCFunction ("timecode_to_sample", ARDOUR::LuaAPI::timecode_to_sample)
		.addFunction ("wait_for_process_callback", ARDOUR::LuaAPI::wait_for_process_callback)
		.addFunction ("segfault", ARDOUR::LuaAPI::segfault)

		.beginNamespace ("FileTest")
		.addConst ("IsRegular", Glib::FILE_TEST_IS_REGULAR)
		.addConst ("IsSymlink", Glib::FILE_TEST_IS_SYMLINK)
		.addConst ("IsDir", Glib::FILE_TEST_IS_DIR)
		.addConst ("IsExecutable", Glib::FILE_TEST_IS_EXECUTABLE)
		.addConst ("Exists", Glib::FILE_TEST_EXISTS)
		.endNamespace () // end LuaAPI

		.beginClass <ARDOUR::LuaAPI::Vamp> ("Vamp")
		.addConstructor <void (*) (const std::string&, float)> ()
		.addStaticFunction ("list_plugins", &ARDOUR::LuaAPI::Vamp::list_plugins)
		.addFunction ("plugin", &ARDOUR::LuaAPI::Vamp::plugin)
		.addFunction ("analyze", &ARDOUR::LuaAPI::Vamp::analyze)
		.addFunction ("reset", &ARDOUR::LuaAPI::Vamp::reset)
		.addFunction ("initialize", &ARDOUR::LuaAPI::Vamp::initialize)
		.addFunction ("process", &ARDOUR::LuaAPI::Vamp::process)
		.endClass ()

		.beginClass <ARDOUR::LuaAPI::Rubberband> ("Rubberband")
		.addConstructor <void (*) (boost::shared_ptr<AudioRegion>, bool)> ()
		.addFunction ("set_strech_and_pitch", &ARDOUR::LuaAPI::Rubberband::set_strech_and_pitch)
		.addFunction ("set_mapping", &ARDOUR::LuaAPI::Rubberband::set_mapping)
		.addFunction ("process", &ARDOUR::LuaAPI::Rubberband::process)
		.addFunction ("readable_length", &ARDOUR::LuaAPI::Rubberband::readable_length_samples)
		.addFunction ("n_channels", &ARDOUR::LuaAPI::Rubberband::n_channels)
		.addFunction ("readable", &ARDOUR::LuaAPI::Rubberband::readable)
		.endClass ()

		.endNamespace () // end LuaAPI
		.endNamespace ();// end ARDOUR

	// DSP functions
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
		.addFunction ("process_map", &DSP::process_map)
		.addRefFunction ("peaks", &DSP::peaks)

		.beginClass <DSP::LowPass> ("LowPass")
		.addConstructor <void (*) (double, float)> ()
		.addFunction ("proc", &DSP::LowPass::proc)
		.addFunction ("ctrl", &DSP::LowPass::ctrl)
		.addFunction ("set_cutoff", &DSP::LowPass::set_cutoff)
		.addFunction ("reset", &DSP::LowPass::reset)
		.endClass ()
		.beginClass <DSP::Biquad> ("Biquad")
		.addConstructor <void (*) (double)> ()
		.addFunction ("run", &DSP::Biquad::run)
		.addFunction ("compute", &DSP::Biquad::compute)
		.addFunction ("configure", &DSP::Biquad::configure)
		.addFunction ("reset", &DSP::Biquad::reset)
		.addFunction ("dB_at_freq", &DSP::Biquad::dB_at_freq)
		.endClass ()
		.beginClass <DSP::FFTSpectrum> ("FFTSpectrum")
		.addConstructor <void (*) (uint32_t, double)> ()
		.addFunction ("set_data_hann", &DSP::FFTSpectrum::set_data_hann)
		.addFunction ("execute", &DSP::FFTSpectrum::execute)
		.addFunction ("power_at_bin", &DSP::FFTSpectrum::power_at_bin)
		.addFunction ("freq_at_bin", &DSP::FFTSpectrum::freq_at_bin)
		.endClass ()
		.beginClass <DSP::Generator> ("Generator")
		.addVoidConstructor ()
		.addFunction ("run", &DSP::Generator::run)
		.addFunction ("set_type", &DSP::Generator::set_type)
		.endClass ()

		.beginClass <ARDOUR::LTCReader> ("LTCReader")
		.addConstructor <void (*) (int, LTC_TV_STANDARD)> ()
		.addFunction ("write", &ARDOUR::LTCReader::write)
		.addRefFunction ("read", &ARDOUR::LTCReader::read)
		.endClass ()

		.beginClass <DSP::Convolution> ("Convolution")
		.addConstructor <void (*) (Session&, uint32_t, uint32_t)> ()
		.addFunction ("add_impdata", &ARDOUR::DSP::Convolution::add_impdata)
		.addFunction ("run", &ARDOUR::DSP::Convolution::run)
		.addFunction ("restart", &ARDOUR::DSP::Convolution::restart)
		.addFunction ("ready", &ARDOUR::DSP::Convolution::ready)
		.addFunction ("latency", &ARDOUR::DSP::Convolution::latency)
		.addFunction ("n_inputs", &ARDOUR::DSP::Convolution::n_inputs)
		.addFunction ("n_outputs", &ARDOUR::DSP::Convolution::n_outputs)
		.endClass ()

		.beginClass <DSP::Convolver::IRSettings> ("IRSettings")
		.addVoidConstructor ()
		.addData ("gain", &DSP::Convolver::IRSettings::gain)
		.addData ("pre_delay", &DSP::Convolver::IRSettings::pre_delay)
		.addFunction ("get_channel_gain", &ARDOUR::DSP::Convolver::IRSettings::get_channel_gain)
		.addFunction ("set_channel_gain", &ARDOUR::DSP::Convolver::IRSettings::set_channel_gain)
		.addFunction ("get_channel_delay", &ARDOUR::DSP::Convolver::IRSettings::get_channel_delay)
		.addFunction ("set_channel_delay", &ARDOUR::DSP::Convolver::IRSettings::set_channel_delay)
		.endClass ()

		.deriveClass <DSP::Convolver, DSP::Convolution> ("Convolver")
		.addConstructor <void (*) (Session&, std::string const&, DSP::Convolver::IRChannelConfig, DSP::Convolver::IRSettings)> ()
		.addFunction ("run_mono_buffered", &ARDOUR::DSP::Convolver::run_mono_buffered)
		.addFunction ("run_stereo_buffered", &ARDOUR::DSP::Convolver::run_stereo_buffered)
		.addFunction ("run_mono_no_latency", &ARDOUR::DSP::Convolver::run_mono_no_latency)
		.addFunction ("run_stereo_no_latency", &ARDOUR::DSP::Convolver::run_stereo_no_latency)
		.endClass ()

		/* DSP enums */
		.beginNamespace ("BiquadType")
		.addConst ("LowPass", ARDOUR::DSP::Biquad::LowPass)
		.addConst ("HighPass", ARDOUR::DSP::Biquad::HighPass)
		.addConst ("BandPassSkirt", ARDOUR::DSP::Biquad::BandPassSkirt)
		.addConst ("BandPass0dB", ARDOUR::DSP::Biquad::BandPass0dB)
		.addConst ("Notch", ARDOUR::DSP::Biquad::Notch)
		.addConst ("AllPass", ARDOUR::DSP::Biquad::AllPass)
		.addConst ("Peaking", ARDOUR::DSP::Biquad::Peaking)
		.addConst ("LowShelf", ARDOUR::DSP::Biquad::LowShelf)
		.addConst ("HighShelf", ARDOUR::DSP::Biquad::HighShelf)
		.endNamespace ()

		.beginNamespace ("NoiseType")
		.addConst ("UniformWhiteNoise", ARDOUR::DSP::Generator::UniformWhiteNoise)
		.addConst ("GaussianWhiteNoise", ARDOUR::DSP::Generator::GaussianWhiteNoise)
		.addConst ("PinkNoise", ARDOUR::DSP::Generator::PinkNoise)
		.endNamespace ()

		.beginNamespace ("LTC_TV_STANDARD")
		.addConst ("LTC_TV_525_60", LTC_TV_525_60)
		.addConst ("LTC_TV_625_50", LTC_TV_625_50)
		.addConst ("LTC_TV_1125_60", LTC_TV_1125_60)
		.addConst ("LTC_TV_FILM_24", LTC_TV_FILM_24)
		.endNamespace ()

		.beginNamespace ("IRChannelConfig")
		.addConst ("Mono", DSP::Convolver::Mono)
		.addConst ("MonoToStereo", DSP::Convolver::MonoToStereo)
		.addConst ("Stereo", DSP::Convolver::Stereo)
		.endNamespace ()

		.beginClass <DSP::DspShm> ("DspShm")
		.addConstructor<void (*) (size_t)> ()
		.addFunction ("allocate", &DSP::DspShm::allocate)
		.addFunction ("clear", &DSP::DspShm::clear)
		.addFunction ("to_float", &DSP::DspShm::to_float)
		.addFunction ("to_int", &DSP::DspShm::to_int)
		.addFunction ("atomic_set_int", &DSP::DspShm::atomic_set_int)
		.addFunction ("atomic_get_int", &DSP::DspShm::atomic_get_int)
		.endClass ()

		.endNamespace () // DSP
		.endNamespace ();// end ARDOUR
}

void
LuaBindings::dsp (lua_State* L)
{
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")

		.beginClass <AudioBuffer> ("AudioBuffer")
		.addEqualCheck ()
		.addFunction ("data", (Sample*(AudioBuffer::*)(samplecnt_t))&AudioBuffer::data)
		.addFunction ("silence", &AudioBuffer::silence)
		.addFunction ("apply_gain", &AudioBuffer::apply_gain)
		.addFunction ("check_silence", &AudioBuffer::check_silence)
		.addFunction ("read_from", (void (AudioBuffer::*)(const Sample*, samplecnt_t, samplecnt_t, samplecnt_t))&AudioBuffer::read_from)
		.endClass()

		.beginClass <MidiBuffer> ("MidiBuffer")
		.addEqualCheck ()
		.addFunction ("silence", &MidiBuffer::silence)
		.addFunction ("size", &MidiBuffer::size)
		.addFunction ("empty", &MidiBuffer::empty)
		.addFunction ("resize", &MidiBuffer::resize)
		.addFunction ("copy", (void (MidiBuffer::*)(MidiBuffer const * const))&MidiBuffer::copy)
		.addFunction ("push_event", (bool (MidiBuffer::*)(const Evoral::Event<samplepos_t>&))&MidiBuffer::push_back)
		.addFunction ("push_back", (bool (MidiBuffer::*)(samplepos_t, Evoral::EventType, size_t, const uint8_t*))&MidiBuffer::push_back)
		// TODO iterators..
		.addExtCFunction ("table", &luabridge::CFunc::listToTable<const Evoral::Event<samplepos_t>, MidiBuffer>)
		.endClass()

		.beginClass <BufferSet> ("BufferSet")
		.addEqualCheck ()
		.addFunction ("get_audio", static_cast<AudioBuffer&(BufferSet::*)(size_t)>(&BufferSet::get_audio))
		.addFunction ("get_midi", static_cast<MidiBuffer&(BufferSet::*)(size_t)>(&BufferSet::get_midi))
		.addFunction ("count", static_cast<const ChanCount&(BufferSet::*)()const>(&BufferSet::count))
		.addFunction ("available", static_cast<const ChanCount&(BufferSet::*)()const>(&BufferSet::available))
		.endClass()
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Evoral")
		.deriveClass <Evoral::Event<samplepos_t>, Evoral::Event<samplepos_t> > ("Event")
		// add Ctor?
		.addFunction ("type", &Evoral::Event<samplepos_t>::type)
		.addFunction ("channel", &Evoral::Event<samplepos_t>::channel)
		.addFunction ("set_type", &Evoral::Event<samplepos_t>::set_type)
		.addFunction ("set_channel", &Evoral::Event<samplepos_t>::set_channel)
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
		.beginClass <FluidSynth> ("FluidSynth")
		.addConstructor <void (*) (float, int)> ()
		.addFunction ("load_sf2", &FluidSynth::load_sf2)
		.addFunction ("synth", &FluidSynth::synth)
		.addFunction ("midi_event", &FluidSynth::midi_event)
		.addFunction ("panic", &FluidSynth::panic)
		.addFunction ("select_program", &FluidSynth::select_program)
		.addFunction ("program_count", &FluidSynth::program_count)
		.addFunction ("program_name", &FluidSynth::program_name)
		.endClass ()
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")

		.beginClass <LuaTableRef> ("LuaTableRef")
		.addCFunction ("get", &LuaTableRef::get)
		.addCFunction ("set", &LuaTableRef::set)
		.endClass ()

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
		.addFunction ("rename", &Session::rename)
		.addFunction ("set_dirty", &Session::set_dirty)
		.addFunction ("unknown_processors", &Session::unknown_processors)
		.addFunction ("export_track_state", &Session::export_track_state)

		.addFunction<RouteList (Session::*)(uint32_t, PresentationInfo::order_t, const std::string&, const std::string&, PlaylistDisposition)> ("new_route_from_template", &Session::new_route_from_template)
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
