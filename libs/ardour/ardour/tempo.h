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
#include "pbd/enum_convert.h"

#include "pbd/stateful.h"
#include "pbd/statefuldestructible.h"

#include "evoral/Beats.hpp"

#include "ardour/ardour.h"

class BBTTest;
class FrameposPlusBeatsTest;
class FrameposMinusBeatsTest;
class TempoTest;
class XMLNode;

namespace ARDOUR {

class Meter;
class TempoMap;

// Find a better place for these
LIBARDOUR_API bool bbt_time_to_string (const Timecode::BBT_Time& bbt, std::string& str);
LIBARDOUR_API bool string_to_bbt_time (const std::string& str, Timecode::BBT_Time& bbt);

/** Tempo, the speed at which musical time progresses (BPM). */
class LIBARDOUR_API Tempo {
  public:
	/**
	 * @param npm Note Types per minute
	 * @param type Note Type (default `4': quarter note)
	 */
	Tempo (double npm, double type=4.0) // defaulting to quarter note
		: _note_types_per_minute (npm), _note_type (type), _end_note_types_per_minute (npm) {}
	Tempo (double start_npm, double type, double end_npm)
		: _note_types_per_minute (start_npm), _note_type (type), _end_note_types_per_minute (end_npm) {}

	double note_types_per_minute () const { return _note_types_per_minute; }
	double note_types_per_minute (double note_type) const { return (_note_types_per_minute / _note_type) * note_type; }
	void set_note_types_per_minute (double npm) { _note_types_per_minute = npm; }
	double note_type () const { return _note_type; }

	double quarter_notes_per_minute () const { return note_types_per_minute (4.0); }
	double pulses_per_minute () const { return note_types_per_minute (1.0); }

	double end_note_types_per_minute () const { return _end_note_types_per_minute; }
	double end_note_types_per_minute (double note_type) const { return (_end_note_types_per_minute / _note_type) * note_type; }
	void set_end_note_types_per_minute (double npm) { _end_note_types_per_minute = npm; }

	double end_quarter_notes_per_minute () const { return end_note_types_per_minute (4.0); }
	double end_pulses_per_minute () const { return end_note_types_per_minute (1.0); }

	/** audio samples per note type.
	 * if you want an instantaneous value for this, use TempoMap::frames_per_quarter_note_at() instead.
	 * @param sr samplerate
	 */
	double frames_per_note_type (framecnt_t sr) const {
		return (60.0 * sr) / _note_types_per_minute;
	}
	/** audio samples per quarter note.
	 * if you want an instantaneous value for this, use TempoMap::frames_per_quarter_note_at() instead.
	 * @param sr samplerate
	 */
	double frames_per_quarter_note (framecnt_t sr) const {
		return (60.0 * sr) / quarter_notes_per_minute ();
	}

  protected:
	double _note_types_per_minute;
	double _note_type;
	double _end_note_types_per_minute;
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

	inline bool operator==(const Meter& other)
	{ return _divisions_per_bar == other.divisions_per_bar() && _note_type == other.note_divisor(); }

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
	MetricSection (double pulse, double minute, PositionLockStyle pls, bool is_tempo, framecnt_t sample_rate)
		: _pulse (pulse), _minute (minute), _initial (false), _position_lock_style (pls), _is_tempo (is_tempo), _sample_rate (sample_rate) {}

	virtual ~MetricSection() {}

	const double& pulse () const { return _pulse; }
	void set_pulse (double pulse) { _pulse = pulse; }

	double minute() const { return _minute; }
	virtual void set_minute (double m) {
		_minute = m;
	}

	framepos_t frame () const { return frame_at_minute (_minute); }

	void set_initial (bool yn) { _initial = yn; }
	bool initial() const { return _initial; }

	/* MeterSections are not stateful in the full sense,
	   but we do want them to control their own
	   XML state information.
	*/
	virtual XMLNode& get_state() const = 0;

	virtual int set_state (const XMLNode&, int version);

	PositionLockStyle position_lock_style () const { return _position_lock_style; }
	void set_position_lock_style (PositionLockStyle ps) { _position_lock_style = ps; }
	bool is_tempo () const { return _is_tempo; }

	framepos_t frame_at_minute (const double& time) const;
	double minute_at_frame (const framepos_t& frame) const;

protected:
	void add_state_to_node (XMLNode& node) const;

private:

	double             _pulse;
	double             _minute;
	bool               _initial;
	PositionLockStyle  _position_lock_style;
	const bool         _is_tempo;
	framecnt_t         _sample_rate;
};

/** A section of timeline with a certain Meter. */
class LIBARDOUR_API MeterSection : public MetricSection, public Meter {
  public:
	MeterSection (double pulse, double minute, double beat, const Timecode::BBT_Time& bbt, double bpb, double note_type, PositionLockStyle pls, framecnt_t sr)
		: MetricSection (pulse, minute, pls, false, sr), Meter (bpb, note_type), _bbt (bbt),  _beat (beat) {}

	MeterSection (const XMLNode&, const framecnt_t sample_rate);

	static const std::string xml_state_node_name;

	XMLNode& get_state() const;

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

	TempoSection (const double& pulse, const double& minute, Tempo tempo, PositionLockStyle pls, framecnt_t sr)
		: MetricSection (pulse, minute, pls, true, sr), Tempo (tempo), _c (0.0), _active (true), _locked_to_meter (false), _clamped (false)  {}

	TempoSection (const XMLNode&, const framecnt_t sample_rate);

	static const std::string xml_state_node_name;

	XMLNode& get_state() const;

	double c () const { return _c; }
	void set_c (double c) { _c = c; }

	Type type () const { if (note_types_per_minute() == end_note_types_per_minute()) { return Constant; } else { return Ramp; } }

	bool active () const { return _active; }
	void set_active (bool yn) { _active = yn; }

	bool locked_to_meter ()  const { return _locked_to_meter; }
	void set_locked_to_meter (bool yn) { _locked_to_meter = yn; }

	bool clamped ()  const { return _clamped; }
	void set_clamped (bool yn) { _clamped = yn; }

	Tempo tempo_at_minute (const double& minute) const;
	double minute_at_ntpm (const double& ntpm, const double& pulse) const;

	Tempo tempo_at_pulse (const double& pulse) const;
	double pulse_at_ntpm (const double& ntpm, const double& minute) const;

	double pulse_at_minute (const double& minute) const;
	double minute_at_pulse (const double& pulse) const;

	double compute_c_pulse (const double& end_ntpm, const double& end_pulse) const;
	double compute_c_minute (const double& end_ntpm, const double& end_minute) const;

	double pulse_at_frame (const framepos_t& frame) const;
	framepos_t frame_at_pulse (const double& pulse) const;

	Timecode::BBT_Time legacy_bbt () { return _legacy_bbt; }
	bool legacy_end () { return _legacy_end; }

  private:

	/*  tempo ramp functions. zero-based with time in minutes,
	 * 'tick tempo' in ticks per minute and tempo in bpm.
	 *  time relative to section start.
	 */
	double a_func (double end_tpm, double c_func) const;
	double c_func (double end_tpm, double end_time) const;

	double _tempo_at_time (const double& time) const;
	double _time_at_tempo (const double& tempo) const;

	double _tempo_at_pulse (const double& pulse) const;
	double _pulse_at_tempo (const double& tempo) const;

	double _pulse_at_time (const double& time) const;
	double _time_at_pulse (const double& pulse) const;

	/* this value provides a fractional offset into the bar in which
	   the tempo section is located in. A value of 0.0 indicates that
	   it occurs on the first beat of the bar, a value of 0.5 indicates
	   that it occurs halfway through the bar and so on.

	   this enables us to keep the tempo change at the same relative
	   position within the bar if/when the meter changes.
	*/

	double _c;
	bool _active;
	bool _locked_to_meter;
	bool _clamped;
	Timecode::BBT_Time _legacy_bbt;
	bool _legacy_end;
};

typedef std::list<MetricSection*> Metrics;

/** Helper class to keep track of the Meter *AND* Tempo in effect
    at a given point in time.
*/
class LIBARDOUR_API TempoMetric {
  public:
	TempoMetric (const Meter& m, const Tempo& t)
		: _meter (&m), _tempo (&t), _minute (0.0), _pulse (0.0) {}

	void set_tempo (const Tempo& t)              { _tempo = &t; }
	void set_meter (const Meter& m)              { _meter = &m; }
	void set_minute (double m)                   { _minute = m; }
	void set_pulse (const double& p)             { _pulse = p; }

	void set_metric (const MetricSection* section) {
		const MeterSection* meter;
		const TempoSection* tempo;
		if ((meter = dynamic_cast<const MeterSection*>(section))) {
			set_meter(*meter);
		} else if ((tempo = dynamic_cast<const TempoSection*>(section))) {
			set_tempo(*tempo);
		}

		set_minute (section->minute());
		set_pulse (section->pulse());
	}

	const Meter&              meter() const { return *_meter; }
	const Tempo&              tempo() const { return *_tempo; }
	double                    minute() const { return _minute; }
	const double&             pulse() const { return _pulse; }

  private:
	const Meter*       _meter;
	const Tempo*       _tempo;
	double             _minute;
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

	TempoMap& operator= (TempoMap const &);

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
		: frame (f), meter (m.divisions_per_bar(), m.note_divisor()), tempo (t.note_types_per_minute(), t.note_type(), t.end_note_types_per_minute()), c (func_c), bar (b), beat (e) {}

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
	               framepos_t start, framepos_t end, uint32_t bar_mod = 0);

	static const Tempo& default_tempo() { return _default_tempo; }
	static const Meter& default_meter() { return _default_meter; }

	/* because tempi may be ramped, this is only valid for the instant requested.*/
	double frames_per_quarter_note_at (const framepos_t&, const framecnt_t& sr) const;

	const TempoSection& tempo_section_at_frame (framepos_t frame) const;
	TempoSection& tempo_section_at_frame (framepos_t frame);
	const MeterSection& meter_section_at_frame (framepos_t frame) const;
	const MeterSection& meter_section_at_beat (double beat) const;

	TempoSection* previous_tempo_section (TempoSection*) const;
	TempoSection* next_tempo_section (TempoSection*) const;

	/** add a tempo section locked to pls. ignored values will be set in recompute_tempi()
	 * @param pulse pulse position of new section. ignored if pls == AudioTime
	 * @param frame frame position of new section. ignored if pls == MusicTime
	 * @param type type of new tempo section (Ramp, Constant)
	 */
	TempoSection* add_tempo (const Tempo&, const double& pulse, const framepos_t& frame, PositionLockStyle pls);

	/** add a meter section locked to pls.. ignored values will be set in recompute_meters()
	 * @param meter the Meter to be added
	 * @param beat beat position of new section
	 * @param where bbt position of new section
	 * @param frame frame position of new section. ignored if pls == MusicTime
	 * note that @frame may also be ignored if it would create an un-solvable map
	 * (previous audio-locked tempi may place the requested beat at an earlier time than frame)
	 * in which case the new meter will be placed at the specified BBT.
	 * @param  pls the position lock style
	 *
	 * adding an audio-locked meter will add a meter-locked tempo section at the meter position.
	 * the meter-locked tempo tempo will be the Tempo at @beat
	 */
	MeterSection* add_meter (const Meter& meter, const double& beat, const Timecode::BBT_Time& where, framepos_t frame, PositionLockStyle pls);

	void remove_tempo (const TempoSection&, bool send_signal);
	void remove_meter (const MeterSection&, bool send_signal);

	void replace_tempo (TempoSection&, const Tempo&, const double& pulse, const framepos_t& frame, PositionLockStyle pls);

	void replace_meter (const MeterSection&, const Meter&, const Timecode::BBT_Time& where, framepos_t frame, PositionLockStyle pls);

	MusicFrame round_to_bar  (framepos_t frame, RoundMode dir);
	MusicFrame round_to_beat (framepos_t frame, RoundMode dir);
	MusicFrame round_to_quarter_note_subdivision (framepos_t fr, int sub_num, RoundMode dir);

	void set_length (framepos_t frames);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	void dump (std::ostream&) const;
	void clear ();

	TempoMetric metric_at (Timecode::BBT_Time bbt) const;

	/** Return the TempoMetric at frame @p t, and point @p last to the latest
	 * metric change <= t, if it is non-NULL.
	 */
	TempoMetric metric_at (framepos_t, Metrics::const_iterator* last=NULL) const;

	Metrics::const_iterator metrics_end() { return _metrics.end(); }

	void change_existing_tempo_at (framepos_t, double bpm, double note_type, double end_ntpm);
	void change_initial_tempo (double ntpm, double note_type, double end_ntpm);

	void insert_time (framepos_t, framecnt_t);
	bool remove_time (framepos_t where, framecnt_t amount);  //returns true if anything was moved

	int n_tempos () const;
	int n_meters () const;

	framecnt_t frame_rate () const { return _frame_rate; }

	/* TEMPO- AND METER-SENSITIVE FUNCTIONS

	   bbt_at_frame(), frame_at_bbt(), beat_at_frame(), frame_at_beat()
	   and bbt_duration_at()
	   are all sensitive to tempo and meter, and will give answers
	   that align with the grid formed by tempo and meter sections.

	   They SHOULD NOT be used to determine the position of events
	   whose location is canonically defined in Evoral::Beats.
	*/

	double beat_at_frame (const framecnt_t& frame) const;
	framepos_t frame_at_beat (const double& beat) const;

	const Meter& meter_at_frame (framepos_t) const;

	/* bbt - it's nearly always better to use meter-based beat (above)
	   unless tick resolution is desirable.
	*/
	Timecode::BBT_Time bbt_at_frame (framepos_t when);
	Timecode::BBT_Time bbt_at_frame_rt (framepos_t when);
	framepos_t frame_at_bbt (const Timecode::BBT_Time&);

	double beat_at_bbt (const Timecode::BBT_Time& bbt);
	Timecode::BBT_Time bbt_at_beat (const double& beats);

	double quarter_note_at_bbt (const Timecode::BBT_Time& bbt);
	double quarter_note_at_bbt_rt (const Timecode::BBT_Time& bbt);
	Timecode::BBT_Time bbt_at_quarter_note (const double& quarter_note);

	framecnt_t bbt_duration_at (framepos_t, const Timecode::BBT_Time&, int dir);
	framepos_t framepos_plus_bbt (framepos_t pos, Timecode::BBT_Time b) const;

	/* TEMPO-SENSITIVE FUNCTIONS

	   These next 2 functions will all take tempo in account and should be
	   used to determine position (and in the last case, distance in beats)
	   when tempo matters but meter does not.

	   They SHOULD be used to determine the position of events
	   whose location is canonically defined in Evoral::Beats.
	*/

	framepos_t framepos_plus_qn (framepos_t, Evoral::Beats) const;
	Evoral::Beats framewalk_to_qn (framepos_t pos, framecnt_t distance) const;

	/* quarter note related functions are also tempo-sensitive and ignore meter.
	   quarter notes may be compared with and assigned to Evoral::Beats.
	*/
	double quarter_note_at_frame (const framepos_t frame) const;
	double quarter_note_at_frame_rt (const framepos_t frame) const;
	framepos_t frame_at_quarter_note (const double quarter_note) const;

	framecnt_t frames_between_quarter_notes (const double start, const double end) const;
	double     quarter_notes_between_frames (const framecnt_t start, const framecnt_t end) const;

	double quarter_note_at_beat (const double beat) const;
	double beat_at_quarter_note (const double beat) const;

	/* obtain a musical subdivision via a frame position and magic note divisor.*/
	double exact_qn_at_frame (const framepos_t& frame, const int32_t sub_num) const;
	double exact_beat_at_frame (const framepos_t& frame, const int32_t sub_num) const;

	Tempo tempo_at_frame (const framepos_t& frame) const;
	framepos_t frame_at_tempo (const Tempo& tempo) const;
	Tempo tempo_at_quarter_note (const double& beat) const;
	double quarter_note_at_tempo (const Tempo& tempo) const;

	void gui_set_tempo_position (TempoSection*, const framepos_t& frame, const int& sub_num);
	void gui_set_meter_position (MeterSection*, const framepos_t& frame);
	bool gui_change_tempo (TempoSection*, const Tempo& bpm);
	void gui_stretch_tempo (TempoSection* tempo, const framepos_t frame, const framepos_t end_frame);
	void gui_stretch_tempo_end (TempoSection* tempo, const framepos_t frame, const framepos_t end_frame);
	bool gui_twist_tempi (TempoSection* first, const Tempo& bpm, const framepos_t frame, const framepos_t end_frame);

	std::pair<double, framepos_t> predict_tempo_position (TempoSection* section, const Timecode::BBT_Time& bbt);
	bool can_solve_bbt (TempoSection* section, const Timecode::BBT_Time& bbt);

	PBD::Signal1<void,const PBD::PropertyChange&> MetricPositionChanged;
	void fix_legacy_session();
	void fix_legacy_end_session();

private:
	/* prevent copy construction */
	TempoMap (TempoMap const&);

	TempoSection* previous_tempo_section_locked (const Metrics& metrics, TempoSection*) const;
	TempoSection* next_tempo_section_locked (const Metrics& metrics, TempoSection*) const;

	double beat_at_minute_locked (const Metrics& metrics, const double& minute) const;
	double minute_at_beat_locked (const Metrics& metrics, const double& beat) const;

	double pulse_at_beat_locked (const Metrics& metrics, const double& beat) const;
	double beat_at_pulse_locked (const Metrics& metrics, const double& pulse) const;

	double pulse_at_minute_locked (const Metrics& metrics, const double& minute) const;
	double minute_at_pulse_locked (const Metrics& metrics, const double& pulse) const;

	Tempo tempo_at_minute_locked (const Metrics& metrics, const double& minute) const;
	double minute_at_tempo_locked (const Metrics& metrics, const Tempo& tempo) const;

	Tempo tempo_at_pulse_locked (const Metrics& metrics, const double& pulse) const;
	double pulse_at_tempo_locked (const Metrics& metrics, const Tempo& tempo) const;

	Timecode::BBT_Time bbt_at_minute_locked (const Metrics& metrics, const double& minute) const;
	double minute_at_bbt_locked (const Metrics& metrics, const Timecode::BBT_Time&) const;

	double beat_at_bbt_locked (const Metrics& metrics, const Timecode::BBT_Time& bbt) const ;
	Timecode::BBT_Time bbt_at_beat_locked (const Metrics& metrics, const double& beats) const;

	double pulse_at_bbt_locked (const Metrics& metrics, const Timecode::BBT_Time& bbt) const;
	Timecode::BBT_Time bbt_at_pulse_locked (const Metrics& metrics, const double& pulse) const;

	double minutes_between_quarter_notes_locked (const Metrics& metrics, const double start_qn, const double end_qn) const;
	double quarter_notes_between_frames_locked (const Metrics& metrics, const framecnt_t  start, const framecnt_t end) const;

	const TempoSection& tempo_section_at_minute_locked (const Metrics& metrics, double minute) const;
	TempoSection& tempo_section_at_minute_locked (const Metrics& metrics, double minute);
	const TempoSection& tempo_section_at_beat_locked (const Metrics& metrics, const double& beat) const;

	const MeterSection& meter_section_at_minute_locked (const Metrics& metrics, double minute) const;
	const MeterSection& meter_section_at_beat_locked (const Metrics& metrics, const double& beat) const;

	bool check_solved (const Metrics& metrics) const;
	bool set_active_tempi (const Metrics& metrics, const framepos_t& frame);

	bool solve_map_minute (Metrics& metrics, TempoSection* section, const double& minute);
	bool solve_map_pulse (Metrics& metrics, TempoSection* section, const double& pulse);
	bool solve_map_minute (Metrics& metrics, MeterSection* section, const double& minute);
	bool solve_map_bbt (Metrics& metrics, MeterSection* section, const Timecode::BBT_Time& bbt);

	double exact_beat_at_frame_locked (const Metrics& metrics, const framepos_t& frame, const int32_t sub_num) const;
	double exact_qn_at_frame_locked (const Metrics& metrics, const framepos_t& frame, const int32_t sub_num) const;

	double minute_at_frame (const framepos_t frame) const;
	framepos_t frame_at_minute (const double minute) const;

	friend class ::BBTTest;
	friend class ::FrameposPlusBeatsTest;
	friend class ::FrameposMinusBeatsTest;
	friend class ::TempoTest;

	static Tempo    _default_tempo;
	static Meter    _default_meter;

	Metrics                       _metrics;
	framecnt_t                    _frame_rate;
	mutable Glib::Threads::RWLock lock;

	void recompute_tempi (Metrics& metrics);
	void recompute_meters (Metrics& metrics);
	void recompute_map (Metrics& metrics, framepos_t end = -1);

	MusicFrame round_to_type (framepos_t fr, RoundMode dir, BBTPointType);

	const MeterSection& first_meter() const;
	MeterSection&       first_meter();
	const TempoSection& first_tempo() const;
	TempoSection&       first_tempo();

	void do_insert (MetricSection* section);

	TempoSection* add_tempo_locked (const Tempo&, double pulse, double minute
			       , PositionLockStyle pls, bool recompute, bool locked_to_meter = false);

	MeterSection* add_meter_locked (const Meter&, double beat, const Timecode::BBT_Time& where, framepos_t frame, PositionLockStyle pls, bool recompute);

	bool remove_tempo_locked (const TempoSection&);
	bool remove_meter_locked (const MeterSection&);

	TempoSection* copy_metrics_and_point (const Metrics& metrics, Metrics& copy, TempoSection* section) const;
	MeterSection* copy_metrics_and_point (const Metrics& metrics, Metrics& copy, MeterSection* section) const;
};

}; /* namespace ARDOUR */

LIBARDOUR_API std::ostream& operator<< (std::ostream&, const ARDOUR::Meter&);
LIBARDOUR_API std::ostream& operator<< (std::ostream&, const ARDOUR::Tempo&);
LIBARDOUR_API std::ostream& operator<< (std::ostream&, const ARDOUR::MetricSection&);

namespace PBD {
	DEFINE_ENUM_CONVERT (ARDOUR::TempoSection::Type)
}

#endif /* __ardour_tempo_h__ */
