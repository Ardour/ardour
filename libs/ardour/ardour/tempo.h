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
#include <glibmm/thread.h>

#include "pbd/undo.h"
#include "pbd/stateful.h"
#include "pbd/statefuldestructible.h"

#include "evoral/types.hpp"

#include "ardour/ardour.h"

class XMLNode;
class BBTTest;

namespace ARDOUR {

class Meter;
class TempoMap;

class Tempo {
  public:
	Tempo (double bpm, double type=4.0) // defaulting to quarter note
		: _beats_per_minute (bpm), _note_type(type) {}

	double beats_per_minute () const { return _beats_per_minute;}
	double note_type () const { return _note_type;}
	double frames_per_beat (framecnt_t sr) const;

  protected:
	double _beats_per_minute;
	double _note_type;
};

class Meter {
  public:
	Meter (double dpb, double bt)
		: _divisions_per_bar (dpb), _note_type (bt) {}

	double divisions_per_bar () const { return _divisions_per_bar; }
	double note_divisor() const { return _note_type; }

	double frames_per_bar (const Tempo&, framecnt_t sr) const;
	double frames_per_division (const Tempo&, framecnt_t sr) const;

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

	int compare (const MetricSection&) const;
	bool operator== (const MetricSection& other) const;
	bool operator!= (const MetricSection& other) const;

  private:
	Timecode::BBT_Time _start;
	framepos_t         _frame;
	bool               _movable;
};

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

/** Helper class that we use to be able to keep track of which
    meter *AND* tempo are in effect at a given point in time.
*/
class TempoMetric {
  public:
	TempoMetric (const Meter& m, const Tempo& t) : _meter (&m), _tempo (&t), _frame (0) {}

	void set_tempo (const Tempo& t)    { _tempo = &t; }
	void set_meter (const Meter& m)    { _meter = &m; }
	void set_frame (framepos_t f)      { _frame = f; }
	void set_start (const Timecode::BBT_Time& t) { _start = t; }

	const Meter&    meter() const { return *_meter; }
	const Tempo&    tempo() const { return *_tempo; }
	framepos_t      frame() const { return _frame; }
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
            BBTPointType type;
            framepos_t  frame;
            const Meter* meter;
            const Tempo* tempo;
            uint32_t bar;
            uint32_t beat;
            
            Timecode::BBT_Time bbt() const { return Timecode::BBT_Time (bar, beat, 0); }
            operator Timecode::BBT_Time() const { return bbt(); }
            operator framepos_t() const { return frame; }
            
            BBTPoint (const Meter& m, const Tempo& t, framepos_t f,
                      BBTPointType ty, uint32_t b, uint32_t e)
                    : type (ty), frame (f), meter (&m), tempo (&t), bar (b), beat (e) {}
	};

	typedef std::vector<BBTPoint> BBTPointList;

	template<class T> void apply_with_metrics (T& obj, void (T::*method)(const Metrics&)) {
		Glib::RWLock::ReaderLock lm (lock);
		(obj.*method)(*metrics);
	}

	const BBTPointList& map() const { return _map ; }
	void map (BBTPointList&, framepos_t start, framepos_t end);
	
	void      bbt_time (framepos_t when, Timecode::BBT_Time&);
        framecnt_t frame_time (const Timecode::BBT_Time&);
	framecnt_t bbt_duration_at (framepos_t, const Timecode::BBT_Time&, int dir);

	static const Tempo& default_tempo() { return _default_tempo; }
	static const Meter& default_meter() { return _default_meter; }

	const Tempo& tempo_at (framepos_t) const;
	const Meter& meter_at (framepos_t) const;

	const TempoSection& tempo_section_at (framepos_t) const;

	void add_tempo(const Tempo&, Timecode::BBT_Time where);
	void add_meter(const Meter&, Timecode::BBT_Time where);

	void remove_tempo(const TempoSection&, bool send_signal);
	void remove_meter(const MeterSection&, bool send_signal);

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

	framepos_t framepos_plus_bbt (framepos_t pos, Timecode::BBT_Time b);
	framepos_t framepos_plus_beats (framepos_t, Evoral::MusicalTime);
	framepos_t framepos_minus_beats (framepos_t, Evoral::MusicalTime);
	Evoral::MusicalTime framewalk_to_beats (framepos_t pos, framecnt_t distance);

	void change_existing_tempo_at (framepos_t, double bpm, double note_type);
	void change_initial_tempo (double bpm, double note_type);

	void insert_time (framepos_t, framecnt_t);

	int n_tempos () const;
	int n_meters () const;

	framecnt_t frame_rate () const { return _frame_rate; }

  private:

	friend class ::BBTTest;
	
	static Tempo    _default_tempo;
	static Meter    _default_meter;

	Metrics*             metrics;
	framecnt_t          _frame_rate;
	framepos_t           last_bbt_when;
	bool                 last_bbt_valid;
	Timecode::BBT_Time   last_bbt;
	mutable Glib::RWLock lock;
	BBTPointList          _map;

	void recompute_map (bool reassign_tempo_bbt, framepos_t end = -1);
        void require_map_to (framepos_t pos);
        void require_map_to (const Timecode::BBT_Time&);

    BBTPointList::const_iterator bbt_before_or_at (framepos_t);
    BBTPointList::const_iterator bbt_after_or_at (framepos_t);
    BBTPointList::const_iterator bbt_point_for (const Timecode::BBT_Time&);

	void timestamp_metrics_from_audio_time ();

	framepos_t round_to_type (framepos_t fr, int dir, BBTPointType);

	void bbt_time_unlocked (framepos_t, Timecode::BBT_Time&);

    framecnt_t bbt_duration_at_unlocked (const Timecode::BBT_Time& when, const Timecode::BBT_Time& bbt, int dir);

	const MeterSection& first_meter() const;
	const TempoSection& first_tempo() const;

	int move_metric_section (MetricSection&, const Timecode::BBT_Time& to);
	void do_insert (MetricSection* section);

	Timecode::BBT_Time bbt_add (const Timecode::BBT_Time&, const Timecode::BBT_Time&, const TempoMetric&) const;
	Timecode::BBT_Time bbt_add (const Timecode::BBT_Time& a, const Timecode::BBT_Time& b) const;
	Timecode::BBT_Time bbt_subtract (const Timecode::BBT_Time&, const Timecode::BBT_Time&) const;
};

}; /* namespace ARDOUR */

std::ostream& operator<< (std::ostream&, const ARDOUR::Meter&);
std::ostream& operator<< (std::ostream&, const ARDOUR::Tempo&);
std::ostream& operator<< (std::ostream&, const ARDOUR::MetricSection&);

#endif /* __ardour_tempo_h__ */
