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

#include "evoral/Beats.hpp"

#include "ardour/ardour.h"

class BBTTest;
class FrameposPlusBeatsTest;
class TempoTest;
class XMLNode;

namespace ARDOUR {

class Meter;
class TempoMap;

/** Tempo, the speed at which musical time progresses (BPM). */
class LIBARDOUR_API Tempo {
  public:
	/**
	 * @param bpm Beats Per Minute
	 * @param type Note Type (default `4': quarter note)
	 */
	Tempo (double bpm, double type=4.0) // defaulting to quarter note
		: _beats_per_minute (bpm), _note_type(type) {}

	/* ..or more aptly 'pulse divisions per minute'.
	   Nothing to do with actual beats, which are defined by the meter and tempo.
	*/
	double beats_per_minute () const { return _beats_per_minute; }
	void set_beats_per_minute (double bpm) { _beats_per_minute = bpm; }
	double note_type () const { return _note_type; }
	double pulses_per_minute () const { return _beats_per_minute / _note_type; }
	/** audio samples per beat
	 * @param sr samplerate
	 */
	double frames_per_beat (framecnt_t sr) const {
		return (60.0 * sr) / _beats_per_minute;
	}
	double frames_per_pulse (framecnt_t sr) const {
		return (_note_type * 60.0 * sr) / _beats_per_minute;
	}

  protected:
	double _beats_per_minute;
	double _note_type;
};

/** Meter, or time signature (beats per bar, and which note type is a beat). */
class LIBARDOUR_API Meter {
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
class LIBARDOUR_API MetricSection {
  public:
	MetricSection (double pulse, framepos_t frame, PositionLockStyle pls)
		: _pulse (pulse), _frame (frame), _movable (true), _position_lock_style (pls) {}

	virtual ~MetricSection() {}

	const double& pulse () const { return _pulse; }
	void set_pulse (double pulse) { _pulse = pulse; }

	framepos_t frame() const { return _frame; }
	virtual void set_frame (framepos_t f) {
		_frame = f;
	}

	void set_movable (bool yn) { _movable = yn; }
	bool movable() const { return _movable; }

	/* MeterSections are not stateful in the full sense,
	   but we do want them to control their own
	   XML state information.
	*/
	virtual XMLNode& get_state() const = 0;

	PositionLockStyle position_lock_style () const { return _position_lock_style; }
	void set_position_lock_style (PositionLockStyle ps) { _position_lock_style = ps; }

private:
	double             _pulse;
	framepos_t         _frame;
	bool               _movable;
	PositionLockStyle  _position_lock_style;
};

/** A section of timeline with a certain Meter. */
class LIBARDOUR_API MeterSection : public MetricSection, public Meter {
  public:
	MeterSection (double pulse, framepos_t frame, double beat, const Timecode::BBT_Time& bbt, double bpb, double note_type, PositionLockStyle pls)
		: MetricSection (pulse, frame, pls), Meter (bpb, note_type), _bbt (bbt),  _beat (beat) {}

	MeterSection (const XMLNode&);

	static const std::string xml_state_node_name;

	XMLNode& get_state() const;

	void set_pulse (double w) {
		MetricSection::set_pulse (w);
	}
	void set_beat (std::pair<double, Timecode::BBT_Time>& w) {
		_beat = w.first;
		_bbt = w.second;
	}

	const Timecode::BBT_Time& bbt() const { return _bbt; }
        const double& beat () const { return _beat; }
	void set_beat (double beat) { _beat = beat; }

private:
	Timecode::BBT_Time _bbt;
	double _beat;
};

/** A section of timeline with a certain Tempo. */
class LIBARDOUR_API TempoSection : public MetricSection, public Tempo {
  public:
	enum Type {
		Ramp,
		Constant,
	};

	TempoSection (const double& pulse, const framepos_t& frame, double qpm, double note_type, Type tempo_type, PositionLockStyle pls)
		: MetricSection (pulse, frame, pls), Tempo (qpm, note_type), _type (tempo_type), _c_func (0.0), _active (true), _locked_to_meter (false)  {}

	TempoSection (const XMLNode&);

	static const std::string xml_state_node_name;

	XMLNode& get_state() const;

	double c_func () const { return _c_func; }
	void set_c_func (double c_func) { _c_func = c_func; }

	void set_type (Type type);
	Type type () const { return _type; }

	bool active () const { return _active; }
	void set_active (bool yn) { _active = yn; }

	bool locked_to_meter ()  const { return _locked_to_meter; }
	void set_locked_to_meter (bool yn) { _locked_to_meter = yn; }

	double tempo_at_frame (const framepos_t& frame, const framecnt_t& frame_rate) const;
	framepos_t frame_at_tempo (const double& ppm, const double& beat, const framecnt_t& frame_rate) const;

	double tempo_at_pulse (const double& pulse) const;
	double pulse_at_tempo (const double& ppm, const framepos_t& frame, const framecnt_t& frame_rate) const;

	double pulse_at_frame (const framepos_t& frame, const framecnt_t& frame_rate) const;
	frameoffset_t frame_at_pulse (const double& pulse, const framecnt_t& frame_rate) const;

	double compute_c_func_pulse (const double& end_bpm, const double& end_pulse, const framecnt_t& frame_rate);
	double compute_c_func_frame (const double& end_bpm, const framepos_t& end_frame, const framecnt_t& frame_rate) const;

	Timecode::BBT_Time legacy_bbt () { return _legacy_bbt; }

  private:

	framepos_t minute_to_frame (const double& time, const framecnt_t& frame_rate) const;
	double frame_to_minute (const framepos_t& frame, const framecnt_t& frame_rate) const;

	/*  tempo ramp functions. zero-based with time in minutes,
	 * 'tick tempo' in ticks per minute and tempo in bpm.
	 *  time relative to section start.
	 */
	double a_func (double end_tpm, double c_func) const;
	double c_func (double end_tpm, double end_time) const;

	double pulse_tempo_at_time (const double& time) const;
	double time_at_pulse_tempo (const double& pulse_tempo) const;

	double pulse_tempo_at_pulse (const double& pulse) const;
	double pulse_at_pulse_tempo (const double& pulse_tempo) const;

	double pulse_at_time (const double& time) const;
	double time_at_pulse (const double& pulse) const;

	/* this value provides a fractional offset into the bar in which
	   the tempo section is located in. A value of 0.0 indicates that
	   it occurs on the first beat of the bar, a value of 0.5 indicates
	   that it occurs halfway through the bar and so on.

	   this enables us to keep the tempo change at the same relative
	   position within the bar if/when the meter changes.
	*/
	Type _type;
	double _c_func;
	bool _active;
	bool _locked_to_meter;
	Timecode::BBT_Time _legacy_bbt;
};

typedef std::list<MetricSection*> Metrics;

/** Helper class to keep track of the Meter *AND* Tempo in effect
    at a given point in time.
*/
class LIBARDOUR_API TempoMetric {
  public:
	TempoMetric (const Meter& m, const Tempo& t)
		: _meter (&m), _tempo (&t), _frame (0) {}

	void set_tempo (const Tempo& t)              { _tempo = &t; }
	void set_meter (const Meter& m)              { _meter = &m; }
	void set_frame (framepos_t f)                { _frame = f; }
	void set_pulse (const double& p)             { _pulse = p; }

	void set_metric (const MetricSection* section) {
		const MeterSection* meter;
		const TempoSection* tempo;
		if ((meter = dynamic_cast<const MeterSection*>(section))) {
			set_meter(*meter);
		} else if ((tempo = dynamic_cast<const TempoSection*>(section))) {
			set_tempo(*tempo);
		}

		set_frame (section->frame());
		set_pulse (section->pulse());
	}

	const Meter&              meter() const { return *_meter; }
	const Tempo&              tempo() const { return *_tempo; }
	framepos_t                frame() const { return _frame; }
	const double&             pulse() const { return _pulse; }

  private:
	const Meter*       _meter;
	const Tempo*       _tempo;
	framepos_t         _frame;
	double             _pulse;
};

/** Tempo Map - mapping of timecode to musical time.
 * convert audio-samples, sample-rate to Bar/Beat/Tick, Meter/Tempo
 */
class LIBARDOUR_API TempoMap : public PBD::StatefulDestructible
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
		Meter               meter;
		Tempo               tempo;
		double              c;
		uint32_t            bar;
		uint32_t            beat;

		BBTPoint (const MeterSection& m, const Tempo& t, framepos_t f,
		          uint32_t b, uint32_t e, double func_c)
		: frame (f), meter (m.divisions_per_bar(), m.note_divisor()), tempo (t.beats_per_minute(), t.note_type()), c (func_c), bar (b), beat (e) {}

		Timecode::BBT_Time bbt() const { return Timecode::BBT_Time (bar, beat, 0); }
		operator Timecode::BBT_Time() const { return bbt(); }
		operator framepos_t() const { return frame; }
		bool is_bar() const { return beat == 1; }
	};

	template<class T> void apply_with_metrics (T& obj, void (T::*method)(const Metrics&)) {
		Glib::Threads::RWLock::ReaderLock lm (lock);
		(obj.*method)(_metrics);
	}

	void get_grid (std::vector<BBTPoint>&,
	               framepos_t start, framepos_t end);

	/* TEMPO- AND METER-SENSITIVE FUNCTIONS

	   bbt_time(), beat_at_frame(), frame_at_beat(), tick_at_frame(),
	   frame_at_tick(),frame_time() and bbt_duration_at()
	   are all sensitive to tempo and meter, and will give answers
	   that align with the grid formed by tempo and meter sections.

	   They SHOULD NOT be used to determine the position of events
	   whose location is canonically defined in beats.
	*/

	void bbt_time (framepos_t when, Timecode::BBT_Time&);

	double beat_at_frame (const framecnt_t& frame) const;
	framecnt_t frame_at_beat (const double& beat) const;

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
	framepos_t framepos_plus_beats (framepos_t, Evoral::Beats) const;
	framepos_t framepos_minus_beats (framepos_t, Evoral::Beats) const;
	Evoral::Beats framewalk_to_beats (framepos_t pos, framecnt_t distance) const;

	static const Tempo& default_tempo() { return _default_tempo; }
	static const Meter& default_meter() { return _default_meter; }

	const Tempo tempo_at (const framepos_t& frame) const;
	double frames_per_beat_at (const framepos_t&, const framecnt_t& sr) const;

	const Meter& meter_at (framepos_t) const;

	const TempoSection& tempo_section_at (framepos_t frame) const;
	const MeterSection& meter_section_at (framepos_t frame) const;
	const MeterSection& meter_section_at_beat (double beat) const;


	/** add a tempo section locked to pls. ignored values will be set in recompute_tempos()
	 * @param pulse pulse position of new section. ignored if pls == AudioTime
	 * @param frame frame position of new section. ignored if pls == MusicTime
	 * @param type type of new tempo section (Ramp, Constant)
	 */
	TempoSection* add_tempo (const Tempo&, const double& pulse, const framepos_t& frame, TempoSection::Type type, PositionLockStyle pls);

	/** add an meter section locked to pls.. ignored values will be set in recompute_meters()
	 * @param beat beat position of new section
	 * @param where bbt position of new section
	 * @param frame frame position of new section. ignored if pls == MusicTime
	 */
	MeterSection* add_meter (const Meter&, const double& beat, const Timecode::BBT_Time& where, const framepos_t& frame, PositionLockStyle pls);

	void remove_tempo (const TempoSection&, bool send_signal);
	void remove_meter (const MeterSection&, bool send_signal);

	void replace_tempo (const TempoSection&, const Tempo&, const double& pulse, const framepos_t& frame
			    , TempoSection::Type type, PositionLockStyle pls);

	void replace_meter (const MeterSection&, const Meter&, const Timecode::BBT_Time& where, const framepos_t& frame
			    , PositionLockStyle pls);

	std::pair<double, framepos_t> predict_tempo_position (TempoSection* section, const Timecode::BBT_Time& bbt);

	void gui_move_tempo (TempoSection*, const framepos_t& frame);
	void gui_move_meter (MeterSection*, const framepos_t& frame);

	bool gui_change_tempo (TempoSection*, const Tempo& bpm);
	void gui_dilate_tempo (TempoSection* tempo, const framepos_t& frame, const framepos_t& end_frame, const double& pulse);

	bool can_solve_bbt (TempoSection* section, const Timecode::BBT_Time& bbt);

	framepos_t round_to_bar  (framepos_t frame, RoundMode dir);
	framepos_t round_to_beat (framepos_t frame, RoundMode dir);
	framepos_t round_to_beat_subdivision (framepos_t fr, int sub_num, RoundMode dir);
	void round_bbt (Timecode::BBT_Time& when, const int32_t& snap_divisor, RoundMode dir);

	void set_length (framepos_t frames);

	void fix_legacy_session();

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	void dump (const Metrics& metrics, std::ostream&) const;
	void clear ();

	TempoMetric metric_at (Timecode::BBT_Time bbt) const;

	/** Return the TempoMetric at frame @p t, and point @p last to the latest
	 * metric change <= t, if it is non-NULL.
	 */
	TempoMetric metric_at (framepos_t, Metrics::const_iterator* last=NULL) const;

	Metrics::const_iterator metrics_end() { return _metrics.end(); }

	void change_existing_tempo_at (framepos_t, double bpm, double note_type);
	void change_initial_tempo (double bpm, double note_type);

	void insert_time (framepos_t, framecnt_t);
	bool remove_time (framepos_t where, framecnt_t amount);  //returns true if anything was moved

	int n_tempos () const;
	int n_meters () const;

	framecnt_t frame_rate () const { return _frame_rate; }

	double beat_at_bbt (const Timecode::BBT_Time& bbt);
	Timecode::BBT_Time bbt_at_beat (const double& beats);

	double pulse_at_bbt (const Timecode::BBT_Time& bbt);
	Timecode::BBT_Time bbt_at_pulse (const double& pulse);

	double pulse_at_beat (const double& beat) const;
	double beat_at_pulse (const double& pulse) const;

	double pulse_at_frame (const framecnt_t& frame) const;
	framecnt_t frame_at_pulse (const double& pulse) const;

	PBD::Signal0<void> MetricPositionChanged;

private:
	double pulse_at_beat_locked (const Metrics& metrics, const double& beat) const;
	double beat_at_pulse_locked (const Metrics& metrics, const double& pulse) const;

	double pulse_at_frame_locked (const Metrics& metrics, const framecnt_t& frame) const;
	framecnt_t frame_at_pulse_locked (const Metrics& metrics, const double& pulse) const;

	double beat_at_frame_locked (const Metrics& metrics, const framecnt_t& frame) const;
	framecnt_t frame_at_beat_locked (const Metrics& metrics, const double& beat) const;

	double beat_at_bbt_locked (const Metrics& metrics, const Timecode::BBT_Time& bbt) const ;
	Timecode::BBT_Time bbt_at_beat_locked (const Metrics& metrics, const double& beats) const;

	double pulse_at_bbt_locked (const Metrics& metrics, const Timecode::BBT_Time& bbt) const;
	Timecode::BBT_Time bbt_at_pulse_locked (const Metrics& metrics, const double& pulse) const;

	framepos_t frame_time_locked (const Metrics& metrics, const Timecode::BBT_Time&) const;

	const TempoSection& tempo_section_at_locked (const Metrics& metrics, framepos_t frame) const;
	const TempoSection& tempo_section_at_beat_locked (const Metrics& metrics, const double& beat) const;
	const TempoSection& tempo_section_at_pulse_locked (const Metrics& metrics, const double& pulse) const;
	const Tempo tempo_at_locked (const Metrics& metrics, const framepos_t& frame) const;

	const MeterSection& meter_section_at_locked (const Metrics& metrics, framepos_t frame) const;
	const MeterSection& meter_section_at_beat_locked (const Metrics& metrics, const double& beat) const;

	bool check_solved (const Metrics& metrics) const;
	bool set_active_tempos (const Metrics& metrics, const framepos_t& frame);

	bool solve_map_frame (Metrics& metrics, TempoSection* section, const framepos_t& frame);
	bool solve_map_pulse (Metrics& metrics, TempoSection* section, const double& pulse);
	bool solve_map_frame (Metrics& metrics, MeterSection* section, const framepos_t& frame);
	bool solve_map_bbt (Metrics& metrics, MeterSection* section, const Timecode::BBT_Time& bbt);

	friend class ::BBTTest;
	friend class ::FrameposPlusBeatsTest;
	friend class ::TempoTest;

	static Tempo    _default_tempo;
	static Meter    _default_meter;

	Metrics                       _metrics;
	framecnt_t                    _frame_rate;
	mutable Glib::Threads::RWLock lock;

	void recompute_tempos (Metrics& metrics);
	void recompute_meters (Metrics& metrics);
	void recompute_map (Metrics& metrics, framepos_t end = -1);

	framepos_t round_to_type (framepos_t fr, RoundMode dir, BBTPointType);

	const MeterSection& first_meter() const;
	MeterSection&       first_meter();
	const TempoSection& first_tempo() const;
	TempoSection&       first_tempo();

	void do_insert (MetricSection* section);

	TempoSection* add_tempo_locked (const Tempo&, double pulse, framepos_t frame
			       , TempoSection::Type type, PositionLockStyle pls, bool recompute, bool locked_to_meter = false);

	MeterSection* add_meter_locked (const Meter&, double beat, const Timecode::BBT_Time& where, framepos_t frame
					, PositionLockStyle pls, bool recompute);

	bool remove_tempo_locked (const TempoSection&);
	bool remove_meter_locked (const MeterSection&);

	TempoSection* copy_metrics_and_point (const Metrics& metrics, Metrics& copy, TempoSection* section);
	MeterSection* copy_metrics_and_point (const Metrics& metrics, Metrics& copy, MeterSection* section);
};

}; /* namespace ARDOUR */

std::ostream& operator<< (std::ostream&, const ARDOUR::Meter&);
std::ostream& operator<< (std::ostream&, const ARDOUR::Tempo&);
std::ostream& operator<< (std::ostream&, const ARDOUR::MetricSection&);

#endif /* __ardour_tempo_h__ */
