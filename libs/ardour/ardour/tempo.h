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

#include <pbd/undo.h>
#include <pbd/stateful.h> 
#include <pbd/statefuldestructible.h> 

#include <sigc++/signal.h>

#include <ardour/ardour.h>

class XMLNode;

using std::list;
using std::vector;

namespace ARDOUR {
class Meter;
class Tempo {
  public:
	Tempo (double bpm, double type=4.0) // defaulting to quarter note
		: _beats_per_minute (bpm), _note_type(type) {} 
	Tempo (const Tempo& other) {
		_beats_per_minute = other._beats_per_minute;
		_note_type = other._note_type;
	}
	void operator= (const Tempo& other) {
		if (&other != this) {
			_beats_per_minute = other._beats_per_minute;
			_note_type = other._note_type;
		}
	}

	double beats_per_minute () const { return _beats_per_minute;}
	double note_type () const { return _note_type;}
	double frames_per_beat (nframes_t sr, const Meter& meter) const;

  protected:
	double _beats_per_minute;
	double _note_type;
};

class Meter {
  public:
	static const double ticks_per_beat;

	Meter (double bpb, double bt) 
		: _beats_per_bar (bpb), _note_type (bt) {}
	Meter (const Meter& other) {
		_beats_per_bar = other._beats_per_bar;
		_note_type = other._note_type;
	}
	void operator= (const Meter& other) {
		if (&other != this) {
			_beats_per_bar = other._beats_per_bar;
			_note_type = other._note_type;
		}
	}

	double beats_per_bar () const { return _beats_per_bar; }
	double note_divisor() const { return _note_type; }
	
	double frames_per_bar (const Tempo&, nframes_t sr) const;

  protected:

	/* this is the number of beats in a bar. it is a real value
	   because there are musical traditions on our planet
	   that do not limit themselves to integral numbers of beats
	   per bar.
	*/

	double _beats_per_bar;

	/* this is the type of "note" that a beat represents. for example,
	   4.0 would be a quarter (crotchet) note, 8.0 would be an eighth 
	   (quaver) note, etc.
	*/

	double _note_type;
};

class MetricSection {
  public:
	MetricSection (const BBT_Time& start)
		: _start (start), _frame (0), _movable (true) {}
	MetricSection (nframes_t start)
		: _frame (start), _movable (true) {}

	virtual ~MetricSection() {}

	const BBT_Time& start() const { return _start; }
	const nframes_t frame() const { return _frame; }

	void set_movable (bool yn) { _movable = yn; }
	bool movable() const { return _movable; }

	virtual void set_frame (nframes_t f) {
		_frame = f;
	};

	virtual void set_start (const BBT_Time& w) {
		_start = w;
	}

	/* MeterSections are not stateful in the full sense,
	   but we do want them to control their own
	   XML state information.
	*/

	virtual XMLNode& get_state() const = 0;

  private:
	BBT_Time       _start;
	nframes_t      _frame;
	bool           _movable;
};

class MeterSection : public MetricSection, public Meter {
  public:
	MeterSection (const BBT_Time& start, double bpb, double note_type)
		: MetricSection (start), Meter (bpb, note_type) {}
	MeterSection (nframes_t start, double bpb, double note_type)
		: MetricSection (start), Meter (bpb, note_type) {}
	MeterSection (const XMLNode&);

	static const string xml_state_node_name;

	XMLNode& get_state() const;
};

class TempoSection : public MetricSection, public Tempo {
  public:
	TempoSection (const BBT_Time& start, double qpm, double note_type)
		: MetricSection (start), Tempo (qpm, note_type) {}
	TempoSection (nframes_t start, double qpm, double note_type)
		: MetricSection (start), Tempo (qpm, note_type) {}
	TempoSection (const XMLNode&);

	static const string xml_state_node_name;

	XMLNode& get_state() const;
};

typedef list<MetricSection*> Metrics;

class TempoMap : public PBD::StatefulDestructible
{
  public:
	TempoMap (nframes_t frame_rate);
	~TempoMap();

	/* measure-based stuff */

	enum BBTPointType {
		Bar,
		Beat,
	};

	struct BBTPoint {
	    BBTPointType type;
	    nframes_t frame;
	    const Meter* meter;
	    const Tempo* tempo;
	    uint32_t bar;
	    uint32_t beat;
	    
	    BBTPoint (const Meter& m, const Tempo& t, nframes_t f, BBTPointType ty, uint32_t b, uint32_t e) 
		    : type (ty), frame (f), meter (&m), tempo (&t), bar (b), beat (e) {}
	};

	typedef vector<BBTPoint> BBTPointList;
	
	template<class T> void apply_with_metrics (T& obj, void (T::*method)(const Metrics&)) {
	        Glib::RWLock::ReaderLock lm (lock);
		(obj.*method)(*metrics);
	}

	BBTPointList *get_points (nframes_t start, nframes_t end) const;

	void      bbt_time (nframes_t when, BBT_Time&) const;
	nframes_t frame_time (const BBT_Time&) const;
	nframes_t bbt_duration_at (nframes_t, const BBT_Time&, int dir) const;

	static const Tempo& default_tempo() { return _default_tempo; }
	static const Meter& default_meter() { return _default_meter; }

	const Tempo& tempo_at (nframes_t);
	const Meter& meter_at (nframes_t);

	const TempoSection& tempo_section_at (nframes_t);

	void add_tempo(const Tempo&, BBT_Time where);
	void add_meter(const Meter&, BBT_Time where);

	void add_tempo(const Tempo&, nframes_t where);
	void add_meter(const Meter&, nframes_t where);

	void move_tempo (TempoSection&, const BBT_Time& to);
	void move_meter (MeterSection&, const BBT_Time& to);
	
	void remove_tempo(const TempoSection&);
	void remove_meter(const MeterSection&);

	void replace_tempo (TempoSection& existing, const Tempo& replacement);
	void replace_meter (MeterSection& existing, const Meter& replacement);


	nframes_t round_to_bar  (nframes_t frame, int dir);

	nframes_t round_to_beat (nframes_t frame, int dir);

	nframes_t round_to_beat_subdivision (nframes_t fr, int sub_num);

	nframes_t round_to_tick (nframes_t frame, int dir);

	void set_length (nframes_t frames);

	XMLNode& get_state (void);
	int set_state (const XMLNode&);

	void dump (std::ostream&) const;
	void clear ();

	/* this is a helper class that we use to be able to keep
	   track of which meter *AND* tempo are in effect at
	   a given point in time.
	*/

	class Metric {
	  public:
		Metric (const Meter& m, const Tempo& t) : _meter (&m), _tempo (&t), _frame (0) {}
		
		void set_tempo (const Tempo& t)    { _tempo = &t; }
		void set_meter (const Meter& m)    { _meter = &m; }
		void set_frame (nframes_t f)  { _frame = f; }
		void set_start (const BBT_Time& t) { _start = t; }
		
		const Meter&    meter() const { return *_meter; }
		const Tempo&    tempo() const { return *_tempo; }
		nframes_t  frame() const { return _frame; }
		const BBT_Time& start() const { return _start; }
		
	  private:
		const Meter*   _meter;
		const Tempo*   _tempo;
		nframes_t _frame;
		BBT_Time       _start;
		
	};

	Metric metric_at (BBT_Time bbt) const;
	Metric metric_at (nframes_t) const;
        void bbt_time_with_metric (nframes_t, BBT_Time&, const Metric&) const;

	void change_existing_tempo_at (nframes_t, double bpm, double note_type);
	void change_initial_tempo (double bpm, double note_type);

	int n_tempos () const;
	int n_meters () const;

	sigc::signal<void,ARDOUR::Change> StateChanged;

  private:
	static Tempo    _default_tempo;
	static Meter    _default_meter;

	Metrics*             metrics;
	nframes_t           _frame_rate;
	nframes_t            last_bbt_when;
	bool                 last_bbt_valid;
	BBT_Time             last_bbt;
	mutable Glib::RWLock lock;
	
	void timestamp_metrics (bool use_bbt);

	nframes_t round_to_type (nframes_t fr, int dir, BBTPointType);

	nframes_t frame_time_unlocked (const BBT_Time&) const;

	void bbt_time_unlocked (nframes_t, BBT_Time&) const;

	nframes_t bbt_duration_at_unlocked (const BBT_Time& when, const BBT_Time& bbt, int dir) const;

	const MeterSection& first_meter() const;
	const TempoSection& first_tempo() const;

	nframes_t count_frames_between (const BBT_Time&, const BBT_Time&) const;
	nframes_t count_frames_between_metrics (const Meter&, const Tempo&, const BBT_Time&, const BBT_Time&) const;

	int move_metric_section (MetricSection&, const BBT_Time& to);
	void do_insert (MetricSection* section, bool with_bbt);
};

}; /* namespace ARDOUR */

#endif /* __ardour_tempo_h__ */
