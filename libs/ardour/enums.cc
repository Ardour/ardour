/*
 * Copyright (C) 2000-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2010-2012 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2013-2018 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2014-2019 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2018 Len Ovens <len@ovenwerks.net>
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

#include "pbd/enumwriter.h"
#include "midi++/types.h"

#include "ardour/delivery.h"
#include "ardour/disk_io.h"
#include "ardour/export_channel.h"
#include "ardour/export_filename.h"
#include "ardour/export_format_base.h"
#include "ardour/export_profile_manager.h"
#include "ardour/io.h"
#include "ardour/location.h"
#include "ardour/midi_model.h"
#include "ardour/mode.h"
#include "ardour/mute_master.h"
#include "ardour/presentation_info.h"
#include "ardour/session.h"
#include "ardour/source.h"
#include "ardour/tempo.h"
#include "ardour/track.h"
#include "ardour/transport_fsm.h"
#include "ardour/transport_master.h"
#include "ardour/triggerbox.h"
#include "ardour/types.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace MIDI;
using namespace Timecode;

namespace ARDOUR {

void
setup_enum_writer ()
{
	EnumWriter& enum_writer (EnumWriter::instance());
	vector<int> i;
	vector<string> s;

	AlignStyle _AlignStyle;
	AlignChoice _AlignChoice;
	MeterPoint _MeterPoint;
	DiskIOPoint _DiskIOPoint;
	MeterType _MeterType;
	TrackMode _TrackMode;
	NoteMode _NoteMode;
	ChannelMode _ChannelMode;
	ColorMode _ColorMode;
	MeterFalloff _MeterFalloff;
	MeterHold _MeterHold;
	VUMeterStandard _VUMeterStandard;
	MeterLineUp _MeterLineUp;
	InputMeterLayout _InputMeterLayout;
	EditMode _EditMode;
	RegionPoint _RegionPoint;
	Placement _Placement;
	MonitorModel _MonitorModel;
	MonitorChoice _MonitorChoice;
	MonitorState _MonitorState;
	PFLPosition _PFLPosition;
	AFLPosition _AFLPosition;
	DenormalModel _DenormalModel;
	ClockDeltaMode _ClockDeltaMode;
	LayerModel _LayerModel;
	InsertMergePolicy _InsertMergePolicy;
	ListenPosition _ListenPosition;
	SampleFormat _SampleFormat;
	CDMarkerFormat _CDMarkerFormat;
	HeaderFormat _HeaderFormat;
	PluginType _PluginType;
	SyncSource _SyncSource;
	TransportRequestType _TransportRequestType;
	ShuttleUnits _ShuttleUnits;
	Session::RecordState _Session_RecordState;
	SessionEvent::Type _SessionEvent_Type;
	SessionEvent::Action _SessionEvent_Action;
	TimecodeFormat _Session_TimecodeFormat;
	Session::PullupFormat _Session_PullupFormat;
	FadeShape _FadeShape;
	RegionSelectionAfterSplit _RegionSelectionAfterSplit;
	RangeSelectionAfterSplit _RangeSelectionAfterSplit;
	IOChange _IOChange;
	AutomationType _AutomationType;
	AutoState _AutoState;
	AutoConnectOption _AutoConnectOption;
	TracksAutoNamingRule _TracksAutoNamingRule;
	Session::StateOfTheState _Session_StateOfTheState;
	Source::Flag _Source_Flag;
	DiskIOProcessor::Flag _DiskIOProcessor_Flag;
	Location::Flags _Location_Flags;
	Track::FreezeState _Track_FreezeState;
	AutomationList::InterpolationStyle _AutomationList_InterpolationStyle;
	AnyTime::Type _AnyTime_Type;
	ExportFilename::TimeFormat _ExportFilename_TimeFormat;
	ExportFilename::DateFormat _ExportFilename_DateFormat;
	ExportFormatBase::Type _ExportFormatBase_Type;
	ExportFormatBase::FormatId _ExportFormatBase_FormatId;
	ExportFormatBase::Endianness _ExportFormatBase_Endianness;
	ExportFormatBase::SampleFormat _ExportFormatBase_SampleFormat;
	ExportFormatBase::DitherType _ExportFormatBase_DitherType;
	ExportFormatBase::Quality _ExportFormatBase_Quality;
	ExportFormatBase::SampleRate _ExportFormatBase_SampleRate;
	ExportFormatBase::SRCQuality _ExportFormatBase_SRCQuality;
	ExportProfileManager::TimeFormat _ExportProfileManager_TimeFormat;
	RegionExportChannelFactory::Type _RegionExportChannelFactory_Type;
	Delivery::Role _Delivery_Role;
	IO::Direction _IO_Direction;
	MuteMaster::MutePoint _MuteMaster_MutePoint;
	MidiModel::NoteDiffCommand::Property _MidiModel_NoteDiffCommand_Property;
	MidiModel::SysExDiffCommand::Property _MidiModel_SysExDiffCommand_Property;
	MidiModel::PatchChangeDiffCommand::Property _MidiModel_PatchChangeDiffCommand_Property;
	RegionEquivalence _RegionEquivalence;
	WaveformScale _WaveformScale;
	WaveformShape _WaveformShape;
	ScreenSaverMode _ScreenSaverMode;
	Session::PostTransportWork _Session_PostTransportWork;
	MTC_Status _MIDI_MTC_Status;
	BufferingPreset _BufferingPreset;
	AutoReturnTarget _AutoReturnTarget;
	PresentationInfo::Flag _PresentationInfo_Flag;
	MusicalMode::Type mode;
	MidiPortFlags _MidiPortFlags;
	TransportFSM::EventType _TransportFSM_EventType;
	TransportFSM::MotionState _TransportFSM_MotionState;
	TransportFSM::ButlerState _TransportFSM_ButlerState;
	TransportFSM::DirectionState _TransportFSM_DirectionState;
	LoopFadeChoice _LoopFadeChooice;
	TransportState _TransportState;
	LocateTransportDisposition _LocateTransportDisposition;
	Trigger::State _TriggerState;
	Trigger::LaunchStyle _TriggerLaunchStyle;
	FollowAction::Type _FollowAction;
	Trigger::StretchMode _TriggerStretchMode;
	CueBehavior _CueBehavior;

#define REGISTER(e) enum_writer.register_distinct (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_BITS(e) enum_writer.register_bits (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_ENUM(e) i.push_back (e); s.push_back (#e)
#define REGISTER_CLASS_ENUM(t,e) i.push_back (t::e); s.push_back (#e)

	/* in mid-2017 the entire code base was changed to use "samples"
	   instead of frames, which included several enums. This hack table
	   entry will catch all of them.
	*/
	enum_writer.add_to_hack_table ("Frames", "Samples");

	REGISTER_ENUM (NullAutomation);
	REGISTER_ENUM (GainAutomation);
	REGISTER_ENUM (PanAzimuthAutomation);
	REGISTER_ENUM (PanElevationAutomation);
	REGISTER_ENUM (PanWidthAutomation);
	REGISTER_ENUM (PanFrontBackAutomation);
	REGISTER_ENUM (PanLFEAutomation);
	REGISTER_ENUM (PluginAutomation);
	REGISTER_ENUM (PluginPropertyAutomation);
	REGISTER_ENUM (SoloAutomation);
	REGISTER_ENUM (SoloIsolateAutomation);
	REGISTER_ENUM (SoloSafeAutomation);
	REGISTER_ENUM (MuteAutomation);
	REGISTER_ENUM (MidiCCAutomation);
	REGISTER_ENUM (MidiPgmChangeAutomation);
	REGISTER_ENUM (MidiPitchBenderAutomation);
	REGISTER_ENUM (MidiChannelPressureAutomation);
	REGISTER_ENUM (MidiNotePressureAutomation);
	REGISTER_ENUM (MidiSystemExclusiveAutomation);
	REGISTER_ENUM (FadeInAutomation);
	REGISTER_ENUM (FadeOutAutomation);
	REGISTER_ENUM (EnvelopeAutomation);
	REGISTER_ENUM (RecEnableAutomation);
	REGISTER_ENUM (RecSafeAutomation);
	REGISTER_ENUM (TrimAutomation);
	REGISTER_ENUM (PhaseAutomation);
	REGISTER_ENUM (MonitoringAutomation);
	REGISTER_ENUM (BusSendLevel);
	REGISTER_ENUM (BusSendEnable);
	REGISTER_ENUM (MainOutVolume);
	REGISTER (_AutomationType);

	REGISTER_ENUM (Off);
	REGISTER_ENUM (Write);
	REGISTER_ENUM (Touch);
	REGISTER_ENUM (Play);
	REGISTER_ENUM (Latch);
	REGISTER_BITS (_AutoState);

	REGISTER_ENUM (CaptureTime);
	REGISTER_ENUM (ExistingMaterial);
	REGISTER (_AlignStyle);

	REGISTER_ENUM (UseCaptureTime);
	REGISTER_ENUM (UseExistingMaterial);
	REGISTER_ENUM (Automatic);
	REGISTER (_AlignChoice);

	REGISTER_ENUM (MeterInput);
	REGISTER_ENUM (MeterPreFader);
	REGISTER_ENUM (MeterPostFader);
	REGISTER_ENUM (MeterOutput);
	REGISTER_ENUM (MeterCustom);
	REGISTER (_MeterPoint);

	REGISTER_ENUM (DiskIOPreFader);
	REGISTER_ENUM (DiskIOPostFader);
	REGISTER_ENUM (DiskIOCustom);
	REGISTER (_DiskIOPoint);

	REGISTER_ENUM (MeterMaxSignal);
	REGISTER_ENUM (MeterMaxPeak);
	REGISTER_ENUM (MeterPeak);
	REGISTER_ENUM (MeterKrms);
	REGISTER_ENUM (MeterK20);
	REGISTER_ENUM (MeterK14);
	REGISTER_ENUM (MeterK12);
	REGISTER_ENUM (MeterIEC1DIN);
	REGISTER_ENUM (MeterIEC1NOR);
	REGISTER_ENUM (MeterIEC2BBC);
	REGISTER_ENUM (MeterIEC2EBU);
	REGISTER_ENUM (MeterVU);
	REGISTER_ENUM (MeterPeak0dB);
	REGISTER_ENUM (MeterMCP);
	REGISTER (_MeterType);

	REGISTER_ENUM (Normal);
	REGISTER_ENUM (NonLayered);
	/* No longer used but we leave this here so that enumwriter can parse
	 * strings containing "Destructive"
	 */
	REGISTER_ENUM (Destructive);
	REGISTER (_TrackMode);

	REGISTER_ENUM (Sustained);
	REGISTER_ENUM (Percussive);
	REGISTER (_NoteMode);

	REGISTER_ENUM (AllChannels);
	REGISTER_ENUM (FilterChannels);
	REGISTER_ENUM (ForceChannel);
	REGISTER (_ChannelMode);

	REGISTER_ENUM (MeterColors);
	REGISTER_ENUM (ChannelColors);
	REGISTER_ENUM (TrackColor);
	REGISTER (_ColorMode);

	REGISTER_ENUM (MeterFalloffOff);
	REGISTER_ENUM (MeterFalloffSlowest);
	REGISTER_ENUM (MeterFalloffSlow);
	REGISTER_ENUM (MeterFalloffSlowish);
	REGISTER_ENUM (MeterFalloffModerate);
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

	REGISTER_ENUM (MeteringVUfrench);
	REGISTER_ENUM (MeteringVUamerican);
	REGISTER_ENUM (MeteringVUstandard);
	REGISTER_ENUM (MeteringVUeight);
	REGISTER (_VUMeterStandard);

	REGISTER_ENUM (MeteringLineUp24);
	REGISTER_ENUM (MeteringLineUp20);
	REGISTER_ENUM (MeteringLineUp18);
	REGISTER_ENUM (MeteringLineUp15);
	REGISTER (_MeterLineUp);

	REGISTER_ENUM (LayoutVertical);
	REGISTER_ENUM (LayoutHorizontal);
	REGISTER_ENUM (LayoutAutomatic);
	REGISTER (_InputMeterLayout);

	REGISTER_ENUM (Slide);
	REGISTER_ENUM (Ripple);
	REGISTER_ENUM (RippleAll);
	REGISTER_ENUM (Lock);
	REGISTER (_EditMode);
	/*
	 * Splice mode is undefined, undocumented, and basically fubar'ed
	 * perhaps someday we will make it work.  but for now, avoid it
	*/
	enum_writer.add_to_hack_table ("Splice", "Slide");

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

	REGISTER_ENUM (MonitorInput);
	REGISTER_ENUM (MonitorDisk);
	REGISTER_ENUM (MonitorAuto);
	REGISTER_ENUM (MonitorCue);
	REGISTER_BITS (_MonitorChoice);

	REGISTER_ENUM (MonitoringInput);
	REGISTER_ENUM (MonitoringDisk);
	REGISTER_ENUM (MonitoringSilence);
	REGISTER_BITS (_MonitorState);

	REGISTER_ENUM (PFLFromBeforeProcessors);
	REGISTER_ENUM (PFLFromAfterProcessors);
	REGISTER (_PFLPosition);

	REGISTER_ENUM (AFLFromBeforeProcessors);
	REGISTER_ENUM (AFLFromAfterProcessors);
	REGISTER (_AFLPosition);

	REGISTER_ENUM (NoDelta);
	REGISTER_ENUM (DeltaEditPoint);
	REGISTER_ENUM (DeltaOriginMarker);
	REGISTER (_ClockDeltaMode);

	REGISTER_ENUM (DenormalNone);
	REGISTER_ENUM (DenormalFTZ);
	REGISTER_ENUM (DenormalDAZ);
	REGISTER_ENUM (DenormalFTZDAZ);
	REGISTER (_DenormalModel);

	/*
	 * EditorOrdered has been deprecated
	 * since the removal of independent
	 * editor / mixer ordering.
	*/
	enum_writer.add_to_hack_table ("EditorOrdered", "MixerOrdered");

	REGISTER_ENUM (LaterHigher);
	REGISTER_ENUM (Manual);
	REGISTER (_LayerModel);

	REGISTER_ENUM (InsertMergeReject);
	REGISTER_ENUM (InsertMergeRelax);
	REGISTER_ENUM (InsertMergeReplace);
	REGISTER_ENUM (InsertMergeTruncateExisting);
	REGISTER_ENUM (InsertMergeTruncateAddition);
	REGISTER_ENUM (InsertMergeExtend);
	REGISTER (_InsertMergePolicy);

	REGISTER_ENUM (AfterFaderListen);
	REGISTER_ENUM (PreFaderListen);
	REGISTER (_ListenPosition);

	REGISTER_ENUM (AutoConnectPhysical);
	REGISTER_ENUM (AutoConnectMaster);
	REGISTER_BITS (_AutoConnectOption);

	REGISTER_ENUM (UseDefaultNames);
	REGISTER_ENUM (NameAfterDriver);
	REGISTER_BITS (_TracksAutoNamingRule);

	REGISTER_ENUM (FormatFloat);
	REGISTER_ENUM (FormatInt24);
	REGISTER_ENUM (FormatInt16);
	REGISTER (_SampleFormat);

	REGISTER_ENUM (CDMarkerNone);
	REGISTER_ENUM (CDMarkerCUE);
	REGISTER_ENUM (CDMarkerTOC);
	REGISTER (_CDMarkerFormat);

	REGISTER_ENUM (BWF);
	REGISTER_ENUM (WAVE);
	REGISTER_ENUM (WAVE64);
	REGISTER_ENUM (CAF);
	REGISTER_ENUM (AIFF);
	REGISTER_ENUM (iXML);
	REGISTER_ENUM (RF64);
	REGISTER_ENUM (RF64_WAV);
	REGISTER_ENUM (MBWF);
	REGISTER_ENUM (FLAC);
	REGISTER (_HeaderFormat);

	REGISTER_ENUM (AudioUnit);
	REGISTER_ENUM (LADSPA);
	REGISTER_ENUM (LV2);
	REGISTER_ENUM (Windows_VST);
	REGISTER_ENUM (LXVST);
	REGISTER_ENUM (MacVST);
	REGISTER_ENUM (Lua);
	REGISTER_ENUM (VST3);
	REGISTER (_PluginType);

	REGISTER_ENUM (MTC);
	REGISTER_ENUM (JACK);
	REGISTER_ENUM (Engine);
	REGISTER_ENUM (MIDIClock);
	REGISTER_ENUM (LTC);
	REGISTER (_SyncSource);

	REGISTER_ENUM (TR_StartStop);
	REGISTER_ENUM (TR_Speed);
	REGISTER_ENUM (TR_Locate);
	REGISTER (_TransportRequestType);

	REGISTER_ENUM (Percentage);
	REGISTER_ENUM (Semitones);
	REGISTER (_ShuttleUnits);

	REGISTER_CLASS_ENUM (Session, Disabled);
	REGISTER_CLASS_ENUM (Session, Enabled);
	REGISTER_CLASS_ENUM (Session, Recording);
	REGISTER (_Session_RecordState);

	REGISTER_CLASS_ENUM (SessionEvent, SetTransportSpeed);
	REGISTER_CLASS_ENUM (SessionEvent, SetDefaultPlaySpeed);
	REGISTER_CLASS_ENUM (SessionEvent, Locate);
	REGISTER_CLASS_ENUM (SessionEvent, LocateRoll);
	REGISTER_CLASS_ENUM (SessionEvent, LocateRollLocate);
	REGISTER_CLASS_ENUM (SessionEvent, SetLoop);
	REGISTER_CLASS_ENUM (SessionEvent, PunchIn);
	REGISTER_CLASS_ENUM (SessionEvent, PunchOut);
	REGISTER_CLASS_ENUM (SessionEvent, RangeStop);
	REGISTER_CLASS_ENUM (SessionEvent, RangeLocate);
	REGISTER_CLASS_ENUM (SessionEvent, Overwrite);
	REGISTER_CLASS_ENUM (SessionEvent, OverwriteAll);
	REGISTER_CLASS_ENUM (SessionEvent, Audition);
	REGISTER_CLASS_ENUM (SessionEvent, SetPlayAudioRange);
	REGISTER_CLASS_ENUM (SessionEvent, CancelPlayAudioRange);
	REGISTER_CLASS_ENUM (SessionEvent, RealTimeOperation);
	REGISTER_CLASS_ENUM (SessionEvent, AdjustPlaybackBuffering);
	REGISTER_CLASS_ENUM (SessionEvent, AdjustCaptureBuffering);
	REGISTER_CLASS_ENUM (SessionEvent, SetTimecodeTransmission);
	REGISTER_CLASS_ENUM (SessionEvent, Skip);
	REGISTER_CLASS_ENUM (SessionEvent, SetTransportMaster);
	REGISTER_CLASS_ENUM (SessionEvent, StartRoll);
	REGISTER_CLASS_ENUM (SessionEvent, EndRoll);
	REGISTER_CLASS_ENUM (SessionEvent, TransportStateChange);
	REGISTER_CLASS_ENUM (SessionEvent, AutoLoop);
	REGISTER_CLASS_ENUM (SessionEvent, SyncCues);
	REGISTER (_SessionEvent_Type);

	REGISTER_CLASS_ENUM (SessionEvent, Add);
	REGISTER_CLASS_ENUM (SessionEvent, Remove);
	REGISTER_CLASS_ENUM (SessionEvent, Replace);
	REGISTER_CLASS_ENUM (SessionEvent, Clear);
	REGISTER (_SessionEvent_Action);

	REGISTER_ENUM (MTC_Stopped);
	REGISTER_ENUM (MTC_Forward);
	REGISTER_ENUM (MTC_Backward);
	REGISTER (_MIDI_MTC_Status);

	REGISTER_CLASS_ENUM (Session, PostTransportStop);
	REGISTER_CLASS_ENUM (Session, PostTransportLocate);
	REGISTER_CLASS_ENUM (Session, PostTransportAbort);
	REGISTER_CLASS_ENUM (Session, PostTransportOverWrite);
	REGISTER_CLASS_ENUM (Session, PostTransportAudition);
	REGISTER_CLASS_ENUM (Session, PostTransportReverse);
	REGISTER_CLASS_ENUM (Session, PostTransportClearSubstate);
	REGISTER_CLASS_ENUM (Session, PostTransportAdjustPlaybackBuffering);
	REGISTER_CLASS_ENUM (Session, PostTransportAdjustCaptureBuffering);
	REGISTER_CLASS_ENUM (Session, PostTransportLoopChanged);
	REGISTER_BITS (_Session_PostTransportWork);

	REGISTER_CLASS_ENUM (Session, Clean);
	REGISTER_CLASS_ENUM (Session, Dirty);
	REGISTER_CLASS_ENUM (Session, CannotSave);
	REGISTER_CLASS_ENUM (Session, Deletion);
	REGISTER_CLASS_ENUM (Session, InitialConnecting);
	REGISTER_CLASS_ENUM (Session, Loading);
	REGISTER_CLASS_ENUM (Session, InCleanup);
	REGISTER_BITS (_Session_StateOfTheState);

	REGISTER_ENUM (timecode_23976);
	REGISTER_ENUM (timecode_24);
	REGISTER_ENUM (timecode_24976);
	REGISTER_ENUM (timecode_25);
	REGISTER_ENUM (timecode_2997);
	REGISTER_ENUM (timecode_2997drop);
	REGISTER_ENUM (timecode_30);
	REGISTER_ENUM (timecode_30drop);
	REGISTER_ENUM (timecode_5994);
	REGISTER_ENUM (timecode_60);
	REGISTER (_Session_TimecodeFormat);

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

	REGISTER_CLASS_ENUM (Source, Writable);
	REGISTER_CLASS_ENUM (Source, CanRename);
	REGISTER_CLASS_ENUM (Source, Broadcast);
	REGISTER_CLASS_ENUM (Source, Removable);
	REGISTER_CLASS_ENUM (Source, RemovableIfEmpty);
	REGISTER_CLASS_ENUM (Source, RemoveAtDestroy);
	REGISTER_CLASS_ENUM (Source, NoPeakFile);
	/* No longer used but we leave this here so that enumwriter can parse
	 * strings containing "Destructive"
	 */
	REGISTER_CLASS_ENUM (Source, Destructive);
	REGISTER_CLASS_ENUM (Source, Empty);
	REGISTER_BITS (_Source_Flag);

	REGISTER_ENUM (FadeLinear);
	REGISTER_ENUM (FadeFast);
	REGISTER_ENUM (FadeSlow);
	REGISTER_ENUM (FadeConstantPower);
	REGISTER_ENUM (FadeSymmetric);
	REGISTER (_FadeShape);

	REGISTER_ENUM(None);
	REGISTER_ENUM(NewlyCreatedLeft);
	REGISTER_ENUM(NewlyCreatedRight);
	REGISTER_ENUM(NewlyCreatedBoth);
	REGISTER_ENUM(Existing);
	REGISTER_ENUM(ExistingNewlyCreatedLeft);
	REGISTER_ENUM(ExistingNewlyCreatedRight);
	REGISTER_ENUM(ExistingNewlyCreatedBoth);
	REGISTER (_RegionSelectionAfterSplit);
	REGISTER (_RangeSelectionAfterSplit);

	REGISTER_CLASS_ENUM (DiskIOProcessor, Recordable);
	REGISTER_CLASS_ENUM (DiskIOProcessor, Hidden);
	REGISTER_BITS (_DiskIOProcessor_Flag);

	REGISTER_CLASS_ENUM (Location, IsMark);
	REGISTER_CLASS_ENUM (Location, IsAutoPunch);
	REGISTER_CLASS_ENUM (Location, IsAutoLoop);
	REGISTER_CLASS_ENUM (Location, IsHidden);
	REGISTER_CLASS_ENUM (Location, IsCDMarker);
	REGISTER_CLASS_ENUM (Location, IsSessionRange);
	REGISTER_CLASS_ENUM (Location, IsRangeMarker);
	REGISTER_CLASS_ENUM (Location, IsSkip);
	REGISTER_CLASS_ENUM (Location, IsClockOrigin);
	REGISTER_CLASS_ENUM (Location, IsXrun);
	REGISTER_CLASS_ENUM (Location, IsCueMarker);
	REGISTER_BITS (_Location_Flags);

	REGISTER_CLASS_ENUM (Track, NoFreeze);
	REGISTER_CLASS_ENUM (Track, Frozen);
	REGISTER_CLASS_ENUM (Track, UnFrozen);
	REGISTER (_Track_FreezeState);

	REGISTER_CLASS_ENUM (AutomationList, Discrete);
	REGISTER_CLASS_ENUM (AutomationList, Linear);
	REGISTER_CLASS_ENUM (AutomationList, Curved);
	REGISTER_CLASS_ENUM (AutomationList, Logarithmic);
	REGISTER_CLASS_ENUM (AutomationList, Exponential);
	REGISTER (_AutomationList_InterpolationStyle);

	REGISTER_CLASS_ENUM (AnyTime, Timecode);
	REGISTER_CLASS_ENUM (AnyTime, BBT);
	REGISTER_CLASS_ENUM (AnyTime, Samples);
	REGISTER_CLASS_ENUM (AnyTime, Seconds);
	REGISTER (_AnyTime_Type);

	REGISTER_CLASS_ENUM (ExportFilename, D_None);
	REGISTER_CLASS_ENUM (ExportFilename, D_ISO);
	REGISTER_CLASS_ENUM (ExportFilename, D_ISOShortY);
	REGISTER_CLASS_ENUM (ExportFilename, D_BE);
	REGISTER_CLASS_ENUM (ExportFilename, D_BEShortY);
	REGISTER (_ExportFilename_DateFormat);

	REGISTER_CLASS_ENUM (ExportFilename, T_None);
	REGISTER_CLASS_ENUM (ExportFilename, T_NoDelim);
	REGISTER_CLASS_ENUM (ExportFilename, T_Delim);
	REGISTER (_ExportFilename_TimeFormat);

	REGISTER_CLASS_ENUM (ExportFormatBase, T_None);
	REGISTER_CLASS_ENUM (ExportFormatBase, T_Sndfile);
	REGISTER_CLASS_ENUM (ExportFormatBase, T_FFMPEG);
	REGISTER (_ExportFormatBase_Type);

	REGISTER_CLASS_ENUM (ExportFormatBase, F_None);
	REGISTER_CLASS_ENUM (ExportFormatBase, F_WAV);
	REGISTER_CLASS_ENUM (ExportFormatBase, F_W64);
	REGISTER_CLASS_ENUM (ExportFormatBase, F_AIFF);
	REGISTER_CLASS_ENUM (ExportFormatBase, F_AU);
	REGISTER_CLASS_ENUM (ExportFormatBase, F_IRCAM);
	REGISTER_CLASS_ENUM (ExportFormatBase, F_RAW);
	REGISTER_CLASS_ENUM (ExportFormatBase, F_FLAC);
	REGISTER_CLASS_ENUM (ExportFormatBase, F_Ogg);
	REGISTER_CLASS_ENUM (ExportFormatBase, F_CAF);
	REGISTER_CLASS_ENUM (ExportFormatBase, F_FFMPEG);
	REGISTER (_ExportFormatBase_FormatId);

	REGISTER_CLASS_ENUM (ExportFormatBase, E_FileDefault);
	REGISTER_CLASS_ENUM (ExportFormatBase, E_Little);
	REGISTER_CLASS_ENUM (ExportFormatBase, E_Big);
	REGISTER_CLASS_ENUM (ExportFormatBase, E_Cpu);
	REGISTER (_ExportFormatBase_Endianness);

	REGISTER_CLASS_ENUM (ExportFormatBase, SF_None);
	REGISTER_CLASS_ENUM (ExportFormatBase, SF_8);
	REGISTER_CLASS_ENUM (ExportFormatBase, SF_16);
	REGISTER_CLASS_ENUM (ExportFormatBase, SF_24);
	REGISTER_CLASS_ENUM (ExportFormatBase, SF_32);
	REGISTER_CLASS_ENUM (ExportFormatBase, SF_U8);
	REGISTER_CLASS_ENUM (ExportFormatBase, SF_Float);
	REGISTER_CLASS_ENUM (ExportFormatBase, SF_Double);
	REGISTER_CLASS_ENUM (ExportFormatBase, SF_Vorbis);
	REGISTER (_ExportFormatBase_SampleFormat);

	REGISTER_CLASS_ENUM (ExportFormatBase, D_None);
	REGISTER_CLASS_ENUM (ExportFormatBase, D_Rect);
	REGISTER_CLASS_ENUM (ExportFormatBase, D_Tri);
	REGISTER_CLASS_ENUM (ExportFormatBase, D_Shaped);
	REGISTER (_ExportFormatBase_DitherType);

	REGISTER_CLASS_ENUM (ExportFormatBase, Q_None);
	REGISTER_CLASS_ENUM (ExportFormatBase, Q_Any);
	REGISTER_CLASS_ENUM (ExportFormatBase, Q_LosslessLinear);
	REGISTER_CLASS_ENUM (ExportFormatBase, Q_LosslessCompression);
	REGISTER_CLASS_ENUM (ExportFormatBase, Q_LossyCompression);
	REGISTER (_ExportFormatBase_Quality);

	REGISTER_CLASS_ENUM (ExportFormatBase, SR_None);
	REGISTER_CLASS_ENUM (ExportFormatBase, SR_Session);
	REGISTER_CLASS_ENUM (ExportFormatBase, SR_8);
	REGISTER_CLASS_ENUM (ExportFormatBase, SR_22_05);
	REGISTER_CLASS_ENUM (ExportFormatBase, SR_44_1);
	REGISTER_CLASS_ENUM (ExportFormatBase, SR_48);
	REGISTER_CLASS_ENUM (ExportFormatBase, SR_88_2);
	REGISTER_CLASS_ENUM (ExportFormatBase, SR_96);
	REGISTER_CLASS_ENUM (ExportFormatBase, SR_192);
	REGISTER (_ExportFormatBase_SampleRate);

	REGISTER_CLASS_ENUM (ExportFormatBase, SRC_SincBest);
	REGISTER_CLASS_ENUM (ExportFormatBase, SRC_SincMedium);
	REGISTER_CLASS_ENUM (ExportFormatBase, SRC_SincFast);
	REGISTER_CLASS_ENUM (ExportFormatBase, SRC_ZeroOrderHold);
	REGISTER_CLASS_ENUM (ExportFormatBase, SRC_Linear);
	REGISTER (_ExportFormatBase_SRCQuality);

	REGISTER_CLASS_ENUM (ExportProfileManager, Timecode);
	REGISTER_CLASS_ENUM (ExportProfileManager, BBT);
	REGISTER_CLASS_ENUM (ExportProfileManager, MinSec);
	REGISTER_CLASS_ENUM (ExportProfileManager, Samples);
	REGISTER (_ExportProfileManager_TimeFormat);

	REGISTER_CLASS_ENUM (RegionExportChannelFactory, None);
	REGISTER_CLASS_ENUM (RegionExportChannelFactory, Raw);
	REGISTER_CLASS_ENUM (RegionExportChannelFactory, Fades);
	REGISTER (_RegionExportChannelFactory_Type);

	REGISTER_CLASS_ENUM (Delivery, Insert);
	REGISTER_CLASS_ENUM (Delivery, Send);
	REGISTER_CLASS_ENUM (Delivery, Listen);
	REGISTER_CLASS_ENUM (Delivery, Main);
	REGISTER_CLASS_ENUM (Delivery, Aux);
	REGISTER_CLASS_ENUM (Delivery, Foldback);
	REGISTER_BITS (_Delivery_Role);

	REGISTER_CLASS_ENUM (MuteMaster, PreFader);
	REGISTER_CLASS_ENUM (MuteMaster, PostFader);
	REGISTER_CLASS_ENUM (MuteMaster, Listen);
	REGISTER_CLASS_ENUM (MuteMaster, Main);
	REGISTER_BITS (_MuteMaster_MutePoint);

	REGISTER_CLASS_ENUM (IO, Input);
	REGISTER_CLASS_ENUM (IO, Output);
	REGISTER (_IO_Direction);

	REGISTER_CLASS_ENUM (MidiModel::NoteDiffCommand, NoteNumber);
	REGISTER_CLASS_ENUM (MidiModel::NoteDiffCommand, Channel);
	REGISTER_CLASS_ENUM (MidiModel::NoteDiffCommand, Velocity);
	REGISTER_CLASS_ENUM (MidiModel::NoteDiffCommand, StartTime);
	REGISTER_CLASS_ENUM (MidiModel::NoteDiffCommand, Length);
	REGISTER (_MidiModel_NoteDiffCommand_Property);

	REGISTER_CLASS_ENUM (MidiModel::SysExDiffCommand, Time);
	REGISTER (_MidiModel_SysExDiffCommand_Property);

	REGISTER_CLASS_ENUM (MidiModel::PatchChangeDiffCommand, Time);
	REGISTER_CLASS_ENUM (MidiModel::PatchChangeDiffCommand, Program);
	REGISTER_CLASS_ENUM (MidiModel::PatchChangeDiffCommand, Bank);
	REGISTER (_MidiModel_PatchChangeDiffCommand_Property);

	REGISTER_ENUM(MidiPortMusic);
	REGISTER_ENUM(MidiPortControl);
	REGISTER_ENUM(MidiPortSelection);
	REGISTER_BITS(_MidiPortFlags);

	REGISTER_ENUM(Exact);
	REGISTER_ENUM(Enclosed);
	REGISTER_ENUM(Overlap);
	REGISTER_ENUM(LayerTime);
	REGISTER(_RegionEquivalence);

	REGISTER_ENUM(Linear);
	REGISTER_ENUM(Logarithmic);
	REGISTER(_WaveformScale);

	REGISTER_ENUM(Traditional);
	REGISTER_ENUM(Rectified);
	REGISTER(_WaveformShape);

	REGISTER_ENUM(InhibitNever);
	REGISTER_ENUM(InhibitWhileRecording);
	REGISTER_ENUM(InhibitAlways);
	REGISTER(_ScreenSaverMode);

	REGISTER_ENUM (Small);
	REGISTER_ENUM (Medium);
	REGISTER_ENUM (Large);
	REGISTER_ENUM (Custom);
	REGISTER(_BufferingPreset);

	REGISTER_ENUM (LastLocate);
	REGISTER_ENUM (RangeSelectionStart);
	REGISTER_ENUM (Loop);
	REGISTER_ENUM (RegionSelectionStart);
	REGISTER_BITS (_AutoReturnTarget);

	REGISTER_CLASS_ENUM (PresentationInfo, AudioTrack);
	REGISTER_CLASS_ENUM (PresentationInfo, MidiTrack);
	REGISTER_CLASS_ENUM (PresentationInfo, AudioBus);
	REGISTER_CLASS_ENUM (PresentationInfo, MidiBus);
	REGISTER_CLASS_ENUM (PresentationInfo, VCA);
	REGISTER_CLASS_ENUM (PresentationInfo, MasterOut);
	REGISTER_CLASS_ENUM (PresentationInfo, MonitorOut);
	REGISTER_CLASS_ENUM (PresentationInfo, Auditioner);
	REGISTER_CLASS_ENUM (PresentationInfo, Hidden);
	REGISTER_CLASS_ENUM (PresentationInfo, OrderSet);
	REGISTER_CLASS_ENUM (PresentationInfo, FoldbackBus);
	REGISTER_CLASS_ENUM (PresentationInfo, TriggerTrack);
#ifdef MIXBUS
	REGISTER_CLASS_ENUM (PresentationInfo, MixbusEditorHidden);
#endif
	REGISTER_BITS (_PresentationInfo_Flag);

	REGISTER_CLASS_ENUM (MusicalMode,Dorian);
	REGISTER_CLASS_ENUM (MusicalMode, IonianMajor);
	REGISTER_CLASS_ENUM (MusicalMode, AeolianMinor);
	REGISTER_CLASS_ENUM (MusicalMode, HarmonicMinor);
	REGISTER_CLASS_ENUM (MusicalMode, MelodicMinorAscending);
	REGISTER_CLASS_ENUM (MusicalMode, MelodicMinorDescending);
	REGISTER_CLASS_ENUM (MusicalMode, Phrygian);
	REGISTER_CLASS_ENUM (MusicalMode, Lydian);
	REGISTER_CLASS_ENUM (MusicalMode, Mixolydian);
	REGISTER_CLASS_ENUM (MusicalMode, Locrian);
	REGISTER_CLASS_ENUM (MusicalMode, PentatonicMajor);
	REGISTER_CLASS_ENUM (MusicalMode, PentatonicMinor);
	REGISTER_CLASS_ENUM (MusicalMode, Chromatic);
	REGISTER_CLASS_ENUM (MusicalMode, BluesScale);
	REGISTER_CLASS_ENUM (MusicalMode, NeapolitanMinor);
	REGISTER_CLASS_ENUM (MusicalMode, NeapolitanMajor);
	REGISTER_CLASS_ENUM (MusicalMode, Oriental);
	REGISTER_CLASS_ENUM (MusicalMode, DoubleHarmonic);
	REGISTER_CLASS_ENUM (MusicalMode, Enigmatic);
	REGISTER_CLASS_ENUM (MusicalMode, Hirajoshi);
	REGISTER_CLASS_ENUM (MusicalMode, HungarianMinor);
	REGISTER_CLASS_ENUM (MusicalMode, HungarianMajor);
	REGISTER_CLASS_ENUM (MusicalMode, Kumoi);
	REGISTER_CLASS_ENUM (MusicalMode, Iwato);
	REGISTER_CLASS_ENUM (MusicalMode, Hindu);
	REGISTER_CLASS_ENUM (MusicalMode, Spanish8Tone);
	REGISTER_CLASS_ENUM (MusicalMode, Pelog);
	REGISTER_CLASS_ENUM (MusicalMode, HungarianGypsy);
	REGISTER_CLASS_ENUM (MusicalMode, Overtone);
	REGISTER_CLASS_ENUM (MusicalMode, LeadingWholeTone);
	REGISTER_CLASS_ENUM (MusicalMode, Arabian);
	REGISTER_CLASS_ENUM (MusicalMode, Balinese);
	REGISTER_CLASS_ENUM (MusicalMode, Gypsy);
	REGISTER_CLASS_ENUM (MusicalMode, Mohammedan);
	REGISTER_CLASS_ENUM (MusicalMode, Javanese);
	REGISTER_CLASS_ENUM (MusicalMode, Persian);
	REGISTER_CLASS_ENUM (MusicalMode, Algerian);
	REGISTER (mode);

	REGISTER_CLASS_ENUM (TransportFSM, ButlerDone);
	REGISTER_CLASS_ENUM (TransportFSM, ButlerRequired);
	REGISTER_CLASS_ENUM (TransportFSM, DeclickDone);
	REGISTER_CLASS_ENUM (TransportFSM, StartTransport);
	REGISTER_CLASS_ENUM (TransportFSM, StopTransport);
	REGISTER_CLASS_ENUM (TransportFSM, Locate);
	REGISTER_CLASS_ENUM (TransportFSM, LocateDone);
	REGISTER_CLASS_ENUM (TransportFSM, SetSpeed);
	REGISTER (_TransportFSM_EventType);

	REGISTER_CLASS_ENUM (TransportFSM, Stopped);
	REGISTER_CLASS_ENUM (TransportFSM, Rolling);
	REGISTER_CLASS_ENUM (TransportFSM, DeclickToStop);
	REGISTER_CLASS_ENUM (TransportFSM, DeclickToLocate);
	REGISTER_CLASS_ENUM (TransportFSM, WaitingForLocate);
	REGISTER (_TransportFSM_MotionState);

	REGISTER_CLASS_ENUM (TransportFSM, NotWaitingForButler);
	REGISTER_CLASS_ENUM (TransportFSM, WaitingForButler);
	REGISTER (_TransportFSM_ButlerState);

	REGISTER_CLASS_ENUM (TransportFSM, Forwards);
	REGISTER_CLASS_ENUM (TransportFSM, Backwards);
	REGISTER_CLASS_ENUM (TransportFSM, Reversing);
	REGISTER (_TransportFSM_DirectionState);

	REGISTER_ENUM (NoLoopFade);
	REGISTER_ENUM (EndLoopFade);
	REGISTER_ENUM (BothLoopFade);
	REGISTER_ENUM (XFadeLoop);
	REGISTER (_LoopFadeChooice);

	REGISTER_ENUM (TransportStopped);
	REGISTER_ENUM (TransportRolling);
	REGISTER_ENUM (TransportLooping);
	REGISTER_ENUM (TransportStarting);
	REGISTER (_TransportState);

	REGISTER_ENUM (MustStop);
	REGISTER_ENUM (MustRoll);
	REGISTER_ENUM (RollIfAppropriate);
	REGISTER (_LocateTransportDisposition);

	REGISTER_CLASS_ENUM (Trigger, Stopped);
	REGISTER_CLASS_ENUM (Trigger, WaitingToStart);
	REGISTER_CLASS_ENUM (Trigger, Running);
	REGISTER_CLASS_ENUM (Trigger, Playout);
	REGISTER_CLASS_ENUM (Trigger, WaitingForRetrigger);
	REGISTER_CLASS_ENUM (Trigger, WaitingToStop);
	REGISTER_CLASS_ENUM (Trigger, Stopping);
	REGISTER (_TriggerState);

	REGISTER_CLASS_ENUM (FollowAction, None);
	REGISTER_CLASS_ENUM (FollowAction, Stop);
	REGISTER_CLASS_ENUM (FollowAction, Again);
	REGISTER_CLASS_ENUM (FollowAction, ForwardTrigger);
	REGISTER_CLASS_ENUM (FollowAction, ReverseTrigger);
	REGISTER_CLASS_ENUM (FollowAction, FirstTrigger);
	REGISTER_CLASS_ENUM (FollowAction, LastTrigger);
	REGISTER_CLASS_ENUM (FollowAction, JumpTrigger);
	REGISTER (_FollowAction);

	REGISTER_CLASS_ENUM (Trigger, OneShot);
	REGISTER_CLASS_ENUM (Trigger, ReTrigger);
	REGISTER_CLASS_ENUM (Trigger, Gate);
	REGISTER_CLASS_ENUM (Trigger, Toggle);
	REGISTER_CLASS_ENUM (Trigger, Repeat);
	REGISTER (_TriggerLaunchStyle);

	REGISTER_CLASS_ENUM (Trigger, Crisp);
	REGISTER_CLASS_ENUM (Trigger, Mixed);
	REGISTER_CLASS_ENUM (Trigger, Smooth);
	REGISTER (_TriggerStretchMode);

	REGISTER_ENUM (FollowCues);
	REGISTER_ENUM (ImplicitlyIgnoreCues);
	REGISTER_BITS (_CueBehavior);
}

} /* namespace ARDOUR */
