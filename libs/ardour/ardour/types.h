/*
    Copyright (C) 2002 Paul Davis

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

#ifndef __ardour_types_h__
#define __ardour_types_h__

#include <istream>
#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include <sys/types.h>
#include <stdint.h>
#include <pthread.h>

#include <inttypes.h>

#include "timecode/bbt_time.h"
#include "timecode/time.h"

#include "pbd/id.h"

#include "evoral/Range.hpp"

#include "ardour/chan_count.h"
#include "ardour/plugin_types.h"

#include <map>

#if __GNUC__ < 3
typedef int intptr_t;
#endif

namespace ARDOUR {

	class Source;
	class AudioSource;
	class Route;
	class Region;
	class Stripable;
	class VCA;
	class AutomationControl;

	typedef float    Sample;
	typedef float    pan_t;
	typedef float    gain_t;
	typedef uint32_t layer_t;
	typedef uint64_t microseconds_t;
	typedef uint32_t pframes_t;

	/* Any position measured in audio frames.
	   Assumed to be non-negative but not enforced.
	*/
	typedef int64_t framepos_t;

	/* Any distance from a given framepos_t.
	   Maybe positive or negative.
	*/
	typedef int64_t frameoffset_t;

	/* Any count of audio frames.
	   Assumed to be positive but not enforced.
	*/
	typedef int64_t framecnt_t;

	static const framepos_t max_framepos = INT64_MAX;
	static const framecnt_t max_framecnt = INT64_MAX;
	static const layer_t    max_layer    = UINT32_MAX;

	// a set of (time) intervals: first of pair is the offset of the start within the region, second is the offset of the end
	typedef std::list<std::pair<frameoffset_t, frameoffset_t> > AudioIntervalResult;
	// associate a set of intervals with regions (e.g. for silence detection)
	typedef std::map<boost::shared_ptr<ARDOUR::Region>,AudioIntervalResult> AudioIntervalMap;

	typedef std::list<boost::shared_ptr<Region> > RegionList;

	struct IOChange {

		enum Type {
			NoChange = 0,
			ConfigurationChanged = 0x1,
			ConnectionsChanged = 0x2
		} type;

		IOChange () : type (NoChange) {}
		IOChange (Type t) : type (t) {}

		/** channel count of IO before a ConfigurationChanged, if appropriate */
		ARDOUR::ChanCount before;
		/** channel count of IO after a ConfigurationChanged, if appropriate */
		ARDOUR::ChanCount after;
	};

	/* policies for inserting/pasting material where overlaps
	   might be an issue.
	*/

	enum InsertMergePolicy {
		InsertMergeReject,  ///< no overlaps allowed
		InsertMergeRelax,   ///< we just don't care about overlaps
		InsertMergeReplace, ///< replace old with new
		InsertMergeTruncateExisting, ///< shorten existing to avoid overlap
		InsertMergeTruncateAddition, ///< shorten new to avoid overlap
		InsertMergeExtend   ///< extend new (or old) to the range of old+new
	};

	/** See evoral/Parameter.hpp
	 *
	 * When you add things here, you REALLY SHOULD add a case clause to
	 * the constructor of ParameterDescriptor, unless the Controllables
	 * that the enum refers to are completely standard (0-1.0 range, 0.0 as
	 * normal, non-toggled, non-enumerated). Anything else needs to be
	 * added there so that things that try to represent them can do so
	 * with as much information as possible.
	 */
	enum AutomationType {
		NullAutomation,
		GainAutomation,
		PanAzimuthAutomation,
		PanElevationAutomation,
		PanWidthAutomation,
		PanFrontBackAutomation,
		PanLFEAutomation,
		PluginAutomation,
		PluginPropertyAutomation,
		SoloAutomation,
		SoloIsolateAutomation,
		SoloSafeAutomation,
		MuteAutomation,
		MidiCCAutomation,
		MidiPgmChangeAutomation,
		MidiPitchBenderAutomation,
		MidiChannelPressureAutomation,
		MidiNotePressureAutomation,
		MidiSystemExclusiveAutomation,
		FadeInAutomation,
		FadeOutAutomation,
		EnvelopeAutomation,
		RecEnableAutomation,
		RecSafeAutomation,
		TrimAutomation,
		PhaseAutomation,
		MonitoringAutomation,
		EQGain,
		EQFrequency,
		EQQ,
		EQShape,
		EQHPF,
		EQEnable,
		CompThreshold,
		CompSpeed,
		CompMode,
		CompMakeup,
		CompRedux,
		CompEnable,
		BusSendLevel,
		BusSendEnable,
	};

	enum AutoState {
		Off = 0x0,
		Write = 0x1,
		Touch = 0x2,
		Play = 0x4
	};

	std::string auto_state_to_string (AutoState);
	AutoState string_to_auto_state (std::string);

	enum AutoStyle {
		Absolute = 0x1,
		Trim = 0x2
	};

	std::string auto_style_to_string (AutoStyle);
	AutoStyle string_to_auto_style (std::string);

	enum AlignStyle {
		CaptureTime,
		ExistingMaterial
	};

	enum AlignChoice {
		UseCaptureTime,
		UseExistingMaterial,
		Automatic
	};

	enum MeterPoint {
		MeterInput,
		MeterPreFader,
		MeterPostFader,
		MeterOutput,
		MeterCustom
	};

	enum MeterType {
		MeterMaxSignal = 0x0001,
		MeterMaxPeak   = 0x0002,
		MeterPeak      = 0x0004,
		MeterKrms      = 0x0008,
		MeterK20       = 0x0010,
		MeterK14       = 0x0020,
		MeterIEC1DIN   = 0x0040,
		MeterIEC1NOR   = 0x0080,
		MeterIEC2BBC   = 0x0100,
		MeterIEC2EBU   = 0x0200,
		MeterVU        = 0x0400,
		MeterK12       = 0x0800,
		MeterPeak0dB   = 0x1000,
		MeterMCP       = 0x2000
	};

	enum TrackMode {
		Normal,
		NonLayered,
		Destructive
	};

	enum NoteMode {
		Sustained,
		Percussive
	};

	enum ChannelMode {
		AllChannels = 0, ///< Pass through all channel information unmodified
		FilterChannels,  ///< Ignore events on certain channels
		ForceChannel     ///< Force all events to a certain channel
	};

	enum ColorMode {
		MeterColors = 0,
		ChannelColors,
		TrackColor
	};

	enum LocaleMode {
		SET_LC_ALL,
		SET_LC_MESSAGES,
		SET_LC_MESSAGES_AND_LC_NUMERIC
	};

	enum RoundMode {
		RoundDownMaybe  = -2,  ///< Round down only if necessary
		RoundDownAlways = -1,  ///< Always round down, even if on a division
		RoundNearest    = 0,   ///< Round to nearest
		RoundUpAlways   = 1,   ///< Always round up, even if on a division
		RoundUpMaybe    = 2    ///< Round up only if necessary
	};

	class AnyTime {
	public:
		enum Type {
			Timecode,
			BBT,
			Frames,
			Seconds
		};

		Type type;

		Timecode::Time     timecode;
		Timecode::BBT_Time bbt;

		union {
			framecnt_t     frames;
			double         seconds;
		};

		AnyTime() { type = Frames; frames = 0; }

		bool operator== (AnyTime const & other) const {
			if (type != other.type) { return false; }

			switch (type) {
			  case Timecode:
				return timecode == other.timecode;
			  case BBT:
				return bbt == other.bbt;
			  case Frames:
				return frames == other.frames;
			  case Seconds:
				return seconds == other.seconds;
			}
			return false; // get rid of warning
		}

		bool not_zero() const
		{
			switch (type) {
			  case Timecode:
				return timecode.hours != 0 || timecode.minutes != 0 ||
				       timecode.seconds != 0 || timecode.frames != 0;
			  case BBT:
				return bbt.bars != 0 || bbt.beats != 0 || bbt.ticks != 0;
			  case Frames:
				return frames != 0;
			  case Seconds:
				return seconds != 0;
			}

			abort(); /* NOTREACHED */
			return false;
		}
	};

	/* used for translating audio frames to an exact musical position using a note divisor.
	   an exact musical position almost never falls exactly on an audio frame, but for sub-sample
	   musical accuracy we need to derive exact musical locations from a frame position
	   the division follows TempoMap::exact_beat_at_frame().
	   division
	   -1       musical location is the bar closest to frame
	    0       musical location is the musical position of the frame
	    1       musical location is the BBT beat closest to frame
	    n       musical location is the quarter-note division n closest to frame
	*/
	struct MusicFrame {
		framepos_t frame;
		int32_t    division;

		MusicFrame (framepos_t f, int32_t d) : frame (f), division (d) {}

		void set (framepos_t f, int32_t d) {frame = f; division = d; }

		MusicFrame operator- (MusicFrame other) { return MusicFrame (frame - other.frame, 0); }
	};

	/* XXX: slightly unfortunate that there is this and Evoral::Range<>,
	   but this has a uint32_t id which Evoral::Range<> does not.
	*/
	struct AudioRange {
		framepos_t start;
		framepos_t end;
		uint32_t id;

		AudioRange (framepos_t s, framepos_t e, uint32_t i) : start (s), end (e) , id (i) {}

		framecnt_t length() const { return end - start + 1; }

		bool operator== (const AudioRange& other) const {
			return start == other.start && end == other.end && id == other.id;
		}

		bool equal (const AudioRange& other) const {
			return start == other.start && end == other.end;
		}

		Evoral::OverlapType coverage (framepos_t s, framepos_t e) const {
			return Evoral::coverage (start, end, s, e);
		}
	};

	struct MusicRange {
		Timecode::BBT_Time start;
		Timecode::BBT_Time end;
		uint32_t id;

		MusicRange (Timecode::BBT_Time& s, Timecode::BBT_Time& e, uint32_t i)
			: start (s), end (e), id (i) {}

		bool operator== (const MusicRange& other) const {
			return start == other.start && end == other.end && id == other.id;
		}

		bool equal (const MusicRange& other) const {
			return start == other.start && end == other.end;
		}
	};

	/*
	    Slowest = 6.6dB/sec falloff at update rate of 40ms
	    Slow    = 6.8dB/sec falloff at update rate of 40ms
	*/

	enum MeterFalloff {
		MeterFalloffOff = 0,
		MeterFalloffSlowest = 1,
		MeterFalloffSlow = 2,
		MeterFalloffSlowish = 3,
		MeterFalloffModerate = 4,
		MeterFalloffMedium = 5,
		MeterFalloffFast = 6,
		MeterFalloffFaster = 7,
		MeterFalloffFastest = 8,
	};

	enum MeterHold {
		MeterHoldOff = 0,
		MeterHoldShort = 40,
		MeterHoldMedium = 100,
		MeterHoldLong = 200
	};

	enum EditMode {
		Slide,
		Splice,
		Ripple,
		Lock
	};

	enum RegionSelectionAfterSplit {
		None = 0,
		NewlyCreatedLeft = 1,  // bit 0
		NewlyCreatedRight = 2, // bit 1
		NewlyCreatedBoth = 3,
		Existing = 4,          // bit 2
		ExistingNewlyCreatedLeft = 5,
		ExistingNewlyCreatedRight = 6,
		ExistingNewlyCreatedBoth = 7
	};

	enum RegionPoint {
		Start,
		End,
		SyncPoint
	};

	enum Placement {
		PreFader,
		PostFader
	};

	enum MonitorModel {
		HardwareMonitoring, ///< JACK does monitoring
		SoftwareMonitoring, ///< Ardour does monitoring
		ExternalMonitoring  ///< we leave monitoring to the audio hardware
	};

	enum MonitorChoice {
		MonitorAuto = 0,
		MonitorInput = 0x1,
		MonitorDisk = 0x2,
		MonitorCue = 0x4,
	};

	enum MonitorState {
		MonitoringSilence = 0x1,
		MonitoringInput = 0x2,
		MonitoringDisk = 0x4,
	};

	enum MeterState {
		MeteringInput, ///< meter the input IO, regardless of what is going through the route
		MeteringRoute  ///< meter what is going through the route
	};

	enum VUMeterStandard {
		MeteringVUfrench,   // 0VU = -2dBu
		MeteringVUamerican, // 0VU =  0dBu
		MeteringVUstandard, // 0VU = +4dBu
		MeteringVUeight     // 0VU = +8dBu
	};

	enum MeterLineUp {
		MeteringLineUp24,
		MeteringLineUp20,
		MeteringLineUp18,
		MeteringLineUp15
	};

	enum PFLPosition {
		/** PFL signals come from before pre-fader processors */
		PFLFromBeforeProcessors,
		/** PFL signals come pre-fader but after pre-fader processors */
		PFLFromAfterProcessors
	};

	enum AFLPosition {
		/** AFL signals come post-fader and before post-fader processors */
		AFLFromBeforeProcessors,
		/** AFL signals come post-fader but after post-fader processors */
		AFLFromAfterProcessors
	};

	enum DenormalModel {
		DenormalNone,
		DenormalFTZ,
		DenormalDAZ,
		DenormalFTZDAZ
	};

	enum LayerModel {
		LaterHigher,
		Manual
	};

	enum ListenPosition {
		AfterFaderListen,
		PreFaderListen
	};

	enum AutoConnectOption {
		ManualConnect = 0x0,
		AutoConnectPhysical = 0x1,
		AutoConnectMaster = 0x2
	};

    enum TracksAutoNamingRule {
        UseDefaultNames = 0x1,
        NameAfterDriver = 0x2
    };

	enum SampleFormat {
		FormatFloat = 0,
		FormatInt24,
		FormatInt16
	};

	int format_data_width (ARDOUR::SampleFormat);

	enum CDMarkerFormat {
		CDMarkerNone,
		CDMarkerCUE,
		CDMarkerTOC,
		MP4Chaps
	};

	enum HeaderFormat {
		BWF,
		WAVE,
		WAVE64,
		CAF,
		AIFF,
		iXML,
		RF64,
		RF64_WAV,
		MBWF,
	};

	struct PeakData {
		typedef Sample PeakDatum;

		PeakDatum min;
		PeakDatum max;
	};

	enum RunContext {
		ButlerContext = 0,
		TransportContext,
		ExportContext
	};

	enum SyncSource {
		/* These are "synonyms". It is important for JACK to be first
		   both here and in enums.cc, so that the string "JACK" is
		   correctly recognized in older session and preference files.
		*/
		JACK = 0,
		Engine = 0,
		MTC,
		MIDIClock,
		LTC
	};

	enum ShuttleBehaviour {
		Sprung,
		Wheel
	};

	enum ShuttleUnits {
		Percentage,
		Semitones
	};

	typedef std::vector<boost::shared_ptr<Source> > SourceList;

	enum SrcQuality {
		SrcBest,
		SrcGood,
		SrcQuick,
		SrcFast,
		SrcFastest
	};

	typedef std::list<framepos_t> AnalysisFeatureList;

	typedef std::list<boost::shared_ptr<Route> > RouteList;
	typedef std::list<boost::shared_ptr<Stripable> > StripableList;
	typedef std::list<boost::weak_ptr  <Route> > WeakRouteList;
	typedef std::list<boost::weak_ptr  <Stripable> > WeakStripableList;
	typedef std::list<boost::shared_ptr<AutomationControl> > ControlList;

	typedef std::list<boost::shared_ptr<VCA> > VCAList;

	class Bundle;
	typedef std::vector<boost::shared_ptr<Bundle> > BundleList;

	enum WaveformScale {
		Linear,
		Logarithmic
	};

	enum WaveformShape {
		Traditional,
		Rectified
	};

	struct CleanupReport {
		std::vector<std::string> paths;
		size_t                   space;
	};

	enum PositionLockStyle {
		AudioTime,
		MusicTime
	};

	/** A struct used to describe changes to processors in a route.
	 *  This is useful because objects that respond to a change in processors
	 *  can optimise what work they do based on details of what has changed.
	*/
	struct RouteProcessorChange {
		enum Type {
			GeneralChange = 0x0,
			MeterPointChange = 0x1,
			RealTimeChange = 0x2
		};

		RouteProcessorChange () : type (GeneralChange), meter_visibly_changed (true)
		{}

		RouteProcessorChange (Type t) : type (t), meter_visibly_changed (true)
		{}

		RouteProcessorChange (Type t, bool m) : type (t), meter_visibly_changed (m)
		{}

		/** type of change; "GeneralChange" means anything could have changed */
		Type type;
		/** true if, when a MeterPointChange has occurred, the change is visible to the user */
		bool meter_visibly_changed;
	};

	struct BusProfile {
		AutoConnectOption input_ac;      /* override the RC config for input auto-connection */
		AutoConnectOption output_ac;     /* override the RC config for output auto-connection */
		uint32_t master_out_channels;    /* how many channels for the master bus */
		uint32_t requested_physical_in;  /* now many of the available physical inputs to consider usable */
		uint32_t requested_physical_out; /* now many of the available physical inputs to consider usable */
	};

	enum FadeShape {
		FadeLinear,
		FadeFast,
		FadeSlow,
		FadeConstantPower,
		FadeSymmetric,
	};

	enum TransportState {
		/* these values happen to match the constants used by JACK but
		   this equality cannot be assumed.
		*/
		TransportStopped = 0,
		TransportRolling = 1,
		TransportLooping = 2,
		TransportStarting = 3,
	};

	enum PortFlags {
		/* these values happen to match the constants used by JACK but
		   this equality cannot be assumed.
		*/
		IsInput = 0x1,
		IsOutput = 0x2,
		IsPhysical = 0x4,
		CanMonitor = 0x8,
		IsTerminal = 0x10,

		/* non-JACK related flags */
		Hidden = 0x20,
		Shadow = 0x40
	};

	enum MidiPortFlags {
		MidiPortMusic = 0x1,
		MidiPortControl = 0x2,
		MidiPortSelection = 0x4,
		MidiPortVirtual = 0x8
	};

	struct LatencyRange {
	    uint32_t min; //< samples
	    uint32_t max; //< samples
	};

	enum BufferingPreset {
		Small,
		Medium,
		Large,
		Custom,
	};

	enum AutoReturnTarget {
		LastLocate = 0x1,
		RangeSelectionStart = 0x2,
		Loop = 0x4,
		RegionSelectionStart = 0x8,
	};

	enum PlaylistDisposition {
		CopyPlaylist,
		NewPlaylist,
		SharePlaylist
	};

	enum MidiTrackNameSource {
		SMFTrackNumber,
		SMFTrackName,
		SMFInstrumentName
	};

	enum MidiTempoMapDisposition {
		SMFTempoIgnore,
		SMFTempoUse,
	};

} // namespace ARDOUR

static inline ARDOUR::framepos_t
session_frame_to_track_frame (ARDOUR::framepos_t session_frame, double speed)
{
	long double result = (long double) session_frame * (long double) speed;

	if (result >= (long double) ARDOUR::max_framepos) {
		return ARDOUR::max_framepos;
	} else if (result <= (long double) (ARDOUR::max_framepos) * (ARDOUR::framepos_t)(-1)) {
		return (ARDOUR::max_framepos * (ARDOUR::framepos_t)(-1));
	} else {
		return result;
	}
}

static inline ARDOUR::framepos_t
track_frame_to_session_frame (ARDOUR::framepos_t track_frame, double speed)
{
	/* NB - do we need a check for speed == 0 ??? */
	long double result = (long double) track_frame / (long double) speed;

	if (result >= (long double) ARDOUR::max_framepos) {
		return ARDOUR::max_framepos;
	} else if (result <= (long double) (ARDOUR::max_framepos) * (ARDOUR::framepos_t)(-1)) {
		return (ARDOUR::max_framepos * (ARDOUR::framepos_t)(-1));
	} else {
		return result;
	}
}

/* for now, break the rules and use "using" to make this "global" */

using ARDOUR::framepos_t;


#endif /* __ardour_types_h__ */
