#include "t.h"

using namespace ARDOUR;
using std::cerr;
using std::cout;
using std::endl;

/* overloaded operator* that avoids floating point math when multiplying a superclock position by a number of quarter notes */
superclock_t operator*(superclock_t sc, Temporal::Beats const & b) { return (sc * ((b.get_beats() * Temporal::Beats::PPQN) + b.get_ticks())) / Temporal::Beats::PPQN; }

Timecode::BBT_Time
Meter::bbt_add (Timecode::BBT_Time const & bbt, Timecode::BBT_Offset const & add) const
{
	int32_t bars = bbt.bars;
	int32_t beats = bbt.beats;
	int32_t ticks = bbt.ticks;

	if ((bars ^ add.bars) < 0) {
		/* signed-ness varies */
		if (abs(add.bars) >= abs(bars)) {
			/* addition will change which side of "zero" the answer is on;
			   adjust bbt.bars towards zero to deal with "unusual" BBT math
			*/
			if (bars < 0) {
				bars++;
			} else {
				bars--;
			}
		}
	}

	if ((beats ^ add.beats) < 0) {
		/* signed-ness varies */
		if (abs (add.beats) >= abs (beats)) {
			/* adjust bbt.beats towards zero to deal with "unusual" BBT math */
			if (beats < 0) {
				beats++;
			} else {
				beats--;
			}
		}
	}

	Timecode::BBT_Offset r (bars + add.bars, beats + add.beats, ticks + add.ticks);

	if (r.ticks >= Temporal::Beats::PPQN) {
		r.beats += r.ticks / Temporal::Beats::PPQN;
		r.ticks %= Temporal::Beats::PPQN;
	}

	if (r.beats > _divisions_per_bar) {
		r.bars += r.beats / _divisions_per_bar;
		r.beats %= _divisions_per_bar;
	}

	if (r.beats == 0) {
		r.beats = 1;
	}

	if (r.bars == 0) {
		r.bars = 1;
	}

	return Timecode::BBT_Time (r.bars, r.beats, r.ticks);
}

Timecode::BBT_Time
Meter::bbt_subtract (Timecode::BBT_Time const & bbt, Timecode::BBT_Offset const & sub) const
{
	int32_t bars = bbt.bars;
	int32_t beats = bbt.beats;
	int32_t ticks = bbt.ticks;

	if ((bars ^ sub.bars) < 0) {
		/* signed-ness varies */
		if (abs (sub.bars) >= abs (bars)) {
			/* adjust bbt.bars towards zero to deal with "unusual" BBT math */
			if (bars < 0) {
				bars++;
			} else {
				bars--;
			}
		}
	}

	if ((beats ^ sub.beats) < 0) {
		/* signed-ness varies */
		if (abs (sub.beats) >= abs (beats)) {
			/* adjust bbt.beats towards zero to deal with "unusual" BBT math */
			if (beats < 0) {
				beats++;
			} else {
				beats--;
			}
		}
	}

	Timecode::BBT_Offset r (bars - sub.bars, beats - sub.beats, ticks - sub.ticks);

	if (r.ticks < 0) {
		r.beats -= 1 - (r.ticks / Temporal::Beats::PPQN);
		r.ticks = Temporal::Beats::PPQN + (r.ticks % Temporal::Beats::PPQN);
	}

	if (r.beats <= 0) {
		r.bars -= 1 - (r.beats / _divisions_per_bar);
		r.beats = _divisions_per_bar + (r.beats % _divisions_per_bar);
	}

	if (r.beats == 0) {
		r.beats = 1;
	}

	if (r.bars <= 0) {
		r.bars -= 1;
	}

	return Timecode::BBT_Time (r.bars, r.beats, r.ticks);
}

Timecode::BBT_Offset
Meter::bbt_delta (Timecode::BBT_Time const & a, Timecode::BBT_Time const & b) const
{
	return Timecode::BBT_Offset (a.bars - b.bars, a.beats - b.beats, a.ticks - b.ticks);
}

Timecode::BBT_Time
Meter::round_up_to_bar (Timecode::BBT_Time const & bbt) const
{
	Timecode::BBT_Time b = bbt.round_up_to_beat ();
	if (b.beats > 1) {
		b.bars++;
		b.beats = 1;
	}
	return b;
}

Temporal::Beats
Meter::to_quarters (Timecode::BBT_Offset const & offset) const
{
	Temporal::Beats b;

	b += (offset.bars * _divisions_per_bar * 4) / _note_value;
	b += (offset.beats * 4) / _note_value;
	b += Temporal::Beats::ticks (offset.ticks);

	return b;
}

superclock_t
TempoMetric::superclock_per_note_type_at_superclock (superclock_t sc) const
{
	return superclocks_per_note_type () * expm1 (_c_per_superclock * sc);
}

superclock_t
TempoMetric::superclocks_per_grid (samplecnt_t sr) const
{
	return (superclock_ticks_per_second * Meter::note_value()) / (note_types_per_minute() / Tempo::note_type());
}

superclock_t
TempoMetric::superclocks_per_bar (samplecnt_t sr) const
{
	return superclocks_per_grid (sr) * _divisions_per_bar;
}

/*
Ramp Overview

      |                     *
Tempo |                   *
Tt----|-----------------*|
Ta----|--------------|*  |
      |            * |   |
      |         *    |   |
      |     *        |   |
T0----|*             |   |
  *   |              |   |
      _______________|___|____
      time           a   t (next tempo)
      [        c         ] defines c

Duration in beats at time a is the integral of some Tempo function.
In our case, the Tempo function (Tempo at time t) is
T(t) = T0(e^(ct))

>>1/S(t) = (1/S0)(e^ct) => (1/S)(t) = (e^(ct))/S0 => S(t) = S0/(e^(ct))

with function constant
c = log(Ta/T0)/a

>>c = log ((1/Sa)/(1/S0)) / a => c = log (S0/Sa) / a

so
a = log(Ta/T0)/c

>>a = log ((1/Ta)/(1/S0) / c => a = log (S0/Sa) / c

The integral over t of our Tempo function (the beat function, which is the duration in beats at some time t) is:
b(t) = T0(e^(ct) - 1) / c

>>b(t) = 1/S0(e^(ct) - 1) / c  => b(t) = (e^(ct) - 1) / (c * S0)

To find the time t at beat duration b, we use the inverse function of the beat function (the time function) which can be shown to be:
t(b) = log((c.b / T0) + 1) / c

>>t(b) = log((c*b / (1/S0)) + 1) / c => t(b) = log ((c*b * S0) + 1) / c

The time t at which Tempo T occurs is a as above:
t(T) = log(T / T0) / c

>> t(1/S) = log ((1/S) / (1/S0) /c => t(1/S) = log (S0/S) / c

The beat at which a Tempo T occurs is:
b(T) = (T - T0) / c

>> b(1/S) = (1/S - 1/S0) / c

The Tempo at which beat b occurs is:
T(b) = b.c + T0

>> T(b) = b.c + (1/S0)

We define c for this tempo ramp by placing a new tempo section at some time t after this one.
Our problem is that we usually don't know t.
We almost always know the duration in beats between this and the new section, so we need to find c in terms of the beat function.
Where a = t (i.e. when a is equal to the time of the next tempo section), the beat function reveals:
t = b log (Ta / T0) / (T0 (e^(log (Ta / T0)) - 1))

By substituting our expanded t as a in the c function above, our problem is reduced to:
c = T0 (e^(log (Ta / T0)) - 1) / b

>> c = (1/S0) (e^(log ((1/Sa) / (1/S0))) - 1) / b => c = (1/S0) (e^(log (S0/Sa)) - 1) / b => c (e^(log (S0/Sa)) - 1) / (b * S0)

Of course the word 'beat' has been left loosely defined above.
In music, a beat is defined by the musical pulse (which comes from the tempo)
and the meter in use at a particular time (how many  pulse divisions there are in one bar).
It would be more accurate to substitute the work 'pulse' for 'beat' above.

 */

/* equation to compute c is:
 *
 *    c = log (Ta / T0) / a
 *
 * where
 *
 *   a : time into section (from section start
 *  T0 : tempo at start of section
 *  Ta : tempo at time a into section
 *
 * THE UNITS QUESTION
 *
 * log (Ta / T0) / (time-units) => C is in per-time-units (1/time-units)
 *
 * so the question is what are the units of a, and thus c?
 *
 * we could use ANY time units (because we can measure a in any time units)
 * but whichever one we pick dictates how we can use c in the future since
 * all subsequent computations will need to use the same time units.
 *
 * options:
 *
 *    pulses        ... whole notes, possibly useful, since we can use it for any other note_type
 *    quarter notes ... linearly related to pulses
 *    beats         ... not a fixed unit of time
 *    minutes       ... linearly related to superclocks
 *    samples       ... needs sample rate
 *    superclocks   ... frequently correct
 *
 * so one answer might be to compute c in two different units so that we have both available.
 *
 * hence, compute_c_superclocks() and compute_c_pulses()
 */

void
TempoMetric::compute_c_superclock (samplecnt_t sr, superclock_t end_scpqn, superclock_t superclock_duration)
{
	if ((superclocks_per_quarter_note() == end_scpqn) || !ramped()) {
		_c_per_superclock = 0.0;
		return;
	}

	_c_per_superclock = log ((double) superclocks_per_quarter_note () / end_scpqn) / superclock_duration;
}
void
TempoMetric::compute_c_quarters (samplecnt_t sr, superclock_t end_scpqn, Temporal::Beats const & quarter_duration)
{
	if ((superclocks_per_quarter_note () == end_scpqn) || !ramped()) {
		_c_per_quarter = 0.0;
		return;
	}

	_c_per_quarter = log (superclocks_per_quarter_note () / (double) end_scpqn) /  quarter_duration.to_double();
}

superclock_t
TempoMetric::superclock_at_qn (Temporal::Beats const & qn) const
{
	if (_c_per_quarter == 0.0) {
		/* not ramped, use linear */
		return llrint (superclocks_per_quarter_note () * qn.to_double());
	}

	return llrint (superclocks_per_quarter_note() * (log1p (_c_per_quarter * qn.to_double()) / _c_per_quarter));
}

void
TempoMapPoint::set_map (TempoMap* m)
{
	_map = m;
}

void
TempoMapPoint::set_dirty (bool yn)
{
	if (yn != _dirty) {
		_dirty = yn;
		if (yn && _map) {
			_map->set_dirty (true);
		}
	}
}

Temporal::Beats
TempoMapPoint::quarters_at (superclock_t sc) const
{
	/* This TempoMapPoint must already have a fully computed metric and position */

	if (!ramped()) {
		return _quarters + Temporal::Beats ((sc - _sclock) / (double) (metric().superclocks_per_quarter_note ()));
	}

	return _quarters + Temporal::Beats (expm1 (metric().c_per_superclock() * (sc - _sclock)) / (metric().c_per_superclock() * metric().superclocks_per_quarter_note ()));
}

Temporal::Beats
TempoMapPoint::quarters_at (Timecode::BBT_Time const & bbt) const
{
	/* This TempoMapPoint must already have a fully computed metric and position */

	Timecode::BBT_Offset offset = metric().bbt_delta (bbt, _bbt);
	return _quarters + metric().to_quarters (offset);
}

Timecode::BBT_Time
TempoMapPoint::bbt_at (Temporal::Beats const & qn) const
{
	/* This TempoMapPoint must already have a fully computed metric and position */

	Temporal::Beats quarters_delta = qn - _quarters;
	int32_t ticks_delta = quarters_delta.to_ticks (Temporal::Beats::PPQN);
	return metric().bbt_add (_bbt, Timecode::BBT_Offset (0, 0,  ticks_delta));
}

TempoMap::TempoMap (Tempo const & initial_tempo, Meter const & initial_meter, samplecnt_t sr)
	: _sample_rate (sr)
	, _dirty (false)
{
	TempoMapPoint tmp (TempoMapPoint::Flag (TempoMapPoint::ExplicitMeter|TempoMapPoint::ExplicitTempo), initial_tempo, initial_meter, 0, Temporal::Beats(), Timecode::BBT_Time(), AudioTime);
	_points.push_back (tmp);
}

void
TempoMap::set_dirty (bool yn)
{
	_dirty = yn;
}

Meter const &
TempoMap::meter_at (superclock_t sc) const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	return meter_at_locked (sc);
}

Meter const &
TempoMap::meter_at (Temporal::Beats const & b) const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	return meter_at_locked (b);
}

Meter const &
TempoMap::meter_at (Timecode::BBT_Time const & bbt) const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	return meter_at_locked (bbt);
}

Tempo const &
TempoMap::tempo_at (superclock_t sc) const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	return tempo_at_locked (sc);
}

Tempo const &
TempoMap::tempo_at (Temporal::Beats const &b) const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	return tempo_at_locked (b);
}

Tempo const &
TempoMap::tempo_at (Timecode::BBT_Time const & bbt) const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	return tempo_at_locked (bbt);
}

void
TempoMap::rebuild (superclock_t limit)
{
	Glib::Threads::RWLock::WriterLock lm (_lock);

	/* step one: remove all implicit points after a dirty explicit point */

  restart:
	TempoMapPoints::iterator tmp = _points.begin();
	TempoMapPoints::iterator first_explicit_dirty = _points.end();

	while ((tmp != _points.end()) && (tmp->is_implicit() || (tmp->is_explicit() && !tmp->dirty ()))) {
		++tmp;
	}

	first_explicit_dirty = tmp;

	if (first_explicit_dirty == _points.end()) {
		/* nothing is dirty */
		return;
	}

	/* remove all implicit points, because we're going to recalculate them all */

	while (tmp != _points.end()) {
		TempoMapPoints::iterator next = tmp;
		++next;

		if (tmp->is_implicit()) {
			_points.erase (tmp);
		}

		tmp = next;
	}

	/* compute C-by-quarters for all ramped sections, because we need it shortly */

	for (tmp = first_explicit_dirty; tmp != _points.end(); ) {
		TempoMapPoints::iterator nxt = tmp;
		++nxt;

		if (tmp->ramped() && (nxt != _points.end())) {
			tmp->compute_c_quarters (_sample_rate, nxt->metric().superclocks_per_quarter_note (), nxt->quarters() - tmp->quarters());
		}

		tmp = nxt;
	}

	TempoMapPoints::iterator prev = _points.end();

	/* Compute correct quarter-note and superclock times for all music-time locked explicit points */

	for (tmp = first_explicit_dirty; tmp != _points.end(); ) {

		TempoMapPoints::iterator next = tmp;
		++next;

		if (prev != _points.end()) {
			if ((tmp->lock_style() == MusicTime)) {
				/* determine superclock and quarter note time for this (music-time) locked point */

				Temporal::Beats qn = prev->quarters_at (tmp->bbt());
				superclock_t sc = prev->sclock() + prev->metric().superclock_at_qn (qn - prev->quarters());

				if (qn != tmp->quarters() || tmp->sclock() != sc) {
					tmp->set_quarters (qn);
					tmp->set_sclock (sc);
					_points.sort (TempoMapPoint::SuperClockComparator());
					goto restart;
				}
			}
		}

		prev = tmp;
		tmp = next;
	}

	/* _points is guaranteed sorted in superclock and quarter note order. It may not be sorted BBT order because of re-ordering
	 * of music-time locked points.
	 */

	prev = _points.end();

	/* step two: add new implicit points between each pair of explicit
	 * points, after the dirty explicit point
	 */

	bool hit_dirty = false;
	superclock_t first_dirty = 0;

	for (tmp = _points.begin(); tmp != _points.end(); ) {

		if (!hit_dirty) {
			if (!tmp->dirty()) {
				++tmp;
				continue;
			}
			hit_dirty = true;
			first_dirty = tmp->sclock();
		}

		TempoMapPoints::iterator next = tmp;
		++next;


		if (prev != _points.end()) {
			if ((tmp->lock_style() == AudioTime)) {
				/* audio-locked explicit point: recompute it's BBT and quarter-note position since this may have changed */
				tmp->set_quarters (prev->quarters_at (tmp->sclock()));
				if (static_cast<Meter>(tmp->metric()) != static_cast<Meter>(prev->metric())) {
					/* new meter, must be on bar/measure start */
					tmp->set_bbt (prev->metric().round_up_to_bar (prev->bbt_at (tmp->quarters())));
				} else {
					/* no meter change, tempo change required to be on beat */
					tmp->set_bbt (prev->bbt_at (tmp->quarters()).round_up_to_beat());
				}
			}
		}

		superclock_t sc = tmp->sclock();
		Temporal::Beats qn (tmp->quarters ());
		Timecode::BBT_Time bbt (tmp->bbt());
		const bool ramped = tmp->ramped () && next != _points.end();

		/* Temporal::Beats are really quarter notes. This counts how many quarter notes
		   there are between grid points in this section of the tempo map.
		 */
		const Temporal::Beats qn_step = (Temporal::Beats (1) * 4) / tmp->metric().note_value();

		/* compute implicit points as far as the next explicit point, or limit,
		   whichever comes first.
		*/

		const superclock_t sc_limit = (next == _points.end() ? limit : (*next).sclock());

		while (1) {

			/* define next beat in superclocks, beats and bbt */

			qn += qn_step;
			bbt = tmp->metric().bbt_add (bbt, Timecode::BBT_Offset (0, 1, 0));

			if (!ramped) {
				sc += tmp->metric().superclocks_per_note_type();
			} else {
				sc = tmp->sclock() + tmp->metric().superclock_at_qn (qn - tmp->quarters());
			}

			if (sc >= sc_limit) {
				break;
			}

			_points.insert (next, TempoMapPoint (*tmp, sc, qn, bbt));
		}

		tmp->set_dirty (false);
		prev = tmp;
		tmp = next;
	}

	Changed (first_dirty, _points.back().sclock()); /* EMIT SIGNAL */
}

bool
TempoMap::set_tempo_and_meter (Tempo const & tempo, Meter const & meter, superclock_t sc, bool ramp, bool flexible)
{
	/* CALLER MUST HOLD LOCK */

	assert (!_points.empty());

	/* special case: first map entry is later than the new point */

	if (_points.front().sclock() > sc) {
		/* first point is later than sc. There's no iterator to reference a point at or before sc */

		/* determine beats and BBT time for this new tempo point. Note that tempo changes (points) must be deemed to be on beat,
		   even if the user moves them later. Even after moving, the TempoMapPoint that was beat N is still beat N, and is not
		   fractional.
		*/

		Temporal::Beats b = _points.front().quarters_at (sc).round_to_beat();
		Timecode::BBT_Time bbt = _points.front().bbt_at (b).round_to_beat ();

		_points.insert (_points.begin(), TempoMapPoint (TempoMapPoint::ExplicitTempo, tempo, meter, sc, b, bbt, AudioTime, ramp));
		return true;
	}

	/* special case #3: only one map entry, at the same time as the new point.
	   This is the common case when editing tempo/meter in a session with a single tempo/meter
	*/

	if (_points.size() == 1 && _points.front().sclock() == sc) {
		/* change metrics */
		*((Tempo*) &_points.front().metric()) = tempo;
		*((Meter*) &_points.front().metric()) = meter;
		_points.front().make_explicit (TempoMapPoint::Flag (TempoMapPoint::ExplicitTempo|TempoMapPoint::ExplicitMeter));
		return true;
	}

	/* Remember: iterator_at() returns an iterator that references the TempoMapPoint at or BEFORE sc */

	TempoMapPoints::iterator i = iterator_at (sc);
	TempoMapPoints::iterator nxt = i;
	++nxt;

	if (i->sclock() == sc) {
		/* change  metrics */
		*((Tempo*) &i->metric()) = tempo;
		*((Meter*) &i->metric()) = meter;
		i->make_explicit (TempoMapPoint::Flag (TempoMapPoint::ExplicitTempo|TempoMapPoint::ExplicitMeter));
		/* done */
		return true;
	}

	if (!flexible && (sc - i->sclock() < i->metric().superclocks_per_note_type())) {
		cerr << "new tempo too close to previous ...\n";
		return false;
	}

	TempoMapPoints::iterator e (i);
	while (!e->is_explicit() && e != _points.begin()) {
		--e;
	}

	if (e->metric().ramped()) {
		/* need to adjust ramp constants for preceding explict point, since the new point will be positioned right after it
		   and thus defines the new ramp distance.
		*/
		e->compute_c_superclock (_sample_rate, tempo.superclocks_per_quarter_note (), sc);
	}

	/* determine beats and BBT time for this new tempo point. Note that tempo changes (points) must be deemed to be on beat,
	   even if the user moves them later. Even after moving, the TempoMapPoint that was beat N is still beat N, and is not
	   fractional.
	 */

	Temporal::Beats qn = i->quarters_at (sc).round_to_beat();

	/* rule: all Tempo changes must be on-beat. So determine the nearest later beat to "sc"
	 */

	Timecode::BBT_Time bbt = i->bbt_at (qn).round_up_to_beat ();

	/* Modify the iterator to reference the point AFTER this new one, because STL insert is always "insert-before"
	 */

	if (i != _points.end()) {
		++i;
	}

	_points.insert (i, TempoMapPoint (TempoMapPoint::ExplicitTempo, tempo, meter, sc, qn, bbt, AudioTime, ramp));
	return true;
}

bool
TempoMap::set_tempo (Tempo const & t, superclock_t sc, bool ramp)
{
	Glib::Threads::RWLock::WriterLock lm (_lock);

	assert (!_points.empty());

	/* special case: first map entry is later than the new point */

	if (_points.front().sclock() > sc) {
		/* first point is later than sc. There's no iterator to reference a point at or before sc */

		/* determine beats and BBT time for this new tempo point. Note that tempo changes (points) must be deemed to be on beat,
		   even if the user moves them later. Even after moving, the TempoMapPoint that was beat N is still beat N, and is not
		   fractional.
		*/

		Temporal::Beats b = _points.front().quarters_at (sc).round_to_beat();
		Timecode::BBT_Time bbt = _points.front().bbt_at (b).round_to_beat ();

		_points.insert (_points.begin(), TempoMapPoint (TempoMapPoint::ExplicitTempo, t, _points.front().metric(), sc, b, bbt, AudioTime, ramp));
		return true;
	}

	/* special case #3: only one map entry, at the same time as the new point.
	   This is the common case when editing tempo/meter in a session with a single tempo/meter
	*/

	if (_points.size() == 1 && _points.front().sclock() == sc) {
		/* change tempo */
		*((Tempo*) &_points.front().metric()) = t;
		_points.front().make_explicit (TempoMapPoint::ExplicitTempo);
		return true;
	}

	/* Remember: iterator_at() returns an iterator that references the TempoMapPoint at or BEFORE sc */

	TempoMapPoints::iterator i = iterator_at (sc);
	TempoMapPoints::iterator nxt = i;
	++nxt;

	if (i->sclock() == sc) {
		/* change tempo */
		*((Tempo*) &i->metric()) = t;
		i->make_explicit (TempoMapPoint::ExplicitTempo);
		/* done */
		return true;
	}

	if (sc - i->sclock() < i->metric().superclocks_per_note_type()) {
		cerr << "new tempo too close to previous ...\n";
		return false;
	}

	Meter const & meter (i->metric());

	TempoMapPoints::iterator e (i);
	while (!e->is_explicit() && e != _points.begin()) {
		--e;
	}

	if (e->metric().ramped()) {
		/* need to adjust ramp constants for preceding explict point, since the new point will be positioned right after it
		   and thus defines the new ramp distance.
		*/
		e->compute_c_superclock (_sample_rate, t.superclocks_per_quarter_note (), sc);
	}

	/* determine beats and BBT time for this new tempo point. Note that tempo changes (points) must be deemed to be on beat,
	   even if the user moves them later. Even after moving, the TempoMapPoint that was beat N is still beat N, and is not
	   fractional.
	 */

	Temporal::Beats qn = i->quarters_at (sc).round_to_beat();

	/* rule: all Tempo changes must be on-beat. So determine the nearest later beat to "sc"
	 */

	Timecode::BBT_Time bbt = i->bbt_at (qn).round_up_to_beat ();

	/* Modify the iterator to reference the point AFTER this new one, because STL insert is always "insert-before"
	 */

	if (i != _points.end()) {
		++i;
	}
	_points.insert (i, TempoMapPoint (TempoMapPoint::ExplicitTempo, t, meter, sc, qn, bbt, AudioTime, ramp));
	return true;
}

bool
TempoMap::set_tempo (Tempo const & t, Timecode::BBT_Time const & bbt, bool ramp)
{
	Glib::Threads::RWLock::WriterLock lm (_lock);

	/* tempo changes are required to be on-beat */

	Timecode::BBT_Time on_beat = bbt.round_up_to_beat();

	assert (!_points.empty());

	if (_points.front().bbt() > on_beat) {
		cerr << "Cannot insert tempo at " << bbt << " before first point at " << _points.front().bbt() << endl;
		return false;
	}

	if (_points.size() == 1 && _points.front().bbt() == on_beat) {
		/* change Meter */
		*((Tempo*) &_points.front().metric()) = t;
		_points.front().make_explicit (TempoMapPoint::ExplicitTempo);
		return true;
	}

	TempoMapPoints::iterator i = iterator_at (on_beat);

	if (i->bbt() == on_beat) {
		*((Tempo*) &i->metric()) = t;
		i->make_explicit (TempoMapPoint::ExplicitTempo);
		return true;
	}

	Meter const & meter (i->metric());
	++i;

	/* stick a prototype music-locked point up front and let ::rebuild figure out the superclock and quarter time */
	_points.insert (i, TempoMapPoint (TempoMapPoint::ExplicitTempo, t, meter, 0, Temporal::Beats(), on_beat, MusicTime, ramp));
	return true;
}

bool
TempoMap::set_meter (Meter const & m, Timecode::BBT_Time const & bbt)
{
	Glib::Threads::RWLock::WriterLock lm (_lock);
	Timecode::BBT_Time measure_start (m.round_up_to_bar (bbt));

	assert (!_points.empty());

	if (_points.front().bbt() > measure_start) {
		cerr << "Cannot insert meter at " << bbt << " before first point at " << _points.front().bbt() << endl;
		return false;
	}

	if (_points.size() == 1 && _points.front().bbt() == measure_start) {
		/* change Meter */
		*((Meter*) &_points.front().metric()) = m;
		_points.front().make_explicit (TempoMapPoint::ExplicitMeter);
		return true;
	}

	TempoMapPoints::iterator i = iterator_at (measure_start);

	if (i->bbt() == measure_start) {
		*((Meter*) &i->metric()) = m;
		i->make_explicit (TempoMapPoint::ExplicitMeter);
		return true;
	}

	Temporal::Beats qn = i->quarters_at (measure_start);
	superclock_t sc = i->sclock() + i->metric().superclock_at_qn (qn);

	Tempo const & tempo (i->metric());
	++i;

	_points.insert (i, TempoMapPoint (TempoMapPoint::ExplicitMeter, tempo, m, sc, qn, measure_start, MusicTime));
	return true;
}

bool
TempoMap::set_meter (Meter const & m, superclock_t sc)
{
	Glib::Threads::RWLock::WriterLock lm (_lock);
	assert (!_points.empty());

	/* special case #2: first map entry is later than the new point */

	if (_points.front().sclock() > sc) {
		/* determine quarters and BBT time for this new tempo point. Note that tempo changes (points) must be deemed to be on beat,
		   even if the user moves them later. Even after moving, the TempoMapPoint that was beat N is still beat N, and is not
		   fractional.
		*/

		Temporal::Beats b = _points.front().quarters_at (sc).round_to_beat();
		Timecode::BBT_Time bbt = _points.front().bbt_at (b).round_to_beat ();

		_points.insert (_points.begin(), TempoMapPoint (TempoMapPoint::ExplicitMeter, _points.front().metric(), m, sc, b, bbt, AudioTime));
		return true;
	}

	/* special case #3: only one map entry, at the same time as the new point.

	   This is the common case when editing tempo/meter in a session with a single tempo/meter
	*/

	if (_points.size() == 1 && _points.front().sclock() == sc) {
		/* change meter */
		*((Meter*) &_points.front().metric()) = m;
		_points.front().make_explicit (TempoMapPoint::ExplicitMeter);
		return true;
	}

	TempoMapPoints::iterator i = iterator_at (sc);

	if (i->sclock() == sc) {
		/* change meter */
		*((Meter*) &i->metric()) = m;

		/* enforce rule described below regarding meter change positions */

		if (i->bbt().beats != 1) {
			i->set_bbt (Timecode::BBT_Time (i->bbt().bars + 1, 1, 0));
		}

		i->make_explicit (TempoMapPoint::ExplicitMeter);

		return true;
	}

	if (sc - i->sclock() < i->metric().superclocks_per_note_type()) {
		cerr << "new tempo too close to previous ...\n";
		return false;
	}

	/* determine quarters and BBT time for this new tempo point. Note that tempo changes (points) must be deemed to be on beat,
	   even if the user moves them later. Even after moving, the TempoMapPoint that was beat N is still beat N, and is not
	   fractional.
	*/

	Temporal::Beats b = i->quarters_at (sc).round_to_beat();

	/* rule: all Meter changes must start a new measure. So determine the nearest, lower beat to "sc". If this is not
	   the first division of the measure, move to the next measure.
	 */

	Timecode::BBT_Time bbt = i->bbt_at (b).round_down_to_beat ();

	if (bbt.beats != 1) {
		bbt.bars += 1;
		bbt.beats = 1;
		bbt.ticks = 0;
	}

	Tempo const & tempo (i->metric());
	++i;

	_points.insert (i, TempoMapPoint (TempoMapPoint::ExplicitMeter, tempo, m, sc, b, bbt, AudioTime));
	return true;
}

TempoMapPoints::iterator
TempoMap::iterator_at (superclock_t sc)
{
	/* CALLER MUST HOLD LOCK */

	if (_points.empty()) {
		throw EmptyTempoMapException();
	}

	if (_points.size() == 1) {
		return _points.begin();
	}

	/* Construct an arbitrary TempoMapPoint. The only property we care about is it's superclock time,
	   so other values used in the constructor are arbitrary and irrelevant.
	*/

	TempoMetric const & metric (_points.front().metric());
	const TempoMapPoint tp (TempoMapPoint::Flag (0), metric, metric, sc, Temporal::Beats(), Timecode::BBT_Time(), AudioTime);
	TempoMapPoint::SuperClockComparator scmp;

	TempoMapPoints::iterator tmp = upper_bound (_points.begin(), _points.end(), tp, scmp);

	if (tmp != _points.begin()) {
		return --tmp;
	}

	return tmp;
}

TempoMapPoints::iterator
TempoMap::iterator_at (Temporal::Beats const & qn)
{
	/* CALLER MUST HOLD LOCK */

	if (_points.empty()) {
		throw EmptyTempoMapException();
	}

	if (_points.size() == 1) {
		return _points.begin();
	}

	/* Construct an arbitrary TempoMapPoint. The only property we care about is its quarters time,
	   so other values used in the constructor are arbitrary and irrelevant.
	*/

	TempoMetric const & metric (_points.front().metric());
	const TempoMapPoint tp (TempoMapPoint::Flag (0), metric, metric, 0, qn, Timecode::BBT_Time(), AudioTime);
	TempoMapPoint::QuarterComparator bcmp;

	TempoMapPoints::iterator tmp = upper_bound (_points.begin(), _points.end(), tp, bcmp);

	if (tmp != _points.begin()) {
		return --tmp;
	}

	return tmp;
}

TempoMapPoints::iterator
TempoMap::iterator_at (Timecode::BBT_Time const & bbt)
{
	/* CALLER MUST HOLD LOCK */

	if (_points.empty()) {
		throw EmptyTempoMapException();
	}

	if (_points.size() == 1) {
		return _points.begin();
	}

	/* Construct an arbitrary TempoMapPoint. The only property we care about is its bbt time,
	   so other values used in the constructor are arbitrary and irrelevant.
	*/

	TempoMetric const & metric (_points.front().metric());
	const TempoMapPoint tp (TempoMapPoint::Flag(0), metric, metric, 0, Temporal::Beats(), bbt, MusicTime);
	TempoMapPoint::BBTComparator bcmp;

	TempoMapPoints::iterator tmp = upper_bound (_points.begin(), _points.end(), tp, bcmp);

	if (tmp != _points.begin()) {
		return --tmp;
	}

	return tmp;
}

Timecode::BBT_Time
TempoMap::bbt_at (superclock_t sc) const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	return bbt_at_locked (sc);
}

Timecode::BBT_Time
TempoMap::bbt_at_locked (superclock_t sc) const
{
	TempoMapPoint point (const_point_at (sc));
	Temporal::Beats b ((sc - point.sclock()) / (double) point.metric().superclocks_per_quarter_note());
	return point.metric().bbt_add (point.bbt(), Timecode::BBT_Offset (0, b.get_beats(), b.get_ticks()));
}

Timecode::BBT_Time
TempoMap::bbt_at (Temporal::Beats const & qn) const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	return bbt_at_locked (qn);
}

Timecode::BBT_Time
TempoMap::bbt_at_locked (Temporal::Beats const & qn) const
{
	//Glib::Threads::RWLock::ReaderLock lm (_lock);
	TempoMapPoint const & point (const_point_at (qn));
	Temporal::Beats delta (qn - point.quarters());
	return point.metric().bbt_add (point.bbt(), Timecode::BBT_Offset (0, delta.get_beats(), delta.get_ticks()));
}

superclock_t
TempoMap::superclock_at (Temporal::Beats const & qn) const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	return superclock_at_locked (qn);
}

superclock_t
TempoMap::superclock_at_locked (Temporal::Beats const & qn) const
{
	assert (!_points.empty());

	/* only the quarters property matters, since we're just using it to compare */
	const TempoMapPoint tmp (TempoMapPoint::Flag (0), Tempo (120), Meter (4, 4), 0, qn, Timecode::BBT_Time(), MusicTime);
	TempoMapPoint::QuarterComparator bcmp;

	TempoMapPoints::const_iterator i = upper_bound (_points.begin(), _points.end(), tmp, bcmp);

	if (i == _points.end()) {
		--i;
	}

	/* compute distance from reference point to b. Remember that Temporal::Beats is always interpreted as quarter notes */

	const Temporal::Beats q_delta = qn - i->quarters();
	superclock_t sclock_delta = i->metric().superclock_at_qn (q_delta);
	return i->sclock() + sclock_delta;
}

superclock_t
TempoMap::superclock_at (Timecode::BBT_Time const & bbt) const
{
	//Glib::Threads::RWLock::ReaderLock lm (_lock);
	return superclock_at_locked (bbt);
}

superclock_t
TempoMap::superclock_at_locked (Timecode::BBT_Time const & bbt) const
{
	//Glib::Threads::RWLock::ReaderLock lm (_lock);
	assert (!_points.empty());

	/* only the bbt property matters, since we're just using it to compare */
	const TempoMapPoint tmp (TempoMapPoint::Flag (0), Tempo (120), Meter (4, 4), 0, Temporal::Beats(), bbt, MusicTime);
	TempoMapPoint::BBTComparator bcmp;

	TempoMapPoints::const_iterator i = upper_bound (_points.begin(), _points.end(), tmp, bcmp);

	if (i == _points.end()) {
		--i;
	}

	/* this computes the distance from the point, in beats whose size is
	   determined by the meter.
	*/

	const Timecode::BBT_Offset delta = i->metric().bbt_delta (bbt, i->bbt());

	/* convert to quarter notes */
	const int32_t ticks = delta.ticks + (Temporal::Beats::PPQN * delta.beats * 4) / i->metric().note_value();

	/* get distance in superclocks */
	return i->sclock() + i->metric().superclock_at_qn (Temporal::Beats::ticks (ticks));
}

void
TempoMap::set_sample_rate (samplecnt_t new_sr)
{
	//Glib::Threads::RWLock::ReaderLock lm (_lock);
	double ratio = new_sr / (double) _sample_rate;

	for (TempoMapPoints::iterator i = _points.begin(); i != _points.end(); ++i) {
		i->map_reset_set_sclock_for_sr_change (llrint (ratio * i->sclock()));
	}
}

void
TempoMap::dump (std::ostream& ostr)
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	dump_locked (ostr);
}

void
TempoMap::dump_locked (std::ostream& ostr)
{
	ostr << "\n\n------------\n";
	for (TempoMapPoints::iterator i = _points.begin(); i != _points.end(); ++i) {
		ostr << *i << std::endl;
	}
}

void
TempoMap::remove_explicit_point (superclock_t sc)
{
	Glib::Threads::RWLock::WriterLock lm (_lock);
	TempoMapPoints::iterator p = iterator_at (sc);

	if (p->sclock() == sc) {
		_points.erase (p);
	}
}

bool
TempoMap::move_to (superclock_t current, superclock_t destination, bool push)
{
	Glib::Threads::RWLock::WriterLock lm (_lock);
	TempoMapPoints::iterator p = iterator_at (current);

	if (p->sclock() != current) {
		cerr << "No point @ " << current << endl;
		return false;
	}

	/* put a "dirty" flag in at the nearest explicit point to the removal point
	 */

	if (p != _points.begin()) {
		TempoMapPoints::iterator prev (p);
		--prev;
		while (prev != _points.begin() && !prev->is_explicit()) {
			--prev;
		}
		prev->set_dirty (true);
	} else {
		TempoMapPoints::iterator nxt (p);
		++nxt;
		while (nxt != _points.end() && !nxt->is_explicit()) {
			++nxt;
		}
		if (nxt != _points.end()) {
			nxt->set_dirty (true);
		}
	}

	if (!set_tempo_and_meter (p->metric(), p->metric(), destination, p->ramped(), true)) {
		return false;
	}

	if (push) {

		p = iterator_at (destination);
		++p;

		const superclock_t delta = destination - current;

		while (p != _points.end()) {
			p->set_sclock (p->sclock() + delta);
			++p;
		}
	}

	_points.erase (p);

	return true;
}

void
TempoMap::get_grid (TempoMapPoints& ret, superclock_t start, superclock_t end, Temporal::Beats const & resolution)
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	TempoMapPoints::iterator p = iterator_at (start);

	while (p != _points.end() && p->sclock() < start) {
		++p;
	}

	if (resolution == Temporal::Beats()) {
		/* just hand over the points as-is */
		while (p != _points.end() && p->sclock() < end) {
			ret.push_back (*p);
			++p;
		}
		return;
	}

	superclock_t pos = p->sclock();
	Temporal::Beats qpos;
	TempoMapPoints::iterator nxt = p;
	++nxt;

	while ((p != _points.end()) && (pos < end)) {
		/* recompute grid down to @param resolution level
		 */

		superclock_t sclock_delta = p->metric().superclock_at_qn (qpos);

		ret.push_back (TempoMapPoint (TempoMapPoint::Flag (TempoMapPoint::ExplicitMeter|TempoMapPoint::ExplicitTempo),
		                              p->metric(), p->metric(),
		                              p->sclock() + sclock_delta,
		                              p->quarters() + qpos,
		                              p->metric().bbt_add (p->bbt(), Timecode::BBT_Offset (0, qpos.get_beats(), qpos.get_ticks())),
		                              AudioTime,
		                              p->ramped()));

		qpos += resolution;
		pos += sclock_delta;

		if (pos >= nxt->sclock()) {
			p = nxt;
			++nxt;
		}
	}
}


void
TempoMap::get_bar_grid (TempoMapPoints& ret, superclock_t start, superclock_t end, int32_t bar_gap)
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);

	for (TempoMapPoints::iterator p = iterator_at (start); (p != _points.end()) && (p->sclock() < end); ++p) {

		if ((p->sclock() >= start) && (p->bbt().beats == 1) && ((p->bbt().bars == 1) || (p->bbt().bars % bar_gap == 0))) {
			ret.push_back (TempoMapPoint (TempoMapPoint::Flag (TempoMapPoint::ExplicitMeter|TempoMapPoint::ExplicitTempo),
			                              p->metric(), p->metric(),
			                              p->sclock(),
			                              p->quarters(),
			                              p->bbt(),
			                              AudioTime,
			                              p->ramped()));
		}
	}
}

std::ostream&
operator<<(std::ostream& str, Meter const & m)
{
	return str << m.divisions_per_bar() << '/' << m.note_value();
}

std::ostream&
operator<<(std::ostream& str, Tempo const & t)
{
	return str << t.note_types_per_minute() << " 1/" << t.note_type() << " notes per minute (" << t.superclocks_per_note_type() << " sc-per-1/" << t.note_type() << ')';
}

std::ostream&
operator<<(std::ostream& str, TempoMapPoint const & tmp)
{
	str << '@' << std::setw (12) << tmp.sclock() << ' ' << tmp.sclock() / (double) superclock_ticks_per_second
	    << (tmp.is_explicit() ? " EXP" : " imp")
	    << " qn " << tmp.quarters ()
	    << " bbt " << tmp.bbt()
	    << " lock to " << tmp.lock_style()
		;

	if (tmp.is_explicit()) {
		str << " tempo " << *((Tempo*) &tmp.metric())
		    << " meter " << *((Meter*) &tmp.metric())
			;
	}

	if (tmp.is_explicit() && tmp.ramped()) {
		str << " ramp c/sc = " << tmp.metric().c_per_superclock() << " c/qn " << tmp.metric().c_per_quarter();
	}
	return str;
}

/*******/

#define SAMPLERATE 48000
#define SECONDS_TO_SUPERCLOCK(s) (superclock_ticks_per_second * s)

using std::cerr;
using std::cout;
using std::endl;

void
test_bbt_math ()
{
	using namespace Timecode;

	BBT_Time a;
	BBT_Time b1 (1,1,1919);
	BBT_Time n1 (-1,1,1919);
	std::vector<Meter> meters;

	meters.push_back (Meter (4, 4));
	meters.push_back (Meter (5, 8));
	meters.push_back (Meter (11, 7));
	meters.push_back (Meter (3, 4));

#define PRINT_RESULT(m,str,op,op1,Bars,Beats,Ticks) cout << m << ' ' << (op1) << ' ' << str << ' ' << BBT_Offset ((Bars),(Beats),(Ticks)) << " = " << m.op ((op1), BBT_Offset ((Bars), (Beats), (Ticks))) << endl;

	for (std::vector<Meter>::iterator m = meters.begin(); m != meters.end(); ++m) {
		for (int B = 1; B < 4; ++B) {
			for (int b = 1; b < 13; ++b) {
				PRINT_RESULT((*m), "+", bbt_add, a, B, b, 0);
				PRINT_RESULT((*m), "+", bbt_add, a, B, b, 1);
				PRINT_RESULT((*m), "+", bbt_add, a, B, b, Temporal::Beats::PPQN/2);
				PRINT_RESULT((*m), "+", bbt_add, a, B, b, Temporal::Beats::PPQN);
				PRINT_RESULT((*m), "+", bbt_add, a, B, b, Temporal::Beats::PPQN - 1);
				PRINT_RESULT((*m), "+", bbt_add, a, B, b, Temporal::Beats::PPQN - 2);

				PRINT_RESULT((*m), "+", bbt_add, b1, B, b, 0);
				PRINT_RESULT((*m), "+", bbt_add, b1, B, b, 1);
				PRINT_RESULT((*m), "+", bbt_add, b1, B, b, Temporal::Beats::PPQN/2);
				PRINT_RESULT((*m), "+", bbt_add, b1, B, b, Temporal::Beats::PPQN);
				PRINT_RESULT((*m), "+", bbt_add, b1, B, b, Temporal::Beats::PPQN - 1);
				PRINT_RESULT((*m), "+", bbt_add, b1, B, b, Temporal::Beats::PPQN - 2);

				PRINT_RESULT((*m), "+", bbt_add, n1, B, b, 0);
				PRINT_RESULT((*m), "+", bbt_add, n1, B, b, 1);
				PRINT_RESULT((*m), "+", bbt_add, n1, B, b, Temporal::Beats::PPQN/2);
				PRINT_RESULT((*m), "+", bbt_add, n1, B, b, Temporal::Beats::PPQN);
				PRINT_RESULT((*m), "+", bbt_add, n1, B, b, Temporal::Beats::PPQN - 1);
				PRINT_RESULT((*m), "+", bbt_add, n1, B, b, Temporal::Beats::PPQN - 2);
			}
		}

		for (int B = 1; B < 4; ++B) {
			for (int b = 1; b < 13; ++b) {
				PRINT_RESULT ((*m), "-", bbt_subtract, a, B, b, 0);
				PRINT_RESULT ((*m), "-", bbt_subtract, a, B, b, 1);
				PRINT_RESULT ((*m), "-", bbt_subtract, a, B, b, Temporal::Beats::PPQN/2);
				PRINT_RESULT ((*m), "-", bbt_subtract, a, B, b, Temporal::Beats::PPQN);
				PRINT_RESULT ((*m), "-", bbt_subtract, a, B, b, Temporal::Beats::PPQN - 1);
				PRINT_RESULT ((*m), "-", bbt_subtract, a, B, b, Temporal::Beats::PPQN - 2);
			}
		}
	}
}

int
main ()
{
	TempoMap tmap (Tempo (140), Meter (4,4), SAMPLERATE);

	//test_bbt_math ();
	//return 0;

	tmap.set_tempo (Tempo (7), SECONDS_TO_SUPERCLOCK(7));
	tmap.set_tempo (Tempo (23), SECONDS_TO_SUPERCLOCK(23));
	tmap.set_tempo (Tempo (24), SECONDS_TO_SUPERCLOCK(24), true);
	tmap.set_tempo (Tempo (40), SECONDS_TO_SUPERCLOCK(28), true);
	tmap.set_tempo (Tempo (100), SECONDS_TO_SUPERCLOCK(100));
	tmap.set_tempo (Tempo (123), SECONDS_TO_SUPERCLOCK(23));

	tmap.set_meter (Meter (3, 4), SECONDS_TO_SUPERCLOCK(23));
	tmap.set_meter (Meter (5, 8), SECONDS_TO_SUPERCLOCK(100));
	tmap.set_meter (Meter (5, 7), SECONDS_TO_SUPERCLOCK(7));
	tmap.set_meter (Meter (4, 4), SECONDS_TO_SUPERCLOCK(24));
	tmap.set_meter (Meter (11, 7), SECONDS_TO_SUPERCLOCK(23));

	tmap.set_meter (Meter (3, 8), Timecode::BBT_Time (17, 1, 0));

	tmap.rebuild (SECONDS_TO_SUPERCLOCK (120));
	tmap.dump (std::cout);

	if (tmap.move_to (SECONDS_TO_SUPERCLOCK(23), SECONDS_TO_SUPERCLOCK (72))) {
		tmap.rebuild (SECONDS_TO_SUPERCLOCK (120));
		tmap.dump (std::cout);
	}

	TempoMapPoints grid;
	tmap.get_grid (grid, SECONDS_TO_SUPERCLOCK (12.3), SECONDS_TO_SUPERCLOCK (44), Temporal::Beats::ticks ((Temporal::Beats::PPQN * 4) / 12));
	cout << "grid contains " << grid.size() << endl;
	for (TempoMapPoints::iterator p = grid.begin(); p != grid.end(); ++p) {
		cout << *p << endl;
	}

	TempoMapPoints bbt_grid;
	tmap.get_bar_grid (bbt_grid, SECONDS_TO_SUPERCLOCK (0), SECONDS_TO_SUPERCLOCK (100), 4);
	cout << "bbt_grid contains " << bbt_grid.size() << endl;
	for (TempoMapPoints::iterator p = bbt_grid.begin(); p != bbt_grid.end(); ++p) {
		cout << *p << endl;
	}

	return 0;
}
