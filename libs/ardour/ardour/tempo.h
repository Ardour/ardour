/*
    Copyright (C) 2000 Paul Davis

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

#ifndef __ardour_tempo_h__
#define __ardour_tempo_h__

#include <list>
#include <string>
#include <vector>
#include <cmath>
#include <glibmm/threads.h>

#include "pbd/undo.h"
#include "pbd/stateful.h"
#include "pbd/statefuldestructible.h"

#include "evoral/types.hpp"

#include "ardour/ardour.h"

class BBTTest;
class FrameposPlusBeatsTest;
class TempoTest;
class XMLNode;

namespace ARDOUR {

class Meter;
class TempoMap;

/** Tempo, the speed at which musical time progresses (BPM). */
class Tempo {
  public:
	Tempo (double bpm, double type=4.0) // defaulting to quarter note
		: _beats_per_minute (bpm), _note_type(type) {}

	double beats_per_minute () const { return _beats_per_minute;}
	double note_type () const { return _note_type;}
	double frames_per_beat (framecnt_t sr) const {
		return (60.0 * sr) / _beats_per_minute;
	}

  protected:
	double _beats_per_minute;
	double _note_type;
};

/** Meter, or time signature (beats per bar, and which note type is a beat). */
class Meter {
  public:
	Meter (double dpb, double bt)
		: _divisions_per_bar (dpb), _note_type (bt) {}

	double divisions_per_bar () const { return _divisions_per_bar; }
	double note_divisor() const { return _note_type; }

	double frames_per_bar (const Tempo&, framecnt_t sr) const;
	double frames_per_grid (const Tempo&, framecnt_t sr) const;

  protected:
	/** The number of divisions in a bar.  This is a floating point value because
	    there are musical traditions on our planet that do not limit
	    themselves to integral numbers of beats per bar.
	*/
	double _divisions_per_bar;

	/** The type of "note" that a division represents.  For example, 4.0 is
	    a quarter (crotchet) note, 8.0 is an eighth (quaver) note, etc.
	*/
	double _note_type;
};

/** A section of timeline with a certain Tempo or Meter. */
class MetricSection {
  public:
	MetricSection (const Timecode::BBT_Time& start)
		: _start (start), _frame (0), _movable (true) {}
	MetricSection (framepos_t start)
		: _frame (start), _movable (true) {}

	virtual ~MetricSection() {}

	const Timecode::BBT_Time& start() const { return _start; }
	framepos_t                frame() const { return _frame; }

	void set_movable (bool yn) { _movable = yn; }
	bool movable() const { return _movable; }

	virtual void set_frame (framepos_t f) {
		_frame = f;
	}

	virtual void set_start (const Timecode::BBT_Time& w) {
		_start = w;
	}

	/* MeterSections are not stateful in the full sense,
	   but we do want them to control their own
	   XML state information.
	*/
	virtual XMLNode& get_state() const = 0;

  private:
	Timecode::BBT_Time _start;
	framepos_t         _frame;
	bool               _movable;
};

/** A section of timeline with a certain Meter. */
class MeterSection : public MetricSection, public Meter {
  public:
	MeterSection (const Timecode::BBT_Time& start, double bpb, double note_type)
		: MetricSection (start), Meter (bpb, note_type) {}
	MeterSection (framepos_t start, double bpb, double note_type)
		: MetricSection (start), Meter (bpb, note_type) {}
	MeterSection (const XMLNode&);

	static const std::string xml_state_node_name;

	XMLNode& get_state() const;
};

/** A section of timeline with a certain Tempo. */
class TempoSection : public MetricSection, public Tempo {
  public:
	TempoSection (const Timecode::BBT_Time& start, double qpm, double note_type)
		: MetricSection (start), Tempo (qpm, note_type), _bar_offset (-1.0)  {}
	TempoSection (framepos_t start, double qpm, double note_type)
		: MetricSection (start), Tempo (qpm, note_type), _bar_offset (-1.0) {}
	TempoSection (const XMLNode&);

	static const std::string xml_state_node_name;

	XMLNode& get_state() const;

	void update_bar_offset_from_bbt (const Meter&);
	void update_bbt_time_from_bar_offset (const Meter&);
	double bar_offset() const { return _bar_offset; }

  private:
	/* this value provides a fractional offset into the bar in which
	   the tempo section is located in. A value of 0.0 indicates that
	   it occurs on the first beat of the bar, a value of 0.5 indicates
	   that it occurs halfway through the bar and so on.
	   
	   this enables us to keep the tempo change at the same relative
	   position within the bar if/when the meter changes.
	*/
	double _bar_offset;
};

typedef std::list<MetricSection*> Metrics;

/** Helper class to keep track of the Meter *AND* Tempo in effect
    at a given point in time.
*/
class TempoMetric {
  public:
	TempoMetric (const Meter& m, const Tempo& t)
		: _meter (&m), _tempo (&t), _frame (0) {}

	void set_tempo (const Tempo& t)              { _tempo = &t; }
	void set_meter (const Meter& m)              { _meter = &m; }
	void set_frame (framepos_t f)                { _frame = f; }
	void set_start (const Timecode::BBT_Time& t) { _start = t; }

	const Meter&              meter() const { return *_meter; }
	const Tempo&              tempo() const { return *_tempo; }
	framepos_t                frame() const { return _frame; }
	const Timecode::BBT_Time& start() const { return _start; }

  private:
	const Meter*       _meter;
	const Tempo*       _tempo;
	framepos_t         _frame;
	Timecode::BBT_Time _start;
};

class TempoMap : public PBD::StatefulDestructible
{
  public:
	TempoMap (framecnt_t frame_rate);
	~TempoMap();

	/* measure-based stuff */

	enum BBTPointType {
		Bar,
		Beat,
	};

	struct BBTPoint {
		framepos_t          frame;
		const MeterSection* meter;
		const TempoSection* tempo;
		uint32_t            bar;
		uint32_t            beat;
            
		BBTPoint (const MeterSection& m, const TempoSection& t, framepos_t f,
		          uint32_t b, uint32_t e)
			: frame (f), meter (&m), tempo (&t), bar (b), beat (e) {}
		
		Timecode::BBT_Time bbt() const { return Timecode::BBT_Time (bar, beat, 0); }
		operator Timecode::BBT_Time() const { return bbt(); }
		operator framepos_t() const { return frame; }
		bool is_bar() const { return beat == 1; }
	};

	typedef std::vector<BBTPoint> BBTPointList;

	template<class T> void apply_with_metrics (T& obj, void (T::*method)(const Metrics&)) {
		Glib::Threads::RWLock::ReaderLock lm (lock);
		(obj.*method)(metrics);
	}

	void get_grid (BBTPointList::const_iterator&, BBTPointList::const_iterator&, 
	               framepos_t start, framepos_t end);
	
	/* TEMPO- AND METER-SENSITIVE FUNCTIONS 

	   bbt_time(), bbt_time_rt(), frame_time() and bbt_duration_at()
	   are all sensitive to tempo and meter, and will give answers
	   that align with the grid formed by tempo and meter sections.
	   
	   They SHOULD NOT be used to determine the position of events 
	   whose location is canonically defined in beats.
	*/

	void bbt_time (framepos_t when, Timecode::BBT_Time&);

	/* realtime safe variant of ::bbt_time(), will throw 
	   std::logic_error if the map is not large enough
	   to provide an answer.
	*/
	void       bbt_time_rt (framepos_t when, Timecode::BBT_Time&);
	framepos_t frame_time (const Timecode::BBT_Time&);
	framecnt_t bbt_duration_at (framepos_t, const Timecode::BBT_Time&, int dir);

	/* TEMPO-SENSITIVE FUNCTIONS
	   
	   These next 4 functions will all take tempo in account and should be
	   used to determine position (and in the last case, distance in beats)
	   when tempo matters but meter does not.

	   They SHOULD be used to determine the position of events 
	   whose location is canonically defined in beats.
	*/

	framepos_t framepos_plus_bbt (framepos_t pos, Timecode::BBT_Time b) const;
	framepos_t framepos_plus_beats (framepos_t, Evoral::MusicalTime) const;
	framepos_t framepos_minus_beats (framepos_t, Evoral::MusicalTime) const;
	Evoral::MusicalTime framewalk_to_beats (framepos_t pos, framecnt_t distance) const;

	static const Tempo& default_tempo() { return _default_tempo; }
	static const Meter& default_meter() { return _default_meter; }

	const Tempo& tempo_at (framepos_t) const;
	const Meter& meter_at (framepos_t) const;

	const TempoSection& tempo_section_at (framepos_t) const;

	void add_tempo (const Tempo&, Timecode::BBT_Time where);
	void add_meter (const Meter&, Timecode::BBT_Time where);

	void remove_tempo (const TempoSection&, bool send_signal);
	void remove_meter (const MeterSection&, bool send_signal);

	void replace_tempo (const TempoSection&, const Tempo&, const Timecode::BBT_Time& where);
	void replace_meter (const MeterSection&, const Meter&, const Timecode::BBT_Time& where);

	framepos_t round_to_bar  (framepos_t frame, int dir);
	framepos_t round_to_beat (framepos_t frame, int dir);
	framepos_t round_to_beat_subdivision (framepos_t fr, int sub_num, int dir);
	framepos_t round_to_tick (framepos_t frame, int dir);

	void set_length (framepos_t frames);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	void dump (std::ostream&) const;
	void clear ();

	TempoMetric metric_at (Timecode::BBT_Time bbt) const;
	TempoMetric metric_at (framepos_t) const;

	void change_existing_tempo_at (framepos_t, double bpm, double note_type);
	void change_initial_tempo (double bpm, double note_type);

	void insert_time (framepos_t, framecnt_t);

	int n_tempos () const;
	int n_meters () const;

	framecnt_t frame_rate () const { return _frame_rate; }

  private:

	friend class ::BBTTest;
	friend class ::FrameposPlusBeatsTest;
	friend class ::TempoTest;
	
	static Tempo    _default_tempo;
	static Meter    _default_meter;

	Metrics                       metrics;
	framecnt_t                    _frame_rate;
	mutable Glib::Threads::RWLock lock;
	BBTPointList                  _map;

	void recompute_map (bool reassign_tempo_bbt, framepos_t end = -1);
	void extend_map (framepos_t end);
	void require_map_to (framepos_t pos);
	void require_map_to (const Timecode::BBT_Time&);
	void _extend_map (TempoSection* tempo, MeterSection* meter, 
	                  Metrics::iterator next_metric,
	                  Timecode::BBT_Time current, framepos_t current_frame, framepos_t end);

	BBTPointList::const_iterator bbt_before_or_at (framepos_t);
	BBTPointList::const_iterator bbt_before_or_at (const Timecode::BBT_Time&);
	BBTPointList::const_iterator bbt_after_or_at (framepos_t);
	
	framepos_t round_to_type (framepos_t fr, int dir, BBTPointType);
	void bbt_time (framepos_t, Timecode::BBT_Time&, const BBTPointList::const_iterator&);
	framecnt_t bbt_duration_at_unlocked (const Timecode::BBT_Time& when, const Timecode::BBT_Time& bbt, int dir);
	
	const MeterSection& first_meter() const;
	const TempoSection& first_tempo() const;
	
	void do_insert (MetricSection* section);
};

}; /* namespace ARDOUR */

std::ostream& operator<< (std::ostream&, const ARDOUR::Meter&);
std::ostream& operator<< (std::ostream&, const ARDOUR::Tempo&);
std::ostream& operator<< (std::ostream&, const ARDOUR::MetricSection&);

#endif /* __ardour_tempo_h__ */
