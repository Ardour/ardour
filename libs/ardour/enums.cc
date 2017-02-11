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

#include "pbd/enumwriter.h"
#include "midi++/types.h"

#include "evoral/Range.hpp" // shouldn't Evoral have its own enum registration?

#include "ardour/delivery.h"
#include "ardour/diskstream.h"
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
	MeterType _MeterType;
	TrackMode _TrackMode;
	NoteMode _NoteMode;
	ChannelMode _ChannelMode;
	ColorMode _ColorMode;
	LocaleMode _LocaleMode;
	MeterFalloff _MeterFalloff;
	MeterHold _MeterHold;
	VUMeterStandard _VUMeterStandard;
	MeterLineUp _MeterLineUp;
	EditMode _EditMode;
	RegionPoint _RegionPoint;
	Placement _Placement;
	MonitorModel _MonitorModel;
	MonitorChoice _MonitorChoice;
	MonitorState _MonitorState;
	PFLPosition _PFLPosition;
	AFLPosition _AFLPosition;
	DenormalModel _DenormalModel;
	LayerModel _LayerModel;
	InsertMergePolicy _InsertMergePolicy;
	ListenPosition _ListenPosition;
	SampleFormat _SampleFormat;
	CDMarkerFormat _CDMarkerFormat;
	HeaderFormat _HeaderFormat;
	PluginType _PluginType;
	SyncSource _SyncSource;
	ShuttleBehaviour _ShuttleBehaviour;
	ShuttleUnits _ShuttleUnits;
	Session::RecordState _Session_RecordState;
	SessionEvent::Type _SessionEvent_Type;
	SessionEvent::Action _SessionEvent_Action;
	TimecodeFormat _Session_TimecodeFormat;
	Session::PullupFormat _Session_PullupFormat;
	FadeShape _FadeShape;
	RegionSelectionAfterSplit _RegionSelectionAfterSplit;
	IOChange _IOChange;
	AutomationType _AutomationType;
	AutoState _AutoState;
	AutoStyle _AutoStyle;
	AutoConnectOption _AutoConnectOption;
	TracksAutoNamingRule _TracksAutoNamingRule;
	Session::StateOfTheState _Session_StateOfTheState;
	Source::Flag _Source_Flag;
	Diskstream::Flag _Diskstream_Flag;
	Location::Flags _Location_Flags;
	PositionLockStyle _PositionLockStyle;
	TempoSection::Type _TempoSection_Type;
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
	WaveformScale _WaveformScale;
	WaveformShape _WaveformShape;
	Session::PostTransportWork _Session_PostTransportWork;
	Session::SlaveState _Session_SlaveState;
	MTC_Status _MIDI_MTC_Status;
	Evoral::OverlapType _OverlapType;
        BufferingPreset _BufferingPreset;
	AutoReturnTarget _AutoReturnTarget;
	PresentationInfo::Flag _PresentationInfo_Flag;
	MusicalMode::Type mode;
	MidiPortFlags _MidiPortFlags;

#define REGISTER(e) enum_writer.register_distinct (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_BITS(e) enum_writer.register_bits (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_ENUM(e) i.push_back (e); s.push_back (#e)
#define REGISTER_CLASS_ENUM(t,e) i.push_back (t::e); s.push_back (#e)

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
	REGISTER_ENUM (EQGain);
	REGISTER_ENUM (EQFrequency);
	REGISTER_ENUM (EQQ);
	REGISTER_ENUM (EQShape);
	REGISTER_ENUM (EQHPF);
	REGISTER_ENUM (EQEnable);
	REGISTER_ENUM (CompThreshold);
	REGISTER_ENUM (CompSpeed);
	REGISTER_ENUM (CompMode);
	REGISTER_ENUM (CompMakeup);
	REGISTER_ENUM (CompRedux);
	REGISTER_ENUM (CompEnable);
	REGISTER_ENUM (BusSendLevel);
	REGISTER_ENUM (BusSendEnable);
	REGISTER (_AutomationType);

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

	REGISTER_ENUM (SET_LC_ALL);
	REGISTER_ENUM (SET_LC_MESSAGES);
	REGISTER_ENUM (SET_LC_MESSAGES_AND_LC_NUMERIC);
	REGISTER (_LocaleMode);

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

	REGISTER_ENUM (Slide);
	REGISTER_ENUM (Splice);
	REGISTER_ENUM (Ripple); // XXX do the old enum values have to stay in order?
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
	REGISTER (_HeaderFormat);

	REGISTER_ENUM (AudioUnit);
	REGISTER_ENUM (LADSPA);
	REGISTER_ENUM (LV2);
	REGISTER_ENUM (Windows_VST);
	REGISTER_ENUM (LXVST);
	REGISTER_ENUM (MacVST);
	REGISTER_ENUM (Lua);
	REGISTER (_PluginType);

	REGISTER_ENUM (MTC);
	REGISTER_ENUM (JACK);
	REGISTER_ENUM (Engine);
	REGISTER_ENUM (MIDIClock);
	REGISTER_ENUM (LTC);
	REGISTER (_SyncSource);

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

	REGISTER_CLASS_ENUM (SessionEvent, SetTransportSpeed);
	REGISTER_CLASS_ENUM (SessionEvent, SetTrackSpeed);
	REGISTER_CLASS_ENUM (SessionEvent, Locate);
	REGISTER_CLASS_ENUM (SessionEvent, LocateRoll);
	REGISTER_CLASS_ENUM (SessionEvent, LocateRollLocate);
	REGISTER_CLASS_ENUM (SessionEvent, SetLoop);
	REGISTER_CLASS_ENUM (SessionEvent, PunchIn);
	REGISTER_CLASS_ENUM (SessionEvent, PunchOut);
	REGISTER_CLASS_ENUM (SessionEvent, RangeStop);
	REGISTER_CLASS_ENUM (SessionEvent, RangeLocate);
	REGISTER_CLASS_ENUM (SessionEvent, Overwrite);
	REGISTER_CLASS_ENUM (SessionEvent, SetSyncSource);
	REGISTER_CLASS_ENUM (SessionEvent, Audition);
	REGISTER_CLASS_ENUM (SessionEvent, InputConfigurationChange);
	REGISTER_CLASS_ENUM (SessionEvent, SetPlayAudioRange);
	REGISTER_CLASS_ENUM (SessionEvent, CancelPlayAudioRange);
	REGISTER_CLASS_ENUM (SessionEvent, RealTimeOperation);
	REGISTER_CLASS_ENUM (SessionEvent, AdjustPlaybackBuffering);
	REGISTER_CLASS_ENUM (SessionEvent, AdjustCaptureBuffering);
	REGISTER_CLASS_ENUM (SessionEvent, SetTimecodeTransmission);
	REGISTER_CLASS_ENUM (SessionEvent, Skip);
	REGISTER_CLASS_ENUM (SessionEvent, StopOnce);
	REGISTER_CLASS_ENUM (SessionEvent, AutoLoop);
	REGISTER_CLASS_ENUM (SessionEvent, AutoLoopDeclick);
	REGISTER (_SessionEvent_Type);

	REGISTER_CLASS_ENUM (SessionEvent, Add);
	REGISTER_CLASS_ENUM (SessionEvent, Remove);
	REGISTER_CLASS_ENUM (SessionEvent, Replace);
	REGISTER_CLASS_ENUM (SessionEvent, Clear);
	REGISTER (_SessionEvent_Action);

	REGISTER_CLASS_ENUM (Session, Stopped);
	REGISTER_CLASS_ENUM (Session, Waiting);
	REGISTER_CLASS_ENUM (Session, Running);
	REGISTER (_Session_SlaveState);

	REGISTER_ENUM (MTC_Stopped);
	REGISTER_ENUM (MTC_Forward);
	REGISTER_ENUM (MTC_Backward);
	REGISTER (_MIDI_MTC_Status);

	REGISTER_CLASS_ENUM (Session, PostTransportStop);
	REGISTER_CLASS_ENUM (Session, PostTransportDuration);
	REGISTER_CLASS_ENUM (Session, PostTransportLocate);
	REGISTER_CLASS_ENUM (Session, PostTransportRoll);
	REGISTER_CLASS_ENUM (Session, PostTransportAbort);
	REGISTER_CLASS_ENUM (Session, PostTransportOverWrite);
	REGISTER_CLASS_ENUM (Session, PostTransportSpeed);
	REGISTER_CLASS_ENUM (Session, PostTransportAudition);
	REGISTER_CLASS_ENUM (Session, PostTransportReverse);
	REGISTER_CLASS_ENUM (Session, PostTransportInputChange);
	REGISTER_CLASS_ENUM (Session, PostTransportCurveRealloc);
	REGISTER_CLASS_ENUM (Session, PostTransportClearSubstate);
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

	REGISTER_CLASS_ENUM (Diskstream, Recordable);
	REGISTER_CLASS_ENUM (Diskstream, Hidden);
	REGISTER_CLASS_ENUM (Diskstream, Destructive);
	REGISTER_BITS (_Diskstream_Flag);

	REGISTER_CLASS_ENUM (Location, IsMark);
	REGISTER_CLASS_ENUM (Location, IsAutoPunch);
	REGISTER_CLASS_ENUM (Location, IsAutoLoop);
	REGISTER_CLASS_ENUM (Location, IsHidden);
	REGISTER_CLASS_ENUM (Location, IsCDMarker);
	REGISTER_CLASS_ENUM (Location, IsSessionRange);
	REGISTER_CLASS_ENUM (Location, IsRangeMarker);
	REGISTER_CLASS_ENUM (Location, IsSkip);
	REGISTER_BITS (_Location_Flags);

	REGISTER_CLASS_ENUM (TempoSection, Ramp);
	REGISTER_CLASS_ENUM (TempoSection, Constant);
	REGISTER (_TempoSection_Type);

	REGISTER_CLASS_ENUM (Track, NoFreeze);
	REGISTER_CLASS_ENUM (Track, Frozen);
	REGISTER_CLASS_ENUM (Track, UnFrozen);
	REGISTER (_Track_FreezeState);

	REGISTER_CLASS_ENUM (AutomationList, Discrete);
	REGISTER_CLASS_ENUM (AutomationList, Linear);
	REGISTER_CLASS_ENUM (AutomationList, Curved);
	REGISTER (_AutomationList_InterpolationStyle);

	REGISTER_CLASS_ENUM (AnyTime, Timecode);
	REGISTER_CLASS_ENUM (AnyTime, BBT);
	REGISTER_CLASS_ENUM (AnyTime, Frames);
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
	REGISTER_CLASS_ENUM (ExportProfileManager, Frames);
	REGISTER (_ExportProfileManager_TimeFormat);

	REGISTER_CLASS_ENUM (RegionExportChannelFactory, None);
	REGISTER_CLASS_ENUM (RegionExportChannelFactory, Raw);
	REGISTER_CLASS_ENUM (RegionExportChannelFactory, Fades);
	REGISTER_CLASS_ENUM (RegionExportChannelFactory, Processed);
	REGISTER (_RegionExportChannelFactory_Type);

	REGISTER_CLASS_ENUM (Delivery, Insert);
	REGISTER_CLASS_ENUM (Delivery, Send);
	REGISTER_CLASS_ENUM (Delivery, Listen);
	REGISTER_CLASS_ENUM (Delivery, Main);
	REGISTER_CLASS_ENUM (Delivery, Aux);
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

	REGISTER_ENUM(Linear);
	REGISTER_ENUM(Logarithmic);
	REGISTER(_WaveformScale);

	REGISTER_ENUM(Traditional);
	REGISTER_ENUM(Rectified);
	REGISTER(_WaveformShape);

	REGISTER_ENUM(AudioTime);
	REGISTER_ENUM(MusicTime);
	REGISTER(_PositionLockStyle);

	REGISTER_ENUM (Evoral::OverlapNone);
	REGISTER_ENUM (Evoral::OverlapInternal);
	REGISTER_ENUM (Evoral::OverlapStart);
	REGISTER_ENUM (Evoral::OverlapEnd);
	REGISTER_ENUM (Evoral::OverlapExternal);
	REGISTER(_OverlapType);

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
	REGISTER_CLASS_ENUM (PresentationInfo, Selected);
	REGISTER_CLASS_ENUM (PresentationInfo, Hidden);
	REGISTER_CLASS_ENUM (PresentationInfo, OrderSet);
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
}

} /* namespace ARDOUR */

/* deserializing types from ardour/types.h */

std::istream& operator>>(std::istream& o, HeaderFormat& var)
{
	std::string s;
	o >> s;
	var = (HeaderFormat) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const HeaderFormat& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, SampleFormat& var)
{
	std::string s;
	o >> s;
	var = (SampleFormat) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const SampleFormat& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, AutoConnectOption& var)
{
	std::string s;
	o >> s;
	var = (AutoConnectOption) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const AutoConnectOption& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, TracksAutoNamingRule& var)
{
	std::string s;
	o >> s;
	var = (TracksAutoNamingRule) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const TracksAutoNamingRule& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, MonitorChoice& var)
{
	std::string s;
	o >> s;
	var = (MonitorChoice) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const MonitorChoice& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, MonitorModel& var)
{
	std::string s;
	o >> s;
	var = (MonitorModel) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const MonitorModel& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, VUMeterStandard& var)
{
	std::string s;
	o >> s;
	var = (VUMeterStandard) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const VUMeterStandard& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, MeterLineUp& var)
{
	std::string s;
	o >> s;
	var = (MeterLineUp) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const MeterLineUp& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, LocaleMode& var)
{
	std::string s;
	o >> s;
	var = (LocaleMode) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const LocaleMode& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, PFLPosition& var)
{
	std::string s;
	o >> s;
	var = (PFLPosition) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const PFLPosition& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, AFLPosition& var)
{
	std::string s;
	o >> s;
	var = (AFLPosition) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const AFLPosition& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, EditMode& var)
{
	std::string s;
	o >> s;
	var = (EditMode) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const EditMode& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}
std::istream& operator>>(std::istream& o, ListenPosition& var)
{
	std::string s;
	o >> s;
	var = (ListenPosition) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const ListenPosition& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}
std::istream& operator>>(std::istream& o, LayerModel& var)
{
	std::string s;
	o >> s;
	var = (LayerModel) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const LayerModel& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, InsertMergePolicy& var)
{
	std::string s;
	o >> s;
	var = (InsertMergePolicy) string_2_enum (s, var);
	return o;
}
std::ostream& operator<<(std::ostream& o, const InsertMergePolicy& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, SyncSource& var)
{
	std::string s;
	o >> s;
	var = (SyncSource) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const SyncSource& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}
std::istream& operator>>(std::istream& o, ShuttleBehaviour& var)
{
	std::string s;
	o >> s;
	var = (ShuttleBehaviour) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const ShuttleBehaviour& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}
std::istream& operator>>(std::istream& o, ShuttleUnits& var)
{
	std::string s;
	o >> s;
	var = (ShuttleUnits) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const ShuttleUnits& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}
std::istream& operator>>(std::istream& o, TimecodeFormat& var)
{
	std::string s;
	o >> s;
	var = (TimecodeFormat) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const TimecodeFormat& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}
std::istream& operator>>(std::istream& o, DenormalModel& var)
{
	std::string s;
	o >> s;
	var = (DenormalModel) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const DenormalModel& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}
std::istream& operator>>(std::istream& o, WaveformScale& var)
{
	std::string s;
	o >> s;
	var = (WaveformScale) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const WaveformScale& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}
std::istream& operator>>(std::istream& o, WaveformShape& var)
{
	std::string s;
	o >> s;
	var = (WaveformShape) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const WaveformShape& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, PositionLockStyle& var)
{
	std::string s;
	o >> s;
	var = (PositionLockStyle) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const PositionLockStyle& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, Evoral::OverlapType& var)
{
	std::string s;
	o >> s;
	var = (Evoral::OverlapType) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const Evoral::OverlapType& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, FadeShape& var)
{
	std::string s;
	o >> s;
	var = (FadeShape) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const FadeShape& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, RegionSelectionAfterSplit& var)
{
	std::string s;
	o >> s;
	var = (RegionSelectionAfterSplit) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const RegionSelectionAfterSplit& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, ARDOUR::BufferingPreset& var)
{
	std::string s;
	o >> s;
	var = (ARDOUR::BufferingPreset) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const ARDOUR::BufferingPreset& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, AutoReturnTarget& var)
{
	std::string s;
	o >> s;
	var = (AutoReturnTarget) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const AutoReturnTarget& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}

std::istream& operator>>(std::istream& o, MeterType& var)
{
	std::string s;
	o >> s;
	var = (MeterType) string_2_enum (s, var);
	return o;
}

std::ostream& operator<<(std::ostream& o, const MeterType& var)
{
	std::string s = enum_2_string (var);
	return o << s;
}
