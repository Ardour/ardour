#ifndef __ardour_tempo_h__
#define __ardour_tempo_h__

#include <list>
#include <string>
#include <vector>
#include <cmath>
#include <exception>

#include <glibmm/threads.h>

#include "pbd/signals.h"

#include "temporal/beats.h"

#include "ardour/ardour.h"
#include "ardour/superclock.h"

#include "temporal/bbt_time.h"

namespace ARDOUR {

class Meter;
class TempoMap;

/** Tempo, the speed at which musical time progresses (BPM).
 */

class LIBARDOUR_API Tempo {
  public:
	/**
	 * @param npm Note Types per minute
	 * @param type Note Type (default `4': quarter note)
	 */
	Tempo (double npm, int type = 4) : _superclocks_per_note_type (double_npm_to_sc (npm)), _note_type (type) {}

	/* these two methods should only be used to show and collect information to the user (for whom
	 * bpm as a floating point number is the obvious representation)
	 */
	double note_types_per_minute () const { return (superclock_ticks_per_second * 60.0) / _superclocks_per_note_type; }
	void set_note_types_per_minute (double npm) { _superclocks_per_note_type = double_npm_to_sc (npm); }

	int note_type () const { return _note_type; }

	superclock_t superclocks_per_note_type () const {
		return _superclocks_per_note_type;
	}
	superclock_t superclocks_per_note_type (int note_type) const {
		return (_superclocks_per_note_type * _note_type) / note_type;
	}
	superclock_t superclocks_per_quarter_note () const {
		return superclocks_per_note_type (4);
	}

	Tempo& operator=(Tempo const& other) {
		if (&other != this) {
			_superclocks_per_note_type = other._superclocks_per_note_type;
			_note_type = other._note_type;
		}
		return *this;
	}

  protected:
	superclock_t _superclocks_per_note_type;
	int8_t       _note_type;

	static inline double       sc_to_double_npm (superclock_t sc) { return (superclock_ticks_per_second * 60.0) / sc; }
	static inline superclock_t double_npm_to_sc (double npm) { return llrint ((superclock_ticks_per_second / npm) * 60.0); }
};

/** Meter, or time signature (subdivisions per bar, and which note type is a single subdivision). */
class LIBARDOUR_API Meter {
  public:
	Meter (int8_t dpb, int8_t nv) : _note_value (nv), _divisions_per_bar (dpb) {}

	int divisions_per_bar () const { return _divisions_per_bar; }
	int note_value() const { return _note_value; }

	inline bool operator==(const Meter& other) { return _divisions_per_bar == other.divisions_per_bar() && _note_value == other.note_value(); }
	inline bool operator!=(const Meter& other) { return _divisions_per_bar != other.divisions_per_bar() || _note_value != other.note_value(); }

	Meter& operator=(Meter const & other) {
		if (&other != this) {
			_divisions_per_bar = other._divisions_per_bar;
			_note_value = other._note_value;
		}
		return *this;
	}

	Timecode::BBT_Time   bbt_add (Timecode::BBT_Time const & bbt, Timecode::BBT_Offset const & add) const;
	Timecode::BBT_Time   bbt_subtract (Timecode::BBT_Time const & bbt, Timecode::BBT_Offset const & sub) const;
	Timecode::BBT_Offset bbt_delta (Timecode::BBT_Time const & bbt, Timecode::BBT_Time const & sub) const;

	Timecode::BBT_Time round_up_to_bar (Timecode::BBT_Time const &) const;
	Timecode::BBT_Time round_down_to_bar (Timecode::BBT_Time const &) const;
	Timecode::BBT_Time round_to_bar (Timecode::BBT_Time const &) const;

	Temporal::Beats to_quarters (Timecode::BBT_Offset const &) const;

  protected:
	/** The type of "note" that a division represents.  For example, 4 is
	    a quarter (crotchet) note, 8 is an eighth (quaver) note, etc.
	*/
	int8_t _note_value;
	/* how many of '_note_value' make up a bar or measure */
	int8_t _divisions_per_bar;
};

/** Helper class to keep track of the Meter *AND* Tempo in effect
    at a given point in time.
*/
class LIBARDOUR_API TempoMetric : public Tempo, public Meter {
  public:
	TempoMetric (Tempo const & t, Meter const & m, bool ramp) : Tempo (t), Meter (m), _c_per_quarter (0.0), _c_per_superclock (0.0), _ramped (ramp) {}
	~TempoMetric () {}

	double c_per_superclock () const { return _c_per_superclock; }
	double c_per_quarter () const { return _c_per_quarter; }

	void compute_c_superclock (samplecnt_t sr, superclock_t end_superclocks_per_note_type, superclock_t duration);
	void compute_c_quarters (samplecnt_t sr, superclock_t end_superclocks_per_note_type, Temporal::Beats const & duration);

	superclock_t superclocks_per_bar (samplecnt_t sr) const;
	superclock_t superclocks_per_grid (samplecnt_t sr) const;

	superclock_t superclock_at_qn (Temporal::Beats const & qn) const;
	superclock_t superclock_per_note_type_at_superclock (superclock_t) const;

	bool ramped () const { return _ramped; }
	void set_ramped (bool yn) { _ramped = yn; } /* caller must mark something dirty to force recompute */

  private:
	double _c_per_quarter;
	double _c_per_superclock;
	bool   _ramped;
};

/** Tempo Map - mapping of timecode to musical time.
 * convert audio-samples, sample-rate to Bar/Beat/Tick, Meter/Tempo
 */

/* TempoMap concepts

   we have several different ways of talking about time:

   * PULSE : whole notes, just because. These are linearly related to any other
             note type, so if you know a number of pulses (whole notes), you
             know the corresponding number of any other note type (e.g. quarter
             notes).

   * QUARTER NOTES : just what the name says. A lot of MIDI software and
                     concepts assume that a "beat" is a quarter-note.

   * BEAT : a fraction of a PULSE. Defined by the meter in effect, so requires
            meter (time signature) information to convert to/from PULSE or QUARTER NOTES.
            In a 5/8 time, a BEAT is 1/8th note. In a 4/4 time, a beat is quarter note.
            This means that measuring time in BEATS is potentially non-linear (if
            the time signature changes, there will be a different number of BEATS
            corresponding to a given time in any other unit).

   * SUPERCLOCK : a very high resolution clock whose frequency
                  has as factors all common sample rates and all common note
                  type divisors. Related to MINUTES or SAMPLES only when a
                  sample rate is known. Related to PULSE or QUARTER NOTES only
                  when a tempo is known.

   * MINUTES : wallclock time measurement. related to SAMPLES or SUPERCLOCK
               only when a sample rate is known.


   * SAMPLES : audio time measurement. Related to MINUTES or SUPERCLOCK only
               when a sample rate is known

   * BBT : bars|beats|ticks ... linearly related to BEATS but with the added
           semantics of bars ("measures") added, in which beats are broken up
           into groups of bars ("measures"). Requires meter (time signature)
           information to compute to/from a given BEATS value. Contains no
           additional time information compared to BEATS, but does have
           additional semantic information.

  Nick sez: not every note onset is on a tick
  Paul wonders: if it's 8 samples off, does it matter?
  Nick sez: it should not phase with existing audio

 */

class LIBARDOUR_API TempoMapPoint
{
  public:
	enum Flag {
		ExplicitTempo = 0x1,
		ExplicitMeter = 0x2,
	};

	TempoMapPoint (Flag f, Tempo const& t, Meter const& m, superclock_t sc, Temporal::Beats const & q, Timecode::BBT_Time const & bbt, PositionLockStyle psl, bool ramp = false)
		: _flags (f), _explicit (t, m, psl, ramp), _sclock (sc), _quarters (q), _bbt (bbt), _dirty (true), _map (0) {}
	TempoMapPoint (TempoMapPoint const & tmp, superclock_t sc, Temporal::Beats const & q, Timecode::BBT_Time const & bbt)
		: _flags (Flag (0)), _reference (&tmp), _sclock (sc), _quarters (q), _bbt (bbt), _dirty (true), _map (0) {}
	~TempoMapPoint () {}

	void set_map (TempoMap* m);

	Flag flags() const       { return _flags; }
	bool is_explicit() const { return _flags != Flag (0); }
	bool is_implicit() const { return _flags == Flag (0); }

	superclock_t superclocks_per_note_type (int8_t note_type) const {
		if (is_explicit()) {
			return _explicit.metric.superclocks_per_note_type (note_type);
		}
		return _reference->superclocks_per_note_type (note_type);
	}

	struct BadTempoMetricLookup : public std::exception {
		virtual const char* what() const throw() { return "cannot obtain non-const Metric from implicit map point"; }
	};

	bool                dirty()  const { return _dirty; }

	superclock_t        sclock() const      { return _sclock; }
	Temporal::Beats const & quarters() const  { return _quarters; }
	Timecode::BBT_Time  const & bbt() const { return _bbt; }
	bool                ramped() const      { return metric().ramped(); }
	TempoMetric const & metric() const      { return is_explicit() ? _explicit.metric : _reference->metric(); }
	PositionLockStyle   lock_style() const  { return is_explicit() ? _explicit.lock_style : _reference->lock_style(); }

	void compute_c_superclock (samplecnt_t sr, superclock_t end_superclocks_per_note_type, superclock_t duration) { if (is_explicit()) { _explicit.metric.compute_c_superclock (sr, end_superclocks_per_note_type, duration); } }
	void compute_c_quarters (samplecnt_t sr, superclock_t end_superclocks_per_note_type, Temporal::Beats const & duration) { if (is_explicit()) { _explicit.metric.compute_c_quarters (sr, end_superclocks_per_note_type, duration); } }

	/* None of these properties can be set for an Implicit point, because
	 * they are determined by the TempoMapPoint pointed to by _reference.
	 */

	void set_sclock (superclock_t  sc) { if (is_explicit()) { _sclock = sc; _dirty = true; } }
	void set_quarters (Temporal::Beats const & q) { if (is_explicit()) { _quarters = q; _dirty = true;  } }
	void set_bbt (Timecode::BBT_Time const & bbt) {  if (is_explicit()) { _bbt = bbt; _dirty = true;  } }
	void set_dirty (bool yn);
	void set_lock_style (PositionLockStyle psl) {  if (is_explicit()) { _explicit.lock_style = psl; _dirty = true; } }

	void make_explicit (Flag f) {
		_flags = Flag (_flags|f);
		/* since _metric and _reference are part of an anonymous union,
		   avoid possible compiler glitches by copying to a stack
		   variable first, then assign.
		*/
		TempoMetric tm (_explicit.metric);
		_explicit.metric = tm;
		_dirty = true;
	}

	void make_implicit (TempoMapPoint & tmp) { _flags = Flag (0); _reference = &tmp; }

	Temporal::Beats quarters_at (superclock_t sc) const;
	Temporal::Beats quarters_at (Timecode::BBT_Time const &) const;

	Timecode::BBT_Time bbt_at (Temporal::Beats const &) const;

#if 0
	XMLNode& get_state() const;
	int set_state (XMLNode const&, int version);
#endif

	struct SuperClockComparator {
		bool operator() (TempoMapPoint const & a, TempoMapPoint const & b) const { return a.sclock() < b.sclock(); }
	};

	struct QuarterComparator {
		bool operator() (TempoMapPoint const & a, TempoMapPoint const & b) const { return a.quarters() < b.quarters(); }
	};

	struct BBTComparator {
		bool operator() (TempoMapPoint const & a, TempoMapPoint const & b) const { return a.bbt() < b.bbt(); }
	};

  protected:
	friend class TempoMap;
	void map_reset_set_sclock_for_sr_change (superclock_t sc) { _sclock = sc; }

  private:
	struct ExplicitInfo {
		ExplicitInfo (Tempo const & t, Meter const & m, PositionLockStyle psl, bool ramp) : metric (t, m, ramp), lock_style (psl) {}

		TempoMetric       metric;
		PositionLockStyle lock_style;
	};

	Flag                  _flags;
	union {
		TempoMapPoint const * _reference;
		ExplicitInfo          _explicit;
	};
	superclock_t          _sclock;
	Temporal::Beats       _quarters;
	Timecode::BBT_Time    _bbt;
	bool                  _dirty;
	TempoMap*             _map;
};

typedef std::list<TempoMapPoint> TempoMapPoints;

class LIBARDOUR_API TempoMap
{
   public:
	TempoMap (Tempo const & initial_tempo, Meter const & initial_meter, samplecnt_t sr);

	void set_dirty (bool yn);

	void set_sample_rate (samplecnt_t sr);
	samplecnt_t sample_rate() const { return _sample_rate; }

	void remove_explicit_point (superclock_t);

	bool move_to (superclock_t current, superclock_t destination, bool push = false);

	bool set_tempo_and_meter (Tempo const &, Meter const &, superclock_t, bool ramp, bool flexible);

	bool set_tempo (Tempo const &, Timecode::BBT_Time const &, bool ramp = false);
	bool set_tempo (Tempo const &, superclock_t, bool ramp = false);

	bool set_meter (Meter const &, Timecode::BBT_Time const &);
	bool set_meter (Meter const &, superclock_t);

	Meter const & meter_at (superclock_t sc) const;
	Meter const & meter_at (Temporal::Beats const & b) const;
	Meter const & meter_at (Timecode::BBT_Time const & bbt) const;
	Tempo const & tempo_at (superclock_t sc) const;
	Tempo const & tempo_at (Temporal::Beats const &b) const;
	Tempo const & tempo_at (Timecode::BBT_Time const & bbt) const;

	Timecode::BBT_Time bbt_at (superclock_t sc) const;
	Timecode::BBT_Time bbt_at (Temporal::Beats const &) const;
	Temporal::Beats quarter_note_at (superclock_t sc) const;
	Temporal::Beats quarter_note_at (Timecode::BBT_Time const &) const;
	superclock_t superclock_at (Temporal::Beats const &) const;
	superclock_t superclock_at (Timecode::BBT_Time const &) const;

	TempoMapPoint const & const_point_at (superclock_t sc) const { return *const_iterator_at (sc); }
	TempoMapPoint const & const_point_at (Temporal::Beats const & b) const { return *const_iterator_at (b); }
	TempoMapPoint const & const_point_at (Timecode::BBT_Time const & bbt) const { return *const_iterator_at (bbt); }

	TempoMapPoint const & const_point_after (superclock_t sc) const;
	TempoMapPoint const & const_point_after (Temporal::Beats const & b) const;
	TempoMapPoint const & const_point_after (Timecode::BBT_Time const & bbt) const;

	/* If resolution == Temporal::Beats() (i.e. zero), then the grid that is
	   returned will contain a mixture of implicit and explicit points,
	   and will only be valid as long as this map remains unchanged
	   (because the implicit points may reference explicit points in the
	   map.

	   If resolution != Temporal::Beats() (i.e. non-zero), then the in-out @param
	   grid will contain only explicit points that do not reference this
	   map in anyway.
	*/
	void get_grid (TempoMapPoints& points, superclock_t start, superclock_t end, Temporal::Beats const & resolution);

	void get_bar_grid (TempoMapPoints& points, superclock_t start, superclock_t end, int32_t bar_gap);

	struct EmptyTempoMapException : public std::exception {
		virtual const char* what() const throw() { return "TempoMap is empty"; }
	};

	void dump (std::ostream&);
	void rebuild (superclock_t limit);

	PBD::Signal2<void,superclock_t,superclock_t> Changed;

   private:
	TempoMapPoints _points;
	samplecnt_t     _sample_rate;
	mutable Glib::Threads::RWLock _lock;
	bool _dirty;

	/* these return an iterator that refers to the TempoMapPoint at or most immediately preceding the given position.
	 *
	 * Conceptually, these could be const methods, but C++ prevents them returning a non-const iterator in that case.
	 *
	 * Note that they cannot return an invalid iterator (e.g. _points.end()) because:
	 *
	 *    - if the map is empty, an exception is thrown
	 *    - if the given time is before the first map entry, _points.begin() is returned
	 *    - if the given time is after the last map entry, the equivalent of _points.rbegin() is returned
	 *    - if the given time is within the map entries, a valid iterator will be returned
	 */

	TempoMapPoints::iterator iterator_at (superclock_t sc);
	TempoMapPoints::iterator iterator_at (Temporal::Beats const &);
	TempoMapPoints::iterator iterator_at (Timecode::BBT_Time const &);

	TempoMapPoints::const_iterator const_iterator_at (superclock_t sc) const { return const_cast<TempoMap*>(this)->iterator_at (sc); }
	TempoMapPoints::const_iterator const_iterator_at (Temporal::Beats const & b) const { return const_cast<TempoMap*>(this)->iterator_at (b); }
	TempoMapPoints::const_iterator const_iterator_at (Timecode::BBT_Time const & bbt) const { return const_cast<TempoMap*>(this)->iterator_at (bbt); }

	/* Returns the TempoMapPoint at or most immediately preceding the given time. If the given time is
	 * before the first map entry, then the first map entry will be returned, which underlies the semantics
	 * that the first map entry's values propagate backwards in time if not at absolute zero.
	 *
	 * As for iterator_at(), define both const+const and non-const variants, because C++ won't let us return a non-const iterator
	   from a const method (which is a bit silly, but presumably aids compiler reasoning).
	*/

	TempoMapPoint & point_at (superclock_t sc) { return *iterator_at (sc); }
	TempoMapPoint & point_at (Temporal::Beats const & b) { return *iterator_at (b); }
	TempoMapPoint & point_at (Timecode::BBT_Time const & bbt) { return *iterator_at (bbt); }

	Meter const & meter_at_locked (superclock_t sc) const { return const_point_at (sc).metric(); }
	Meter const & meter_at_locked (Temporal::Beats const & b) const { return const_point_at (b).metric(); }
	Meter const & meter_at_locked (Timecode::BBT_Time const & bbt) const { return const_point_at (bbt).metric(); }
	Tempo const & tempo_at_locked (superclock_t sc) const { return const_point_at (sc).metric(); }
	Tempo const & tempo_at_locked (Temporal::Beats const &b) const { return const_point_at (b).metric(); }
	Tempo const & tempo_at_locked (Timecode::BBT_Time const & bbt) const { return const_point_at (bbt).metric(); }
	Timecode::BBT_Time bbt_at_locked (superclock_t sc) const;
	Timecode::BBT_Time bbt_at_locked (Temporal::Beats const &) const;
	Temporal::Beats quarter_note_at_locked (superclock_t sc) const;
	Temporal::Beats quarter_note_at_locked (Timecode::BBT_Time const &) const;
	superclock_t superclock_at_locked (Temporal::Beats const &) const;
	superclock_t superclock_at_locked (Timecode::BBT_Time const &) const;

	void rebuild_locked (superclock_t limit);
	void dump_locked (std::ostream&);
};

}

std::ostream& operator<<(std::ostream&, ARDOUR::TempoMapPoint const &);
std::ostream& operator<<(std::ostream&, ARDOUR::Tempo const &);
std::ostream& operator<<(std::ostream&, ARDOUR::Meter const &);

#endif /* __ardour_tempo_h__ */
