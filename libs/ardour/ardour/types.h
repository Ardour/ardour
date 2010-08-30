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

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS /* PRI<foo>; C++ requires explicit requesting of these */
#endif

#include <istream>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <sys/types.h>

#include <inttypes.h>
#include <jack/types.h>
#include <jack/midiport.h>
#include "control_protocol/timecode.h"
#include "pbd/id.h"

#include "ardour/bbt_time.h"
#include "ardour/chan_count.h"

#include <map>

#if __GNUC__ < 3
typedef int intptr_t;
#endif

namespace ARDOUR {

	class Source;
	class AudioSource;
	class Route;

	typedef jack_default_audio_sample_t Sample;
	typedef float                       pan_t;
	typedef float                       gain_t;
	typedef uint32_t                    layer_t;
	typedef uint64_t                    microseconds_t;
	typedef uint32_t                    nframes_t;
	typedef int64_t                     nframes64_t;


	/** "Session frames", frames relative to the session timeline.
	 * Everything related to transport position etc. should be of this type.
	 * We might want to make this a compile time option for 32-bitters who
	 * don't want to pay for extremely long session times they don't need...
	 */
	typedef int64_t sframes_t;
	typedef int64_t framepos_t;
	/* any offset from a framepos_t, measured in audio frames */
	typedef int64_t frameoffset_t;
	/* any count of audio frames */
	typedef int64_t framecnt_t;

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

	/** See parameter.h
	 * XXX: I don't think/hope these hex values matter anymore.
	 */
	enum AutomationType {
		NullAutomation = 0x0,
		GainAutomation = 0x1,
		PanAutomation = 0x2,
		PluginAutomation = 0x4,
		SoloAutomation = 0x8,
		MuteAutomation = 0x10,
		MidiCCAutomation = 0x20,
		MidiPgmChangeAutomation = 0x21,
		MidiPitchBenderAutomation = 0x22,
		MidiChannelPressureAutomation = 0x23,
		MidiSystemExclusiveAutomation = 0x24,
		FadeInAutomation = 0x40,
		FadeOutAutomation = 0x80,
		EnvelopeAutomation = 0x100
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

	enum MeterPoint {
		MeterInput,
		MeterPreFader,
		MeterPostFader,
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

		Timecode::Time    timecode;
		BBT_Time       bbt;

		union {
			nframes_t      frames;
			double         seconds;
		};

		AnyTime() { type = Frames; frames = 0; }
	};

	struct AudioRange {
		nframes_t start;
		nframes_t end;
		uint32_t id;

		AudioRange (nframes_t s, nframes_t e, uint32_t i) : start (s), end (e) , id (i) {}

		nframes_t length() { return end - start + 1; }

		bool operator== (const AudioRange& other) const {
			return start == other.start && end == other.end && id == other.id;
		}

		bool equal (const AudioRange& other) const {
			return start == other.start && end == other.end;
		}

		OverlapType coverage (nframes_t s, nframes_t e) const {
			return ARDOUR::coverage (start, end, s, e);
		}
	};

	struct MusicRange {
		BBT_Time start;
		BBT_Time end;
		uint32_t id;

		MusicRange (BBT_Time& s, BBT_Time& e, uint32_t i)
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
		HardwareMonitoring,
		SoftwareMonitoring,
		ExternalMonitoring
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

	enum LayerModel {
		LaterHigher,
		MoveAddHigher,
		AddHigher
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

	struct InterThreadInfo {
		InterThreadInfo () : done (false), cancel (false), progress (0), thread (0) {}
			
		volatile bool  done;
		volatile bool  cancel;
		volatile float progress;
		pthread_t      thread;
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
		VST
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

	struct TimeFXRequest : public InterThreadInfo {
		TimeFXRequest()
			: time_fraction(0), pitch_fraction(0),
			quick_seek(false), antialias(false),  opts(0) {}
		float time_fraction;
		float pitch_fraction;
		/* SoundTouch */
		bool  quick_seek;
		bool  antialias;
		/* RubberBand */
		int   opts; // really RubberBandStretcher::Options
	};

	typedef std::list<nframes64_t> AnalysisFeatureList;

	typedef std::list<boost::shared_ptr<Route> >      RouteList;

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

} // namespace ARDOUR


/* these cover types declared above in this header. See enums.cc
   for the definitions.
*/

std::istream& operator>>(std::istream& o, ARDOUR::SampleFormat& sf);
std::istream& operator>>(std::istream& o, ARDOUR::HeaderFormat& sf);
std::istream& operator>>(std::istream& o, ARDOUR::AutoConnectOption& sf);
std::istream& operator>>(std::istream& o, ARDOUR::EditMode& sf);
std::istream& operator>>(std::istream& o, ARDOUR::MonitorModel& sf);
std::istream& operator>>(std::istream& o, ARDOUR::RemoteModel& sf);
std::istream& operator>>(std::istream& o, ARDOUR::ListenPosition& sf);
std::istream& operator>>(std::istream& o, ARDOUR::LayerModel& sf);
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
std::ostream& operator<<(std::ostream& o, const ARDOUR::RemoteModel& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::ListenPosition& sf);
std::ostream& operator<<(std::ostream& o, const ARDOUR::LayerModel& sf);
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

static inline ARDOUR::nframes64_t
session_frame_to_track_frame (ARDOUR::nframes64_t session_frame, double speed)
{
	return (ARDOUR::nframes64_t)( (double)session_frame * speed );
}

static inline ARDOUR::nframes64_t
track_frame_to_session_frame (ARDOUR::nframes64_t track_frame, double speed)
{
	return (ARDOUR::nframes64_t)( (double)track_frame / speed );
}

/* for now, break the rules and use "using" to make these "global" */

using ARDOUR::nframes_t;
using ARDOUR::nframes64_t;


#endif /* __ardour_types_h__ */

