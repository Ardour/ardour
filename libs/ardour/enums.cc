/*
    Copyright (C) 2000-2007 Paul Davis 

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

#include <pbd/enumwriter.h>

#include <ardour/types.h>
#include <ardour/session.h>
#include <ardour/location.h>
#include <ardour/audiofilesource.h>
#include <ardour/diskstream.h>
#include <ardour/audioregion.h>
#include <ardour/route_group.h>
#include <ardour/panner.h>
#include <ardour/track.h>

using namespace std;
using namespace PBD;
using namespace ARDOUR;

void
setup_enum_writer ()
{
	EnumWriter* enum_writer = new EnumWriter();
	vector<int> i;
	vector<string> s;

	OverlapType _OverlapType;
	AlignStyle _AlignStyle;
	MeterPoint _MeterPoint;
	TrackMode _TrackMode;
	MeterFalloff _MeterFalloff;
	MeterHold _MeterHold;
	EditMode _EditMode;
	RegionPoint _RegionPoint;
	Placement _Placement;
	MonitorModel _MonitorModel;
	RemoteModel _RemoteModel;
	DenormalModel _DenormalModel;
	CrossfadeModel _CrossfadeModel;
	LayerModel _LayerModel;
	SoloModel _SoloModel;
	SampleFormat _SampleFormat;
	HeaderFormat _HeaderFormat;
	PluginType _PluginType;
	SlaveSource _SlaveSource;
	ShuttleBehaviour _ShuttleBehaviour;
	ShuttleUnits _ShuttleUnits;
	mute_type _mute_type;
	Session::RecordState _Session_RecordState;
	Session::Event::Type _Session_Event_Type;
	SmpteFormat _Session_SmpteFormat;
	Session::PullupFormat _Session_PullupFormat;
	AudioRegion::FadeShape _AudioRegion_FadeShape;
	Panner::LinkDirection _Panner_LinkDirection;
	IOChange _IOChange;
	AutomationType _AutomationType;
	AutoState _AutoState;
	AutoStyle _AutoStyle;
	AutoConnectOption _AutoConnectOption;
	Session::StateOfTheState _Session_StateOfTheState;
	Route::Flag _Route_Flag;
	AudioFileSource::Flag _AudioFileSource_Flag;
	Diskstream::Flag _Diskstream_Flag;
	Location::Flags _Location_Flags;
	RouteGroup::Flag _RouteGroup_Flag;
	Region::Flag _Region_Flag;
	Track::FreezeState _Track_FreezeState;

#define REGISTER(e) enum_writer->register_distinct (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_BITS(e) enum_writer->register_bits (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_ENUM(e) i.push_back (e); s.push_back (#e)
#define REGISTER_CLASS_ENUM(t,e) i.push_back (t::e); s.push_back (#e)

	REGISTER_ENUM (NoChange);
	REGISTER_ENUM (ConfigurationChanged);
	REGISTER_ENUM (ConnectionsChanged);
	REGISTER_BITS (_IOChange);

	REGISTER_ENUM (OverlapNone);
	REGISTER_ENUM (OverlapInternal);
	REGISTER_ENUM (OverlapStart);
	REGISTER_ENUM (OverlapEnd);
	REGISTER_ENUM (OverlapExternal);
	REGISTER (_OverlapType);
	
	REGISTER_ENUM (GainAutomation);
	REGISTER_ENUM (PanAutomation);
	REGISTER_ENUM (PluginAutomation);
	REGISTER_ENUM (SoloAutomation);
	REGISTER_ENUM (MuteAutomation);
	REGISTER_BITS (_AutomationType);

	REGISTER_ENUM (Off);
	REGISTER_ENUM (Write);
	REGISTER_ENUM (Touch);
	REGISTER_ENUM (Play);
	REGISTER_BITS (_AutoState);

	REGISTER_ENUM (Absolute);
	REGISTER_ENUM (Trim);
	REGISTER_BITS (_AutoStyle);

	REGISTER_ENUM (CaptureTime);
	REGISTER_ENUM (ExistingMaterial);	
	REGISTER (_AlignStyle);

	REGISTER_ENUM (MeterInput);
	REGISTER_ENUM (MeterPreFader);
	REGISTER_ENUM (MeterPostFader);	
	REGISTER (_MeterPoint);

	REGISTER_ENUM (Normal);
	REGISTER_ENUM (Destructive);
	REGISTER (_TrackMode);

	REGISTER_ENUM (MeterFalloffOff);
	REGISTER_ENUM (MeterFalloffSlowest);
	REGISTER_ENUM (MeterFalloffSlow);
	REGISTER_ENUM (MeterFalloffMedium);
	REGISTER_ENUM (MeterFalloffFast);
	REGISTER_ENUM (MeterFalloffFaster);
	REGISTER_ENUM (MeterFalloffFastest);
	REGISTER (_MeterFalloff);
	
	REGISTER_ENUM (MeterHoldOff);
	REGISTER_ENUM (MeterHoldShort);
	REGISTER_ENUM (MeterHoldMedium);
	REGISTER_ENUM (MeterHoldLong);
	REGISTER (_MeterHold);

	REGISTER_ENUM (Slide);
	REGISTER_ENUM (Splice);	
	REGISTER (_EditMode);

	REGISTER_ENUM (Start);
	REGISTER_ENUM (End);
	REGISTER_ENUM (SyncPoint);	
	REGISTER (_RegionPoint);

	REGISTER_ENUM (PreFader);
	REGISTER_ENUM (PostFader);
	REGISTER (_Placement);

	REGISTER_ENUM (HardwareMonitoring);
	REGISTER_ENUM (SoftwareMonitoring);
	REGISTER_ENUM (ExternalMonitoring);
	REGISTER (_MonitorModel);

	REGISTER_ENUM (DenormalNone);
	REGISTER_ENUM (DenormalFTZ);
	REGISTER_ENUM (DenormalDAZ);
	REGISTER_ENUM (DenormalFTZDAZ);
	REGISTER (_DenormalModel);

	REGISTER_ENUM (UserOrdered);
	REGISTER_ENUM (MixerOrdered);
	REGISTER_ENUM (EditorOrdered);
	REGISTER (_RemoteModel);

	REGISTER_ENUM (FullCrossfade);
	REGISTER_ENUM (ShortCrossfade);
	REGISTER (_CrossfadeModel);

	REGISTER_ENUM (LaterHigher);
	REGISTER_ENUM (MoveAddHigher);
	REGISTER_ENUM (AddHigher);	
	REGISTER (_LayerModel);

	REGISTER_ENUM (InverseMute);
	REGISTER_ENUM (SoloBus);	
	REGISTER (_SoloModel);

	REGISTER_ENUM (AutoConnectPhysical);
	REGISTER_ENUM (AutoConnectMaster);
	REGISTER_BITS (_AutoConnectOption);

	REGISTER_ENUM (FormatFloat);
	REGISTER_ENUM (FormatInt24);
	REGISTER_ENUM (FormatInt16);
	REGISTER (_SampleFormat);

	REGISTER_ENUM (BWF);
	REGISTER_ENUM (WAVE);
	REGISTER_ENUM (WAVE64);
	REGISTER_ENUM (CAF);
	REGISTER_ENUM (AIFF);
	REGISTER_ENUM (iXML);
	REGISTER_ENUM (RF64);	
	REGISTER (_HeaderFormat);

	REGISTER_ENUM (AudioUnit);
	REGISTER_ENUM (LADSPA);
	REGISTER_ENUM (VST);
	REGISTER (_PluginType);

	REGISTER_ENUM (None);
	REGISTER_ENUM (MTC);
	REGISTER_ENUM (JACK);	
	REGISTER (_SlaveSource);

	REGISTER_ENUM (Sprung);
	REGISTER_ENUM (Wheel);	
	REGISTER (_ShuttleBehaviour);

	REGISTER_ENUM (Percentage);
	REGISTER_ENUM (Semitones);	
	REGISTER (_ShuttleUnits);

	REGISTER_CLASS_ENUM (Session, Disabled);
	REGISTER_CLASS_ENUM (Session, Enabled);
	REGISTER_CLASS_ENUM (Session, Recording);
	REGISTER (_Session_RecordState);

	REGISTER_CLASS_ENUM (Session::Event, SetTransportSpeed);
	REGISTER_CLASS_ENUM (Session::Event, SetDiskstreamSpeed);
	REGISTER_CLASS_ENUM (Session::Event, Locate);
	REGISTER_CLASS_ENUM (Session::Event, LocateRoll);
	REGISTER_CLASS_ENUM (Session::Event, LocateRollLocate);
	REGISTER_CLASS_ENUM (Session::Event, SetLoop);
	REGISTER_CLASS_ENUM (Session::Event, PunchIn);
	REGISTER_CLASS_ENUM (Session::Event, PunchOut);
	REGISTER_CLASS_ENUM (Session::Event, RangeStop);
	REGISTER_CLASS_ENUM (Session::Event, RangeLocate);
	REGISTER_CLASS_ENUM (Session::Event, Overwrite);
	REGISTER_CLASS_ENUM (Session::Event, SetSlaveSource);
	REGISTER_CLASS_ENUM (Session::Event, Audition);
	REGISTER_CLASS_ENUM (Session::Event, InputConfigurationChange);
	REGISTER_CLASS_ENUM (Session::Event, SetAudioRange);
	REGISTER_CLASS_ENUM (Session::Event, SetPlayRange);
	REGISTER_CLASS_ENUM (Session::Event, StopOnce);
	REGISTER_CLASS_ENUM (Session::Event, AutoLoop);
	REGISTER (_Session_Event_Type);

	REGISTER_CLASS_ENUM (Session, Clean);
	REGISTER_CLASS_ENUM (Session, Dirty);
	REGISTER_CLASS_ENUM (Session, CannotSave);
	REGISTER_CLASS_ENUM (Session, Deletion);
	REGISTER_CLASS_ENUM (Session, InitialConnecting);
	REGISTER_CLASS_ENUM (Session, Loading);
	REGISTER_CLASS_ENUM (Session, InCleanup);
	REGISTER_BITS (_Session_StateOfTheState);

	REGISTER_ENUM (smpte_23976);
	REGISTER_ENUM (smpte_24);
	REGISTER_ENUM (smpte_24976);
	REGISTER_ENUM (smpte_25);
	REGISTER_ENUM (smpte_2997);
	REGISTER_ENUM (smpte_2997drop);
	REGISTER_ENUM (smpte_30);
	REGISTER_ENUM (smpte_30drop);
	REGISTER_ENUM (smpte_5994);
	REGISTER_ENUM (smpte_60);
	REGISTER (_Session_SmpteFormat);

	REGISTER_CLASS_ENUM (Session, pullup_Plus4Plus1);
	REGISTER_CLASS_ENUM (Session, pullup_Plus4);
	REGISTER_CLASS_ENUM (Session, pullup_Plus4Minus1);
	REGISTER_CLASS_ENUM (Session, pullup_Plus1);
	REGISTER_CLASS_ENUM (Session, pullup_None);
	REGISTER_CLASS_ENUM (Session, pullup_Minus1);
	REGISTER_CLASS_ENUM (Session, pullup_Minus4Plus1);
	REGISTER_CLASS_ENUM (Session, pullup_Minus4);
	REGISTER_CLASS_ENUM (Session, pullup_Minus4Minus1);
	REGISTER (_Session_PullupFormat);

	REGISTER_ENUM (PRE_FADER);
	REGISTER_ENUM (POST_FADER);
	REGISTER_ENUM (CONTROL_OUTS);
	REGISTER_ENUM (MAIN_OUTS);
	REGISTER (_mute_type);

	REGISTER_CLASS_ENUM (Route, Hidden);
	REGISTER_CLASS_ENUM (Route, MasterOut);
	REGISTER_CLASS_ENUM (Route, ControlOut);
	REGISTER_BITS (_Route_Flag);

	REGISTER_CLASS_ENUM (AudioFileSource, Writable);
	REGISTER_CLASS_ENUM (AudioFileSource, CanRename);
	REGISTER_CLASS_ENUM (AudioFileSource, Broadcast);
	REGISTER_CLASS_ENUM (AudioFileSource, Removable);
	REGISTER_CLASS_ENUM (AudioFileSource, RemovableIfEmpty);
	REGISTER_CLASS_ENUM (AudioFileSource, RemoveAtDestroy);
	REGISTER_CLASS_ENUM (AudioFileSource, NoPeakFile);
	REGISTER_CLASS_ENUM (AudioFileSource, Destructive);
	REGISTER_BITS (_AudioFileSource_Flag);

	REGISTER_CLASS_ENUM (AudioRegion, Linear);
	REGISTER_CLASS_ENUM (AudioRegion, Fast);
	REGISTER_CLASS_ENUM (AudioRegion, Slow);
	REGISTER_CLASS_ENUM (AudioRegion, LogA);
	REGISTER_CLASS_ENUM (AudioRegion, LogB);
	REGISTER (_AudioRegion_FadeShape);

	REGISTER_CLASS_ENUM (Diskstream, Recordable);
	REGISTER_CLASS_ENUM (Diskstream, Hidden);
	REGISTER_CLASS_ENUM (Diskstream, Destructive);
	REGISTER_BITS (_Diskstream_Flag);

	REGISTER_CLASS_ENUM (Location, IsMark);
	REGISTER_CLASS_ENUM (Location, IsAutoPunch);
	REGISTER_CLASS_ENUM (Location, IsAutoLoop);
	REGISTER_CLASS_ENUM (Location, IsHidden);
	REGISTER_CLASS_ENUM (Location, IsCDMarker);
	REGISTER_CLASS_ENUM (Location, IsEnd);
	REGISTER_CLASS_ENUM (Location, IsRangeMarker);
	REGISTER_CLASS_ENUM (Location, IsStart);
	REGISTER_BITS (_Location_Flags);


	REGISTER_CLASS_ENUM (RouteGroup, Relative);
	REGISTER_CLASS_ENUM (RouteGroup, Active);
	REGISTER_CLASS_ENUM (RouteGroup, Hidden);
	REGISTER_BITS (_RouteGroup_Flag);

	REGISTER_CLASS_ENUM (Panner, SameDirection);
	REGISTER_CLASS_ENUM (Panner, OppositeDirection);
	REGISTER (_Panner_LinkDirection);

	REGISTER_CLASS_ENUM (Region, Muted);
	REGISTER_CLASS_ENUM (Region, Opaque);
	REGISTER_CLASS_ENUM (Region, EnvelopeActive);
	REGISTER_CLASS_ENUM (Region, DefaultFadeIn);
	REGISTER_CLASS_ENUM (Region, DefaultFadeOut);
	REGISTER_CLASS_ENUM (Region, Locked);
	REGISTER_CLASS_ENUM (Region, Automatic);
	REGISTER_CLASS_ENUM (Region, WholeFile);
	REGISTER_CLASS_ENUM (Region, FadeIn);
	REGISTER_CLASS_ENUM (Region, FadeOut);
	REGISTER_CLASS_ENUM (Region, Copied);
	REGISTER_CLASS_ENUM (Region, Import);
	REGISTER_CLASS_ENUM (Region, External);
	REGISTER_CLASS_ENUM (Region, SyncMarked);
	REGISTER_CLASS_ENUM (Region, LeftOfSplit);
	REGISTER_CLASS_ENUM (Region, RightOfSplit);
	REGISTER_CLASS_ENUM (Region, Hidden);
	REGISTER_CLASS_ENUM (Region, DoNotSaveState);
	REGISTER_BITS (_Region_Flag);

	REGISTER_CLASS_ENUM (Track, NoFreeze);
	REGISTER_CLASS_ENUM (Track, Frozen);
	REGISTER_CLASS_ENUM (Track, UnFrozen);
	REGISTER (_Track_FreezeState);
	
}
