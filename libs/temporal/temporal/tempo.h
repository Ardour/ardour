/*
    Copyright (C) 2017 Paul Davis

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

#ifndef __temporal_tempo_h__
#define __temporal_tempo_h__

#include <list>
#include <string>
#include <vector>
#include <cmath>
#include <exception>

#include <boost/intrusive/list.hpp>

#include <glibmm/threads.h>

#include "pbd/enum_convert.h"
#include "pbd/i18n.h"
#include "pbd/integer_division.h"
#include "pbd/memento_command.h"
#include "pbd/rcu.h"
#include "pbd/signals.h"
#include "pbd/statefuldestructible.h"

#include "temporal/visibility.h"
#include "temporal/beats.h"
#include "temporal/bbt_time.h"
#include "temporal/superclock.h"
#include "temporal/timeline.h"
#include "temporal/types.h"

/* A tempo map is built from 3 types of entities

   1) tempo markers
   2) meter (time signature) markers
   3) position markers

   Beats increase monotonically throughout the tempo map (BBT may not).

   The map has a single time domain at any time.
*/

namespace Temporal {

class Meter;
class TempoMap;

/* Conceptually, Point is similar to timepos_t. However, whereas timepos_t can
 * use the TempoMap to translate between time domains, Point cannot. Why not?
 * Because Point is foundational in building the tempo map, and we cannot
 * create a circular functional dependency between them. So a Point always has
 * its superclock and beat time defined and no translation between them is possible.
 */

class LIBTEMPORAL_API Point {
  public:
	Point (TempoMap const & map, superclock_t sc, Beats const & b, BBT_Time const & bbt) : _sclock (sc), _quarters (b), _bbt (bbt), _map (&map) {}
	Point (TempoMap const & map, XMLNode const &);

	virtual ~Point() {}

	virtual void set (superclock_t sc, Beats const & b, BBT_Time const & bbt) {
		_sclock = sc;
		_quarters = b;
		_bbt = bbt;
	}

	superclock_t sclock() const  { return _sclock; }
	Beats const & beats() const  { return _quarters; }
	BBT_Time const & bbt() const { return _bbt; }
	samplepos_t sample(samplecnt_t sr) const { return superclock_to_samples (sclock(), sr); }

	timepos_t time() const;

	struct sclock_comparator {
		bool operator() (Point const & a, Point const & b) const {
			return a.sclock() < b.sclock();
		}
	};

  	struct ptr_sclock_comparator {
		bool operator() (Point const * a, Point const * b) const {
			return a->sclock() < b->sclock();
		}
	};

	struct beat_comparator {
		bool operator() (Point const & a, Point const & b) const {
			return a.beats() < b.beats();
		}
	};

	struct bbt_comparator {
		bool operator() (Point const & a, Point const & b) const {
			return a.bbt() < b.bbt();
		}
	};

	/* all time members are supposed to be synced at all times, so we need
	   test only one.
	*/
	inline bool operator== (Point const & other) const { return _sclock == other._sclock; }
	inline bool operator!= (Point const & other) const { return _sclock != other._sclock; }

	TempoMap const & map() const { return *_map; }

  protected:
	superclock_t     _sclock;
	Beats            _quarters;
	BBT_Time         _bbt;
	TempoMap const * _map;

	void add_state (XMLNode &) const;

  protected:
	friend class TempoMap;
	void map_reset_set_sclock_for_sr_change (superclock_t sc) { _sclock = sc; }
};

class LIBTEMPORAL_API Rampable {
  protected:
	virtual ~Rampable() {}

  private:
	friend class TempoMap;
	virtual bool set_ramped (bool yn) = 0;
};

/** Tempo, the speed at which musical time progresses (BPM).
 */

class LIBTEMPORAL_API Tempo : public Rampable {
  private:
	/* beats per minute * big_numerator => rational number expressing (possibly fractional) bpm as superbeats-per-minute
	 *
	 * It is not required that big_numerator equal superclock_ticks_per_second but since the values in both cases have similar
	 * desired properties (many, many factors), it doesn't hurt to use the same number.
	 */
	static const superclock_t big_numerator = 508032000; // 2^10 * 3^4 * 5^3 * 7^2

  public:
	enum Type {
		Ramped,
		Constant
	};

	static std::string xml_node_name;

	Tempo (XMLNode const &);

	/**
	 * @param npm Note Types per minute
	 * @param note_type Note Type (default `4': quarter note)
	 */
	Tempo (double npm, int note_type = 4)
		: _npm (npm)
		, _enpm (npm)
		, _superclocks_per_note_type (double_npm_to_scpn (npm))
		, _end_superclocks_per_note_type (double_npm_to_scpn (npm))
		, _super_note_type_per_second (double_npm_to_snps (npm))
		, _end_super_note_type_per_second (double_npm_to_snps (npm))
		, _note_type (note_type)
		, _active (true)
		, _locked_to_meter (false)
		, _clamped (false)
		, _type (Tempo::Constant) {}

	Tempo (double npm, double enpm, int note_type = 4)
		: _npm (npm)
		, _enpm (npm)
		, _superclocks_per_note_type (double_npm_to_scpn (npm))
		, _end_superclocks_per_note_type (double_npm_to_scpn (enpm))
		, _super_note_type_per_second (double_npm_to_snps (npm))
		, _end_super_note_type_per_second (double_npm_to_snps (enpm))
		, _note_type (note_type)
		, _active (true)
		, _locked_to_meter (false)
		, _clamped (false)
		, _type (npm != enpm ? Tempo::Ramped : Tempo::Constant) {}

	/* these five methods should only be used to show and collect information to the user (for whom
	 * bpm as a floating point number is the obvious representation)
	 */
	double note_types_per_minute () const { return (superclock_ticks_per_second * 60.0) / _superclocks_per_note_type; }
	double end_note_types_per_minute () const { return (superclock_ticks_per_second * 60.0) / _end_superclocks_per_note_type; }
	double quarter_notes_per_minute() const { return (superclock_ticks_per_second * 60.0 * 4.0) / (_note_type * _superclocks_per_note_type); }
	double samples_per_note_type(samplecnt_t sr) const { return superclock_to_samples (superclocks_per_note_type (), sr); }
	double samples_per_quarter_note(samplecnt_t sr) const { return superclock_to_samples (superclocks_per_quarter_note(), sr); }
	void   set_note_types_per_minute (double npm) { _superclocks_per_note_type = double_npm_to_scpn (npm); }

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
	superclock_t end_superclocks_per_note_type () const {
		return _end_superclocks_per_note_type;
	}
	superclock_t end_superclocks_per_note_type (int note_type) const {
		return (_end_superclocks_per_note_type * _note_type) / note_type;
	}
	superclock_t end_superclocks_per_quarter_note () const {
		return end_superclocks_per_note_type (4);
	}
	superclock_t superclocks_per_ppqn () const {
		return superclocks_per_quarter_note() / ticks_per_beat;
	}

	static void superbeats_to_beats_ticks (int64_t sb, int32_t& b, int32_t& t) {
		b = sb / big_numerator;
		int64_t remain = sb - (b * big_numerator);
		t = int_div_round ((Temporal::ticks_per_beat * remain), big_numerator);
	}

	bool active () const { return _active; }
	void set_active (bool yn) { _active = yn; }

	bool locked_to_meter ()  const { return _locked_to_meter; }
	void set_locked_to_meter (bool yn) { _locked_to_meter = yn; }

	bool clamped() const { return _clamped; }
	bool set_clamped (bool yn);

	Type type() const { return _type; }

	bool ramped () const { return _type != Constant; }

	XMLNode& get_state () const;
	int set_state (XMLNode const&, int version);

	bool operator== (Tempo const & other) const {
		return _superclocks_per_note_type == other._superclocks_per_note_type &&
			_end_superclocks_per_note_type == other._end_superclocks_per_note_type &&
			_note_type == other._note_type &&
			_active == other._active &&
			_locked_to_meter == other._locked_to_meter &&
			_clamped == other._clamped &&
			_type == other._type;
	}

	bool operator!= (Tempo const & other) const {
		return _superclocks_per_note_type != other._superclocks_per_note_type ||
			_end_superclocks_per_note_type != other._end_superclocks_per_note_type ||
			_note_type != other._note_type ||
			_active != other._active ||
			_locked_to_meter != other._locked_to_meter ||
			_clamped != other._clamped ||
			_type != other._type;
	}

	uint64_t super_note_type_per_second() const { return _super_note_type_per_second; }

  protected:
	double       _npm;
	double       _enpm;
	superclock_t _superclocks_per_note_type;
	superclock_t _end_superclocks_per_note_type;
	uint64_t     _super_note_type_per_second;
	uint64_t     _end_super_note_type_per_second;
	int8_t       _note_type;
	bool         _active;
	bool         _locked_to_meter; /* XXX name has unclear meaning with nutempo */
	bool         _clamped;
	Type         _type;

	static inline uint64_t     double_npm_to_snps (double npm) { return (uint64_t) llround (npm * big_numerator / 60); }
	static inline superclock_t double_npm_to_scpn (double npm) { return (superclock_t) llround ((60./npm) * superclock_ticks_per_second); }

  private:
	bool set_ramped (bool yn);
};

/** Meter, or time signature (subdivisions per bar, and which note type is a single subdivision). */
class LIBTEMPORAL_API Meter {
  public:

	static std::string xml_node_name;

	Meter (XMLNode const &);
	Meter (int8_t dpb, int8_t nv) : _note_value (nv), _divisions_per_bar (dpb) {}

	int divisions_per_bar () const { return _divisions_per_bar; }
	int note_value() const { return _note_value; }

	int32_t ticks_per_grid () const { return (4 * Beats::PPQN) / _note_value; }

	inline bool operator==(const Meter& other) const { return _divisions_per_bar == other.divisions_per_bar() && _note_value == other.note_value(); }
	inline bool operator!=(const Meter& other) const { return _divisions_per_bar != other.divisions_per_bar() || _note_value != other.note_value(); }

	Meter& operator=(Meter const & other) {
		if (&other != this) {
			_divisions_per_bar = other._divisions_per_bar;
			_note_value = other._note_value;
		}
		return *this;
	}

	BBT_Time bbt_add (BBT_Time const & bbt, BBT_Offset const & add) const;
	BBT_Time bbt_subtract (BBT_Time const & bbt, BBT_Offset const & sub) const;
	BBT_Time round_to_bar (BBT_Time const &) const;
	BBT_Time round_down_to_bar (BBT_Time const &) const;
	BBT_Time round_up_to_bar (BBT_Time const &) const;
	BBT_Time round_up_to_beat (BBT_Time const &) const;
	BBT_Time round_to_beat (BBT_Time const &) const;
	Beats    to_quarters (BBT_Offset const &) const;

	XMLNode& get_state () const;
	int set_state (XMLNode const&, int version);

  protected:
	/** The type of "note" that a division represents.  For example, 4 is
	    a quarter (crotchet) note, 8 is an eighth (quaver) note, etc.
	*/
	int8_t _note_value;
	/* how many of '_note_value' make up a bar or measure */
	int8_t _divisions_per_bar;
};

/* A MeterPoint is literally just the combination of a Meter with a Point
 */
class LIBTEMPORAL_API MeterPoint : public Meter, public Point
{
  public:
	MeterPoint (TempoMap const & map, Meter const & m, superclock_t sc, Beats const & b, BBT_Time const & bbt) : Meter (m), Point (map, sc, b, bbt) {}
	MeterPoint (TempoMap const & map, XMLNode const &);

	Beats quarters_at (BBT_Time const & bbt) const;
	BBT_Time bbt_at (Beats const & beats) const;

	bool operator== (MeterPoint const & other) const {
		return Meter::operator== (other) && Point::operator== (other);
	}
	bool operator!= (MeterPoint const & other) const {
		return Meter::operator!= (other) || Point::operator!= (other);
	}

	boost::intrusive::list_member_hook<> _meter_hook;

	XMLNode& get_state () const;
};

/* A TempoPoint is a combination of a Tempo with a Point. However, if the temp
 * is ramped, then at some point we will need to compute the ramp coefficients
 * (c-per-quarter and c-per-superclock) and store them so that we can compute
 * time-at-quarter-note on demand.
 */

class LIBTEMPORAL_API TempoPoint : public Tempo, public Point
{
  public:
	TempoPoint (TempoMap const & map, Tempo const & t, superclock_t sc, Beats const & b, BBT_Time const & bbt) : Tempo (t), Point (map, sc, b, bbt), _omega (0.0) {}
	TempoPoint (Tempo const & t, Point const & p) : Tempo (t), Point (p), _omega (0) {}
	TempoPoint (TempoMap const & map, XMLNode const &);

	/* just change the tempo component, without moving */
	TempoPoint& operator=(Tempo const & t) {
		*((Tempo*)this) = t;
		return *this;
	}

	/* Given that this tempo point controls tempo for the time indicated by
	 * the argument of the following 4 functions, return information about
	 * that time. The first 3 return convert between domains (with
	 * ::sample_at() just being a convenience function); the fourth returns
	 * information about the tempo at that time.
	 */

	superclock_t superclock_at (Beats const & qn) const;
	samplepos_t  sample_at (Beats const & qn) const { return Temporal::superclock_to_samples (superclock_at (qn), TEMPORAL_SAMPLE_RATE); }
	superclock_t superclocks_per_note_type_at (timepos_t const &) const;

	/* This method should be used only for display purposes, and even
	 * then, only when absolutely necessary. It returns a double
	 * representation of the tempo, and we do not want to be using such
	 * representations ever, if we could.
	 */
	double note_types_per_minute_at_DOUBLE (timepos_t const & pos) const {
		return (superclock_ticks_per_second * 60.0) / superclocks_per_note_type_at (pos);
	}

	double omega() const { return _omega; }
	void compute_omega (samplecnt_t sr, superclock_t end_superclocks_per_note_type, Beats const & duration);
	bool actually_ramped () const { return Tempo::ramped() && (_omega != 0); }

	XMLNode& get_state () const;
	int set_state (XMLNode const&, int version);

	bool operator== (TempoPoint const & other) const {
		return Tempo::operator== (other) && Point::operator== (other);
	}
	bool operator!= (TempoPoint const & other) const {
		return Tempo::operator!= (other) || Point::operator!= (other);
	}

	boost::intrusive::list_member_hook<> _tempo_hook;

	Beats quarters_at_sample (samplepos_t sc) const { return quarters_at_superclock (samples_to_superclock (sc, TEMPORAL_SAMPLE_RATE)); }
	Beats quarters_at_superclock (superclock_t sc) const;

  private:
	double _omega;

};

/** Helper class to perform computations that require both Tempo and Meter
    at a given point in time.

    It may seem nicer to make this IS-A TempoPoint and IS-A MeterPoint. Doing
    so runs into multiple inheritance of Point, plus the major semantic issue
    that pairing a tempo and a meter does in fact allow for two positions, not
    one. That means we have to provide accessors to the TempoPoint and
    MeterPoint and thus it may as well be HAS-A rather than IS-A.

    This object should always be short lived. It holds references to a
    TempoPoint and a MeterPoint that are not lifetime-managed. It's just a
    convenience object, in essence, to avoid having to replicate the
    computation code that requires both tempo and meter information every place
    it is used.
*/
class LIBTEMPORAL_API TempoMetric {
  public:
	TempoMetric (TempoPoint & t, MeterPoint & m) : _tempo (&t), _meter (&m) {}
	~TempoMetric () {}

	TempoPoint & tempo() const { return *_tempo; }
	MeterPoint & meter() const { return *_meter; }

	/* even more convenient wrappers for individual aspects of a
	 * TempoMetric (i.e. just tempo or just meter information required
	 */

	superclock_t superclock_at (Beats const & qn) const { return _tempo->superclock_at (qn); }
	samplepos_t  sample_at (Beats const & qn) const { return _tempo->sample_at (qn); }
	Beats        quarters_at (BBT_Time const & bbt) const { return _meter->quarters_at (bbt); }
	BBT_Time     bbt_at (Beats const & beats) const { return _meter->bbt_at (beats); }

	superclock_t superclocks_per_note_type () const { return _tempo->superclocks_per_note_type (); }
	superclock_t end_superclocks_per_note_type () const {return _tempo->end_superclocks_per_note_type (); }
	superclock_t superclocks_per_note_type (int note_type) const {return _tempo->superclocks_per_note_type (note_type); }
	superclock_t superclocks_per_quarter_note () const {return _tempo->superclocks_per_quarter_note (); }
	superclock_t superclocks_per_ppqn () const {return _tempo->superclocks_per_ppqn (); }

	int note_type () const { return _tempo->note_type(); }
	int divisions_per_bar () const { return _meter->divisions_per_bar(); }
	int note_value() const { return _meter->note_value(); }
	BBT_Time   bbt_add (BBT_Time const & bbt, BBT_Offset const & add) const { return _meter->bbt_add (bbt, add); }
	BBT_Time   bbt_subtract (BBT_Time const & bbt, BBT_Offset const & sub) const { return _meter->bbt_subtract (bbt, sub); }
	BBT_Time round_to_bar (BBT_Time const & bbt) const { return _meter->round_to_bar (bbt); }
	Beats to_quarters (BBT_Offset const & bbo) const { return _meter->to_quarters (bbo); }

	/* combination methods that require both tempo and meter information */

	superclock_t superclocks_per_bar () const {
		return superclocks_per_grid () * _meter->divisions_per_bar();
	}
	superclock_t superclocks_per_grid () const {
		return int_div_round (_tempo->superclocks_per_note_type() * _tempo->note_type(), (int64_t) _meter->note_value());
	}

	superclock_t superclocks_per_note_type_at_superclock (superclock_t sc) const {
		if (!_tempo->actually_ramped()) {
			return _tempo->superclocks_per_note_type ();
		}
		return _tempo->superclocks_per_note_type() * exp (-_tempo->omega() * (sc - _tempo->sclock()));
	}

	superclock_t superclocks_per_grid_at (superclock_t sc) const {
		return int_div_round (superclocks_per_note_type_at_superclock (sc) * _tempo->note_type(), (int64_t) _meter->note_value());
	}

	BBT_Time bbt_at (superclock_t sc) const;
	superclock_t superclock_at (BBT_Time const &) const;

	samplepos_t samples_per_bar (samplecnt_t sr) const {
		return superclock_to_samples (superclocks_per_bar (), sr);
	}

	Beats quarters_at_sample (samplepos_t sc) const { return quarters_at_superclock (samples_to_superclock (sc, TEMPORAL_SAMPLE_RATE)); }
	Beats quarters_at_superclock (superclock_t sc) const { return _tempo->quarters_at_superclock (sc); }

  protected:
	TempoPoint* _tempo;
	MeterPoint* _meter;

};

/* A music time point is a place where BBT time is reset from
 * whatever it would be when just inferring from the usual counting. Its
 * position is given by a Point that might use superclock or Beats, and the
 * Point's BBT time member is overwritten.
 */
class LIBTEMPORAL_API MusicTimePoint : public Point
{
  public:
	MusicTimePoint (TempoMap const & map) : Point (map, 0, Beats(), BBT_Time()) {}
	MusicTimePoint (BBT_Time const & bbt_time, Point const & p) : Point (p) { _bbt = bbt_time; }
	MusicTimePoint (TempoMap const & map, XMLNode const &);

	boost::intrusive::list_member_hook<> _bartime_hook;

	XMLNode & get_state () const;
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

class LIBTEMPORAL_API TempoMapPoint : public Point, public TempoMetric
{
  public:
	TempoMapPoint (TempoMap & map, TempoMetric const & tm, superclock_t sc, Beats const & q, BBT_Time const & bbt)
		: Point (map, sc, q, bbt), TempoMetric (tm), _floating (false) {}
	~TempoMapPoint () {}

	/* called by a GUI that is manipulating the position of this point */
	void start_float ();
	void end_float ();
	bool floating() const { return _floating; }

	bool is_explicit_meter() const { return _meter->sclock() == sclock(); }
	bool is_explicit_tempo() const { return _tempo->sclock() == sclock(); }
	bool is_explicit_position() const { return false; }
	bool is_explicit () const { return is_explicit_meter() || is_explicit_tempo() || is_explicit_position(); }

  private:
	bool         _floating;
};

typedef std::list<TempoMapPoint> TempoMapPoints;

class LIBTEMPORAL_API TempoMap : public PBD::StatefulDestructible
{
	/* Any given thread must be able to carry out tempo-related arithmetic
	 * and time domain conversions using a consistent version of a
	 * TempoMap. The map could be updated at any time, and for any reason
	 * (typically from a GUI thread), but some other thread could be
	 * using the map to convert from audio to music time (for example).
	 *
	 * We do not want to use locks for this - this math may happen in a
	 * realtime thread, and even worse, the lock may need to be held for
	 * long periods of time in order to have the desired effect: a thread
	 * may be performing some tempo-based arithmetic as part of a complex
	 * operation that requires multiple steps. The tempo map it uses must
	 * remain consistent across all steps, and so we would have to hold the
	 * lock across them all. That would create awkward and difficult
	 * semantics for map users - somewhat arbitrary rules about how long
	 * one could hold the map for, etc.
	 *
	 * Elsewhere in the codebase, we use RCU to solve this sort of
	 * issue. For example, if we need to operate an the current list of
	 * Routes, we get read-only copy of the list, and iterate over it,
	 * knowing that even if the canonical version is being changed, the
	 * copy we are using will not.
	 *
	 * However, the tempo map's use is often implicit rather than
	 * explicit. The callstack to convert between an audio domain time and
	 * a music domain time should not require passing a tempo map into
	 * every call.
	 *
	 * The approach taken here is to use a combination of RCU and
	 * thread-local variables. Any given thread is by definition ... single
	 * threaded. If the thread has a thread-local copy of a tempo map, it
	 * will not change except at explicit calls to change it. The tempo map
	 * can be accessed from any method executed by the thread. But the
	 * relationship between the thread-local copy and "actual" tempo map(s)
	 * is managed via RCU, meaning that read-only access is cheap (no
	 * actual copy required).
	 *
	 */
  public:
	typedef boost::shared_ptr<TempoMap> SharedPtr;
  private:
	static thread_local SharedPtr _tempo_map_p;
	static SerializedRCUManager<TempoMap> _map_mgr;
  public:
	static void init ();

	static void update_thread_tempo_map() { _tempo_map_p = _map_mgr.reader(); }
	static SharedPtr use() { assert (_tempo_map_p); return _tempo_map_p; }
	static SharedPtr fetch() { update_thread_tempo_map(); return use(); }

	static SharedPtr write_copy() { return _map_mgr.write_copy(); }
	static void fetch_writable() { _tempo_map_p = write_copy(); }
	static void update (SharedPtr m);
	static void abort_update ();

	/* and now on with the rest of the show ... */

   public:
	TempoMap (Tempo const & initial_tempo, Meter const & initial_meter);
	TempoMap (TempoMap const &);
	TempoMap (XMLNode const &, int version);
	~TempoMap();

	TempoMap& operator= (TempoMap const &);

	void sample_rate_changed (samplecnt_t new_sr);

	/* methods which modify the map. These must all be called using
	 * RCU-style semantics: get a writable copy, modify it, then update via
	 * the RCU manager.
	 */

	bool set_ramped (TempoPoint&, bool);

	void insert_time (timepos_t const & pos, timecnt_t const & duration);
	bool remove_time (timepos_t const & pos, timecnt_t const & duration);

	void change_tempo (TempoPoint&, Tempo const &);

	MusicTimePoint & set_bartime (BBT_Time const &, timepos_t const &);
	void remove_bartime (MusicTimePoint const & tp);

	TempoPoint & set_tempo (Tempo const &, BBT_Time const &);
	TempoPoint & set_tempo (Tempo const &, timepos_t const &);

	MeterPoint & set_meter (Meter const &, BBT_Time const &);
	MeterPoint & set_meter (Meter const &, timepos_t const &);

	void remove_tempo (TempoPoint const &);
	void remove_meter (MeterPoint const &);

	/* these are a convenience method that just wrap some odd semantics */
	bool move_tempo (TempoPoint const & point, timepos_t const & destination, bool push = false);
	bool move_meter (MeterPoint const & point, timepos_t const & destination, bool push = false);

	void set_time_domain (TimeDomain td);
	int set_state (XMLNode const&, int version);

	/* END OF MODIFYING METHODS */

	TimeDomain time_domain() const { return _time_domain; }

	typedef std::list<Point*> Metrics;

	template<class T> void apply_with_metrics (T& obj, void (T::*method)(Metrics const &)) {
		Metrics metrics;
		for (Tempos::iterator t = _tempos.begin(); t != _tempos.end(); ++t) {
			metrics.push_back (&*t);
		}
		for (Meters::iterator m = _meters.begin(); m != _meters.end(); ++m) {
			metrics.push_back (&*m);
		}
		(obj.*method)(metrics);
	}

	bool can_remove (TempoPoint const &) const;
	bool can_remove (MeterPoint const &) const;

	bool is_initial (TempoPoint const &) const;
	bool is_initial (MeterPoint const &) const;

	uint32_t n_meters() const;
	uint32_t n_tempos() const;

	Tempo const * next_tempo (Tempo const &) const;
	Meter const * next_meter (Meter const &) const;

	TempoMetric metric_at (timepos_t const &) const;
	TempoMetric metric_at (superclock_t sc) const;
	TempoMetric metric_at (Beats const &b) const;
	TempoMetric metric_at (BBT_Time const & bbt) const;

	/* essentially convenience methods */

	MeterPoint& meter_at (timepos_t const & p) const { return metric_at (p).meter(); }
	MeterPoint& meter_at (superclock_t sc) const { return metric_at (sc).meter(); }
	MeterPoint& meter_at (Beats const &b) const { return metric_at (b).meter(); }
	MeterPoint& meter_at (BBT_Time const & bbt) const { return metric_at (bbt).meter(); }

	TempoPoint& tempo_at (timepos_t const & p) const { return metric_at (p).tempo(); }
	TempoPoint& tempo_at (superclock_t sc) const { return metric_at (sc).tempo(); }
	TempoPoint& tempo_at (Beats const &b) const { return metric_at (b).tempo(); }
	TempoPoint& tempo_at (BBT_Time const & bbt) const { return metric_at (bbt).tempo(); }

	TempoPoint const * previous_tempo (TempoPoint const &) const;

	/* convenience function */
	BBT_Time round_to_bar (BBT_Time const & bbt) const {
		return metric_at (bbt).meter().round_to_bar (bbt);
	}

	BBT_Time bbt_at (superclock_t sc) const;
	BBT_Time bbt_at (Beats const &) const;
	BBT_Time bbt_at (timepos_t const &) const;

	Beats quarters_at (BBT_Time const &) const;
	Beats quarters_at (timepos_t const &) const;

	superclock_t superclock_at (Beats const &) const;
	superclock_t superclock_at (BBT_Time const &) const;
	superclock_t superclock_at (timepos_t const &) const;

	samplepos_t sample_at (Beats const & b, samplecnt_t sr) const { return superclock_to_samples (superclock_at (b), sr); }
	samplepos_t sample_at (BBT_Time const & b, samplecnt_t sr) const { return superclock_to_samples (superclock_at (b), sr); }
	samplepos_t sample_at (timepos_t const & t, samplecnt_t sr) const { return superclock_to_samples (superclock_at (t), sr); }

	/* ways to walk along the tempo map, measure distance between points,
	 * etc.
	 */

	Beats superclock_delta_as_quarters (superclock_t start, superclock_t distance) const;
	Beats scwalk_to_quarters (superclock_t pos, superclock_t distance) const;
	Beats scwalk_to_quarters (Beats const & pos, superclock_t distance) const;
	superclock_t superclock_plus_quarters_as_superclock (superclock_t start, Beats const & distance) const;
	superclock_t superclock_quarters_delta_as_superclock (superclock_t start, Beats const & distance) const;
	superclock_t superclock_plus_bbt (superclock_t pos, BBT_Time op) const;

	timecnt_t bbt_duration_at (timepos_t const & pos, BBT_Offset const & bbt) const;
	Beats bbtwalk_to_quarters (Beats const & start, BBT_Offset const & distance) const;

	Temporal::timecnt_t full_duration_at (Temporal::timepos_t const &, Temporal::timecnt_t const & duration, Temporal::TimeDomain domain) const;

	BBT_Time bbt_walk (BBT_Time const &, BBT_Offset const &) const;

	void get_grid (TempoMapPoints& points, superclock_t start, superclock_t end, uint32_t bar_mod = 0);

	struct EmptyTempoMapException : public std::exception {
		virtual const char* what() const throw() { return "TempoMap is empty"; }
	};

	void dump (std::ostream&) const;

	static PBD::Signal0<void> MapChanged;

	XMLNode& get_state();

	class MementoBinder : public MementoCommandBinder<TempoMap> {
          public:
		MementoBinder () {}
		void set_state (XMLNode const & node, int version) const;
		XMLNode& get_state () const { return TempoMap::use()->get_state(); }
		std::string type_name() const { return X_("Temporal::TempoMap"); }
		void add_state (XMLNode*) {}
	};

	typedef boost::intrusive::member_hook<TempoPoint,boost::intrusive::list_member_hook<>, &TempoPoint::_tempo_hook> TempoHookOption;
	typedef boost::intrusive::member_hook<MeterPoint,boost::intrusive::list_member_hook<>, &MeterPoint::_meter_hook> MeterHookOption;
	typedef boost::intrusive::member_hook<MusicTimePoint,boost::intrusive::list_member_hook<>, &MusicTimePoint::_bartime_hook> BarTimeHookOption;

	typedef boost::intrusive::list<TempoPoint,TempoHookOption> Tempos;
	typedef boost::intrusive::list<MeterPoint,MeterHookOption> Meters;
	typedef boost::intrusive::list<MusicTimePoint,BarTimeHookOption> MusicTimes;

	Beats quarters_at_sample (samplepos_t sc) const { return quarters_at_superclock (samples_to_superclock (sc, TEMPORAL_SAMPLE_RATE)); }
	Beats quarters_at_superclock (superclock_t sc) const;

   private:
	Tempos       _tempos;
	Meters       _meters;
	MusicTimes   _bartimes;

	TimeDomain _time_domain;

	/* These return the TempoMetric in effect at the given time. If
	   can_match is true, then the TempoMetric may refer to a Tempo or
	   Meter at the given time. If can_match is false, the TempoMetric will
	   only refer to the Tempo or Metric preceding the given time.
	*/
	TempoMetric metric_at_locked (superclock_t, bool can_match = true) const;
	TempoMetric metric_at_locked (Beats const &, bool can_match = true) const;
	TempoMetric metric_at_locked (BBT_Time const &, bool can_match = true) const;

	int set_tempos_from_state (XMLNode const &);
	int set_meters_from_state (XMLNode const &);
	int set_music_times_from_state (XMLNode const &);

	MeterPoint & set_meter (Meter const &, superclock_t);

	TempoPoint* add_tempo (TempoPoint*);
	MeterPoint* add_meter (MeterPoint*);
	MusicTimePoint* add_or_replace_bartime (MusicTimePoint &);

	void add_point (Point &);

	void reset_starting_at (superclock_t);
	void reset_starting_at (Beats const &);

	void copy_points (TempoMap const & other);
};

} /* end of namespace Temporal */

#ifdef COMPILER_MSVC
#pragma warning(disable:4101)
#endif

namespace PBD {
DEFINE_ENUM_CONVERT(Temporal::Tempo::Type);
DEFINE_ENUM_CONVERT(Temporal::TimeDomain);
} /* namespace PBD */


namespace std {
std::ostream& operator<<(std::ostream& str, Temporal::TempoMapPoint const &);
std::ostream& operator<<(std::ostream& str, Temporal::Tempo const &);
std::ostream& operator<<(std::ostream& str, Temporal::Meter const &);
std::ostream& operator<<(std::ostream& str, Temporal::Point const &);
std::ostream& operator<<(std::ostream& str, Temporal::TempoPoint const &);
std::ostream& operator<<(std::ostream& str, Temporal::MeterPoint const &);
std::ostream& operator<<(std::ostream& str, Temporal::TempoMetric const &);
}

#endif /* __temporal_tempo_h__ */
