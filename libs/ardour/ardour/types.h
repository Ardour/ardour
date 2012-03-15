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

#include <inttypes.h>
#include <jack/types.h>
#include <jack/midiport.h>

#include "timecode/bbt_time.h"
#include "timecode/time.h"

#include "pbd/id.h"

#include "ardour/chan_count.h"

#include <map>

#if __GNUC__ < 3
typedef int intptr_t;
#endif

namespace ARDOUR {

	class Source;
	class AudioSource;
	class Route;
	class Region;

	typedef jack_default_audio_sample_t Sample;
	typedef float                       pan_t;
	typedef float                       gain_t;
	typedef uint32_t                    layer_t;
	typedef uint64_t                    microseconds_t;
	typedef jack_nframes_t              pframes_t;

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

	enum OverlapType {
		OverlapNone,      // no overlap
		OverlapInternal,  // the overlap is 100% with the object
		OverlapStart,     // overlap covers start, but ends within
		OverlapEnd,       // overlap begins within and covers end
		OverlapExternal   // overlap extends to (at least) begin+end
	};

	ARDOUR::OverlapType coverage (framepos_t sa, framepos_t ea,
	                              framepos_t sb, framepos_t eb);

	/* policies for inserting/pasting material where overlaps
	   might be an issue.
	*/

	enum InsertMergePolicy {
		InsertMergeReject,  // no overlaps allowed
		InsertMergeRelax,   // we just don't care about overlaps
		InsertMergeReplace, // replace old with new
		InsertMergeTruncateExisting, // shorten existing to avoid overlap
		InsertMergeTruncateAddition, // shorten new to avoid overlap
		InsertMergeExtend   // extend new (or old) to the range of old+new
	};

	/** See evoral/Parameter.hpp
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
		SoloAutomation,
		MuteAutomation,
		MidiCCAutomation,
		MidiPgmChangeAutomation,
		MidiPitchBenderAutomation,
		MidiChannelPressureAutomation,
		MidiSystemExclusiveAutomation,
		FadeInAutomation,
		FadeOutAutomation,
		EnvelopeAutomation
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

	enum TimecodeFormat {
		timecode_23976,
		timecode_24,
		timecode_24976,
		timecode_25,
		timecode_2997,
		timecode_2997drop,
		timecode_30,
		timecode_30drop,
		timecode_5994,
		timecode_60
	};

	struct AnyTime {
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

			/* NOTREACHED */
			assert (false);
			return false;
		}
	};

	struct AudioRange {
		framepos_t start;
		framepos_t end;
		uint32_t id;

		AudioRange (framepos_t s, framepos_t e, uint32_t i) : start (s), end (e) , id (i) {}

		framecnt_t length() { return end - start + 1; }

		bool operator== (const AudioRange& other) const {
			return start == other.start && end == other.end && id == other.id;
		}

		bool equal (const AudioRange& other) const {
			return start == other.start && end == other.end;
		}

		OverlapType coverage (framepos_t s, framepos_t e) const {
			return ARDOUR::coverage (start, end, s, e);
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
		MeterFalloffMedium = 3,
		MeterFalloffFast = 4,
		MeterFalloffFaster = 5,
		MeterFalloffFastest = 6
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
		Lock
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

	enum RemoteModel {
		UserOrdered,
		MixerOrdered,
		EditorOrdered
	};

	enum CrossfadeModel {
		FullCrossfade,
		ShortCrossfade
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

	enum SampleFormat {
		FormatFloat = 0,
		FormatInt24,
		FormatInt16
	};

	enum CDMarkerFormat {
		CDMarkerNone,
		CDMarkerCUE,
		CDMarkerTOC
	};

	enum HeaderFormat {
		BWF,
		WAVE,
		WAVE64,
		CAF,
		AIFF,
		iXML,
		RF64
	};

	struct PeakData {
		typedef Sample PeakDatum;

		PeakDatum min;
		PeakDatum max;
	};

	enum PluginType {
		AudioUnit,
		LADSPA,
		LV2,
		Windows_VST,
		LXVST,
	};

	enum RunContext {
		ButlerContext = 0,
		TransportContext,
		ExportContext
	};

	enum SyncSource {
		JACK,
		MTC,
		MIDIClock
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
	typedef std::list<boost::weak_ptr  <Route> > WeakRouteList;

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

	enum QuantizeType {
		Plain,
		Legato,
		Groove
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
			MeterPointChange = 0x1
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
		FadeLogA,
		FadeLogB
	};

} // namespace ARDOUR


/* these cover types declared above in this header. See enums.cc
   for the definitions.
*/

std::istream& operator>>(std::istream& o, ARDOUR::SampleFormat& sf);
std::istream& operator>>(std::istream& o, ARDOUR::HeaderFormat& sf);
std::istream& operator>>(std::istream& o, ARDOUR::AutoConnectOption& sf);
std::istream& operator>>(std::istream& o, ARDOUR::EditMode& sf);
std::istream& operator>>(std::istream& o, ARDOUR::MonitorModel& sf);
std::istream& operator>>(std::istream& o, ARDOUR::PFLPosition& sf);
std::istream& operator>>(std::istream& o, ARDOUR::AFLPosition& sf);
std::istream& operator>>(std::istream& o, ARDOUR::RemoteModel& sf);
std::istream& operator>>(std::istream& o, ARDOUR::ListenPosition& sf);
std::istream& operator>>(std::istream& o, ARDOUR::InsertMergePolicy& sf);
std::istream& operator>>(std::istream& o, ARDOUR::CrossfadeModel& sf);
std::istream& operator>>(std::istream& o, ARDOUR::SyncSource& sf);
std::istream& operator>>(std::istream& o, ARDOUR::ShuttleBehaviour& sf);
std::istream& operator>>(std::istream& o, ARDOUR::ShuttleUnits& sf);
std::istream& operator>>(std::istream& o, ARDOUR::TimecodeFormat& sf);
std::istream& operator>>(std::istream& o, ARDOUR::DenormalModel& sf);
std::istream& operator>>(std::istream& o, ARDOUR::WaveformScale& sf);
std::istream& operator>>(std::istream& o, ARDOUR::WaveformShape& sf);
std::istream& operator>>(std::istream& o, ARDOUR::PositionLockStyle& sf);

std::ostream& operator<<(std::ostream& o, const ARDOUR::SampleFormat& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::HeaderFormat& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::AutoConnectOption& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::EditMode& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::MonitorModel& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::PFLPosition& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::AFLPosition& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::RemoteModel& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::ListenPosition& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::InsertMergePolicy& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::CrossfadeModel& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::SyncSource& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::ShuttleBehaviour& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::ShuttleUnits& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::TimecodeFormat& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::DenormalModel& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::WaveformScale& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::WaveformShape& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::PositionLockStyle& sf);

static inline ARDOUR::framepos_t
session_frame_to_track_frame (ARDOUR::framepos_t session_frame, double speed)
{
	return (ARDOUR::framepos_t) ((long double) session_frame * (long double) speed);
}

static inline ARDOUR::framepos_t
track_frame_to_session_frame (ARDOUR::framepos_t track_frame, double speed)
{
	return (ARDOUR::framepos_t) ((long double) track_frame / (long double) speed);
}

/* for now, break the rules and use "using" to make this "global" */

using ARDOUR::framepos_t;


#endif /* __ardour_types_h__ */

