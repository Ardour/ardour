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

    $Id$
*/

#ifndef __ardour_types_h__
#define __ardour_types_h__

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS /* PRI<foo>; C++ requires explicit requesting of these */
#endif

#include <istream>

#include <inttypes.h>
#include <jack/types.h>
#include <control_protocol/smpte.h>
#include <pbd/id.h>

#include <map>

#if __GNUC__ < 3

typedef int intptr_t;
#endif

namespace ARDOUR {

	class Source;

	typedef jack_default_audio_sample_t Sample;
	typedef float                       pan_t;
	typedef float                       gain_t;
	typedef uint32_t                    layer_t;
	typedef uint64_t                    microseconds_t;

	typedef unsigned char RawMidi;

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

	OverlapType coverage (jack_nframes_t start_a, jack_nframes_t end_a,
			      jack_nframes_t start_b, jack_nframes_t end_b);

	enum AutomationType {
		GainAutomation = 0x1,
		PanAutomation = 0x2,
		PluginAutomation = 0x4,
		SoloAutomation = 0x8,
		MuteAutomation = 0x10
	};

	enum AutoState {
		Off = 0x0,
		Write = 0x1,
		Touch = 0x2,
		Play = 0x4
	};

	enum AutoStyle {
		Absolute = 0x1,
		Trim = 0x2
	};

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
		jack_nframes_t frames;
		double         seconds;
	    };
	};

	struct AudioRange {
	    jack_nframes_t start;
	    jack_nframes_t end;
	    uint32_t id;
	    
	    AudioRange (jack_nframes_t s, jack_nframes_t e, uint32_t i) : start (s), end (e) , id (i) {}
	    
	    jack_nframes_t length() { return end - start + 1; } 

	    bool operator== (const AudioRange& other) const {
		    return start == other.start && end == other.end && id == other.id;
	    }

	    bool equal (const AudioRange& other) const {
		    return start == other.start && end == other.end;
	    }

	    OverlapType coverage (jack_nframes_t s, jack_nframes_t e) const {
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

	enum EditMode {
		Slide,
		Splice
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

	enum CrossfadeModel {
		FullCrossfade,
		ShortCrossfade
	};

	struct InterThreadInfo {
	    volatile bool  done;
	    volatile bool  cancel;
	    volatile float progress;
	    pthread_t      thread;
	};

	enum SampleFormat {
		FormatFloat = 0,
		FormatInt24
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
}

std::istream& operator>>(std::istream& o, ARDOUR::SampleFormat& sf);
std::istream& operator>>(std::istream& o, ARDOUR::HeaderFormat& sf);

static inline jack_nframes_t
session_frame_to_track_frame (jack_nframes_t session_frame, double speed)
{
	return (jack_nframes_t)( (double)session_frame * speed );
}


static inline jack_nframes_t
track_frame_to_session_frame (jack_nframes_t track_frame, double speed)
{
	return (jack_nframes_t)( (double)track_frame / speed );
}


#endif /* __ardour_types_h__ */

