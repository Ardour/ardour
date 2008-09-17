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

#include <inttypes.h>
#include <jack/types.h>
#include <jack/midiport.h>
#include <control_protocol/smpte.h>
#include <pbd/id.h>

#include <map>

#if __GNUC__ < 3

typedef int intptr_t;
#endif

/* eventually, we'd like everything (including JACK) to
   move to this. for now, its a dedicated type.
*/

typedef int64_t                    nframes64_t;

namespace ARDOUR {

	class Source;
	class AudioSource;

	typedef jack_default_audio_sample_t Sample;
	typedef float                       pan_t;
	typedef float                       gain_t;
	typedef uint32_t                    layer_t;
	typedef uint64_t                    microseconds_t;
	typedef uint32_t                    nframes_t;

	enum IOChange {
		NoChange = 0,
		ConfigurationChanged = 0x1,
		ConnectionsChanged = 0x2
	};

	enum OverlapType {
		OverlapNone,      // no overlap
		OverlapInternal,  // the overlap is 100% with the object
		OverlapStart,     // overlap covers start, but ends within
		OverlapEnd,       // overlap begins within and covers end
		OverlapExternal   // overlap extends to (at least) begin+end
	};

	OverlapType coverage (nframes_t start_a, nframes_t end_a,
			      nframes_t start_b, nframes_t end_b);

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
		MidiChannelAftertouchAutomation = 0x23,
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
		MeterPostFader
	};

	enum TrackMode {
		Normal,
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

	enum EventTimeUnit {
		Frames,
		Beats
	};

	struct BBT_Time {
	    uint32_t bars;
	    uint32_t beats;
	    uint32_t ticks;

	    BBT_Time() {
		    bars = 1;
		    beats = 1;
		    ticks = 0;
	    }

	    /* we can't define arithmetic operators for BBT_Time, because
	       the results depend on a TempoMap, but we can define
	       a useful check on the less-than condition.
	    */

	    bool operator< (const BBT_Time& other) const {
		    return bars < other.bars ||
			    (bars == other.bars && beats < other.beats) ||
			    (bars == other.bars && beats == other.beats && ticks < other.ticks);
	    }

	    bool operator== (const BBT_Time& other) const {
		    return bars == other.bars && beats == other.beats && ticks == other.ticks;
	    }

	};
	enum SmpteFormat {
		smpte_23976,
		smpte_24,
		smpte_24976,
		smpte_25,
		smpte_2997,
		smpte_2997drop,
		smpte_30,
		smpte_30drop,
		smpte_5994,
		smpte_60
	};

	struct AnyTime {
	    enum Type {
		    SMPTE,
		    BBT,
		    Frames,
		    Seconds
	    };

	    Type type;

	    SMPTE::Time    smpte;
	    BBT_Time       bbt;

	    union {
		nframes_t frames;
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

	enum Change {
		range_guarantee = ~0
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

	enum SoloModel {
		InverseMute,
		SoloBus
	};

	enum AutoConnectOption {
		AutoConnectPhysical = 0x1,
		AutoConnectMaster = 0x2
	};

	struct InterThreadInfo {
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

	enum SlaveSource {
		None = 0,
		MTC,
		JACK,
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
		TimeFXRequest() : time_fraction(0), pitch_fraction(0),
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

} // namespace ARDOUR

std::istream& operator>>(std::istream& o, ARDOUR::SampleFormat& sf);
std::istream& operator>>(std::istream& o, ARDOUR::HeaderFormat& sf);
std::istream& operator>>(std::istream& o, ARDOUR::AutoConnectOption& sf);
std::istream& operator>>(std::istream& o, ARDOUR::EditMode& sf);
std::istream& operator>>(std::istream& o, ARDOUR::MonitorModel& sf);
std::istream& operator>>(std::istream& o, ARDOUR::RemoteModel& sf);
std::istream& operator>>(std::istream& o, ARDOUR::SoloModel& sf);
std::istream& operator>>(std::istream& o, ARDOUR::LayerModel& sf);
std::istream& operator>>(std::istream& o, ARDOUR::CrossfadeModel& sf);
std::istream& operator>>(std::istream& o, ARDOUR::SlaveSource& sf);
std::istream& operator>>(std::istream& o, ARDOUR::ShuttleBehaviour& sf);
std::istream& operator>>(std::istream& o, ARDOUR::ShuttleUnits& sf);
std::istream& operator>>(std::istream& o, ARDOUR::SmpteFormat& sf);
std::istream& operator>>(std::istream& o, ARDOUR::DenormalModel& sf);

using ARDOUR::nframes_t;

static inline nframes_t
session_frame_to_track_frame (nframes_t session_frame, double speed)
{
	return (nframes_t)( (double)session_frame * speed );
}

static inline nframes_t
track_frame_to_session_frame (nframes_t track_frame, double speed)
{
	return (nframes_t)( (double)track_frame / speed );
}


#endif /* __ardour_types_h__ */

