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

#include <algorithm>

#include "pbd/error.h"
#include "pbd/i18n.h"
#include "pbd/compose.h"
#include "pbd/enumwriter.h"
#include "pbd/failed_constructor.h"
#include "pbd/stacktrace.h"

#include "temporal/debug.h"
#include "temporal/tempo.h"

using namespace PBD;
using namespace Temporal;
using std::cerr;
using std::cout;
using std::endl;
using Temporal::superclock_t;

std::string Tempo::xml_node_name = X_("Tempo");
std::string Meter::xml_node_name = X_("Meter");

superclock_t Temporal::superclock_ticks_per_second = 508032000; // 2^10 * 3^4 * 5^3 * 7^2

SerializedRCUManager<TempoMap> TempoMap::_map_mgr (0);
thread_local TempoMap::SharedPtr TempoMap::_tempo_map_p;

void
Point::add_state (XMLNode & node) const
{
	node.set_property (X_("sclock"), _sclock);
	node.set_property (X_("quarters"), _quarters);
	node.set_property (X_("bbt"), _bbt);
}

Point::Point (TempoMap const & map, XMLNode const & node)
	: _map (&map)
{
	if (!node.get_property (X_("sclock"), _sclock)) {
		throw failed_constructor();
	}
	if (!node.get_property (X_("quarters"), _quarters)) {
		throw failed_constructor();
	}
	if (!node.get_property (X_("bbt"), _bbt)) {
		throw failed_constructor();
	}
}

#if 0
samplepos_t
Point::sample() const
{
	return superclock_to_samples (_sclock, _map->sample_rate());
}
#endif

timepos_t
Point::time() const
{
	switch (_map->time_domain()) {
	case AudioTime:
		return timepos_t::from_superclock (sclock());
	case BeatTime:
		return timepos_t (beats());
	case BarTime:
		/*NOTREACHED*/
		break;
	}
	/*NOTREACHED*/
	abort();
	/*NOTREACHED*/
	return timepos_t (AudioTime);
}

Tempo::Tempo (XMLNode const & node)
{
	assert (node.name() == xml_node_name);
	if (!node.get_property (X_("scpnt-start"), _superclocks_per_note_type)) {
		throw failed_constructor ();
	}
	if (!node.get_property (X_("scpnt-end"), _end_superclocks_per_note_type)) {
		throw failed_constructor ();
	}
	if (!node.get_property (X_("note-type"), _note_type)) {
		throw failed_constructor ();
	}
	if (!node.get_property (X_("type"), _type)) {
		throw failed_constructor ();
	}
	if (!node.get_property (X_("active"), _active)) {
		throw failed_constructor ();
	}
}

bool
Tempo::set_ramped (bool yn)
{
	_type = (yn ? Ramped : Constant);
	return true;
}

bool
Tempo::set_clamped (bool)
{
#warning implement Tempo::set_clamped
	return true;
}

XMLNode&
Tempo::get_state () const
{
	XMLNode* node = new XMLNode (xml_node_name);

	node->set_property (X_("scpnt-start"), superclocks_per_note_type());
	node->set_property (X_("scpnt-end"), end_superclocks_per_note_type());
	node->set_property (X_("note-type"), note_type());
	node->set_property (X_("type"), type());
	node->set_property (X_("active"), active());

	return *node;
}

int
Tempo::set_state (XMLNode const & node, int /*version*/)
{
	if (node.name() != xml_node_name) {
		return -1;
	}

	node.get_property (X_("scpnt-start"), _superclocks_per_note_type);
	node.get_property (X_("scpnt-end"), _end_superclocks_per_note_type);
	node.get_property (X_("note-type"), _note_type);
	node.get_property (X_("type"), _type);
	node.get_property (X_("active"), _active);

	return 0;
}

Meter::Meter (XMLNode const & node)
{
	assert (node.name() == xml_node_name);
	if (!node.get_property (X_("note-value"), _note_value)) {
		throw failed_constructor ();
	}
	if (!node.get_property (X_("divisions-per-bar"), _divisions_per_bar)) {
		throw failed_constructor ();
	}
}

XMLNode&
Meter::get_state () const
{
	XMLNode* node = new XMLNode (xml_node_name);
	node->set_property (X_("note-value"), note_value());
	node->set_property (X_("divisions-per-bar"), divisions_per_bar());
	return *node;
}

int
Meter::set_state (XMLNode const & node, int /* version */)
{
	if (node.name() != xml_node_name) {
		return -1;
	}

	node.get_property (X_("note-value"), _note_value);
	node.get_property (X_("divisions-per-bar"), _divisions_per_bar);

	return 0;
}

Temporal::BBT_Time
Meter::bbt_add (Temporal::BBT_Time const & bbt, Temporal::BBT_Offset const & add) const
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

	Temporal::BBT_Offset r (bars + add.bars, beats + add.beats, ticks + add.ticks);

	/* ticks-per-bar-division; PPQN is ticks-per-quarter note */

	const int32_t tpg = ticks_per_grid ();

	if (r.ticks >= tpg) {

		/* ticks per bar */
		const int32_t tpb = tpg * _divisions_per_bar;

		if (r.ticks >= tpb) {
			r.bars += r.ticks / tpb;
			r.ticks %= tpb;
		}

		if (r.ticks >= tpg) {
			r.beats += r.ticks / tpg;
			r.ticks %= tpg;
		}
	}

	if (r.beats > _divisions_per_bar) {

		/* adjust to zero-based math, since that's what C++ operators expect */

		r.beats -= 1;
		r.bars += r.beats / _divisions_per_bar;
		r.beats %= _divisions_per_bar;

		/* adjust back */

		r.beats += 1;
	}

	if (r.bars == 0) {
		r.bars = 1;
	}

	return Temporal::BBT_Time (r.bars, r.beats, r.ticks);
}

Temporal::BBT_Time
Meter::bbt_subtract (Temporal::BBT_Time const & bbt, Temporal::BBT_Offset const & sub) const
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

	Temporal::BBT_Offset r (bars - sub.bars, beats - sub.beats, ticks - sub.ticks);

	/* ticks-per-bar-division; PPQN is ticks-per-quarter note */

	const int32_t tpg = ticks_per_grid ();

	if (r.ticks < 0) {
		r.beats -= (r.ticks / tpg);
		r.ticks = tpg + (r.ticks % Temporal::Beats::PPQN);
	}

	if (r.beats < 0) {

		r.beats += 1;

		r.bars -= r.beats / _divisions_per_bar;
		r.beats = r.beats % _divisions_per_bar;

		r.beats -= 1;
	}

	if (r.bars <= 0) {
		r.bars -= 1;
	}

	return Temporal::BBT_Time (r.bars, r.beats, r.ticks);
}

Temporal::BBT_Time
Meter::round_to_bar (Temporal::BBT_Time const & bbt) const
{
	Temporal::BBT_Time b = bbt.round_to_beat ();
	if (b.beats > _divisions_per_bar/2) {
		b.bars++;
	}
	b.beats = 1;
	return b;
}

Temporal::BBT_Time
Meter::round_up_to_bar (Temporal::BBT_Time const & bbt) const
{
	if (bbt.ticks == 0 && bbt.beats == 1) {
		return bbt;
	}
	BBT_Time b = bbt.round_up_to_beat ();
	if (b.beats > 1) {
		b.bars += 1;
		b.beats = 1;
	}
	return b;
}

Temporal::BBT_Time
Meter::round_down_to_bar (Temporal::BBT_Time const & bbt) const
{
	if (bbt.ticks == 0 && bbt.beats == 1) {
		return bbt;
	}
	BBT_Time b = bbt.round_down_to_beat ();
	if (b.beats > 1) {
		b.beats = 1;
	}
	return b;
}

Temporal::BBT_Time
Meter::round_up_to_beat (Temporal::BBT_Time const & bbt) const
{
	Temporal::BBT_Time b = bbt.round_up_to_beat ();
	if (b.beats > _divisions_per_bar) {
		b.bars++;
		b.beats = 1;
	}
	return b;
}

Temporal::Beats
Meter::to_quarters (Temporal::BBT_Offset const & offset) const
{
	int64_t ticks = 0;

	ticks += (Beats::PPQN * offset.bars * _divisions_per_bar * 4) / _note_value;
	ticks += (Beats::PPQN * offset.beats * 4) / _note_value;

	/* "parts per bar division" */

	const int tpg = ticks_per_grid ();

	if (offset.ticks > tpg) {
		ticks += Beats::PPQN * offset.ticks / tpg;
		ticks += offset.ticks % tpg;
	} else {
		ticks += offset.ticks;
	}

	return Beats (ticks/Beats::PPQN, ticks%Beats::PPQN);
}

int
TempoPoint::set_state (XMLNode const & node, int version)
{
	int ret;

	if ((ret = Tempo::set_state (node, version)) == 0) {
		node.get_property (X_("omega"), _omega);
	}

	return ret;
}

XMLNode&
TempoPoint::get_state () const
{
	XMLNode& base (Tempo::get_state());
	Point::add_state (base);
	base.set_property (X_("omega"), _omega);
	return base;
}

TempoPoint::TempoPoint (TempoMap const & map, XMLNode const & node)
	: Tempo (node)
	, Point (map, node)
	, _omega (0)
{
}

/* To understand the math(s) behind ramping, see the file doc/tempo.{pdf,tex}
 */

void
TempoPoint::compute_omega (samplecnt_t sr, superclock_t end_scpqn, Temporal::Beats const & quarter_duration)
{
	if ((superclocks_per_quarter_note () == end_scpqn) || (_type == Constant)) {
		_omega = 0.0;
		return;
	}

	_omega = ((1.0/end_scpqn) - (1.0/superclocks_per_quarter_note())) / quarter_duration.to_double();

	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("computed omega = %1%2 dur was %3\n", std::setprecision(12),_omega, quarter_duration.to_double()));
}

superclock_t
TempoPoint::superclock_at (Temporal::Beats const & qn) const
{
	if (qn == _quarters) {
		return _sclock;
	}

	if (!actually_ramped()) {
		/* not ramped, use linear */
		const superclock_t spqn = superclocks_per_quarter_note ();
		return (spqn * qn.get_beats()) + int_div_round ((spqn * qn.get_ticks()), superclock_t (Temporal::ticks_per_beat));
	}

	return _sclock + llrint (log1p (superclocks_per_quarter_note() * _omega * (qn - _quarters).to_double()) / _omega);
}

superclock_t
TempoPoint::superclocks_per_note_type_at (timepos_t const &pos) const
{
	if (!actually_ramped()) {
		return _superclocks_per_note_type;
	}

	return _superclocks_per_note_type * exp (-_omega * pos.superclocks());
}

Temporal::Beats
TempoPoint::quarters_at (superclock_t sc) const
{
	if (!actually_ramped()) {
		/* convert sc into superbeats, given that sc represents some number of seconds */
		const superclock_t whole_seconds = sc / superclock_ticks_per_second;
		const superclock_t remainder = sc - (whole_seconds * superclock_ticks_per_second);
		const superclock_t superbeats = ((_super_note_type_per_second/4) * whole_seconds) + int_div_round (superclock_t ((_super_note_type_per_second/4) * remainder), superclock_ticks_per_second);

		/* convert superbeats to beats:ticks */
		int32_t b;
		int32_t t;

		Tempo::superbeats_to_beats_ticks (superbeats, b, t);

		return Beats (b, t);
	}

	const double b = (exp (_omega * (sc - _sclock)) - 1) / (superclocks_per_quarter_note() * _omega);
	return _quarters + Beats::from_double (b);
}

MeterPoint::MeterPoint (TempoMap const & map, XMLNode const & node)
	: Meter (node)
	, Point (map, node)
{
}

/* Given a time in BBT_Time, compute the equivalent Beat Time.
 *
 * Computation assumes that the Meter is in effect at the time specified as
 * BBT_Time (i.e. there is no other MeterPoint between this one and the specified
 * time.
 */
Temporal::Beats
MeterPoint::quarters_at (Temporal::BBT_Time const & bbt) const
{
	Temporal::BBT_Offset offset = bbt_delta (bbt, _bbt);
	return _quarters + to_quarters (offset);
}

/* Given a time in Beats, compute the equivalent BBT Time.
 *
 * Computation assumes that the Meter is in effect at the time specified in
 * Beats (i.e. there is no other MeterPoint between this one and the specified
 * time.
 */

Temporal::BBT_Time
MeterPoint::bbt_at (Temporal::Beats const & qn) const
{
	return bbt_add (_bbt, Temporal::BBT_Offset (0, 0,  (qn - _quarters).to_ticks()));
}

XMLNode&
MeterPoint::get_state () const
{
	XMLNode& base (Meter::get_state());
	Point::add_state (base);
	return base;
}

Temporal::BBT_Time
TempoMetric::bbt_at (superclock_t sc) const
{
	const Beats dq = _tempo->quarters_at (sc) - _meter->beats();
	const BBT_Offset bbt_offset (0, dq.get_beats(), dq.get_ticks());
	return _meter->bbt_add (_meter->bbt(), bbt_offset);
}

superclock_t
TempoMetric::superclock_at (BBT_Time const & bbt) const
{
	return _tempo->superclock_at (_meter->quarters_at (bbt));
}

MusicTimePoint::MusicTimePoint (TempoMap const & map, XMLNode const & node)
	: Point (map, node)
{
}

XMLNode&
MusicTimePoint::get_state () const
{
	XMLNode* node = new XMLNode (X_("MusicTime"));
	Point::add_state (*node);
	return *node;
}

void
TempoMapPoint::start_float ()
{
	_floating = true;
}

void
TempoMapPoint::end_float ()
{
	_floating = false;
}

/* TEMPOMAP */

TempoMap::TempoMap (Tempo const & initial_tempo, Meter const & initial_meter)
{
	TempoPoint* tp = new TempoPoint (*this, initial_tempo, 0, Beats(), BBT_Time());
	MeterPoint* mp = new MeterPoint (*this, initial_meter, 0, Beats(), BBT_Time());
	MusicTimePoint* mtp = new MusicTimePoint (*this);

	_tempos.push_back   (*tp);
	_meters.push_back   (*mp);
	_bartimes.push_back (*mtp);

	_points.push_back (*tp);
	_points.push_back (*mp);
	_points.push_back (*mtp);
}

TempoMap::~TempoMap()
{
}

TempoMap::TempoMap (XMLNode const & node, int version)
{
	set_state (node, version);
}

TempoMap::TempoMap (TempoMap const & other)
	: _time_domain (other.time_domain())
{
#warning NUTEMPO since these lists are intrusive we must actually rebuild them
	// _meters = other._meters;
	// _bartimes = other._bartimes;
	// _points = other._points;
	// _tempos = other._tempos;
}

void
TempoMap::set_time_domain (TimeDomain td)
{
	if (td == time_domain()) {
		return;
	}

#warning paul tempo_map::set_time_domain needs implementing
#if 0
	switch (td) {
	case AudioTime:
		for (Tempos::iterator t = _tempos.begin(); t != _tempos.end(); ++t) {
			t->set_sclock (t->superclock_at (t->beats ()));
		}
		for (Meters::iterator m = _meters.begin(); m != _meters.end(); ++m) {
			m->set_sclock (m->superclock_at (m->beats ()));
		}
		break;

	default:
		for (Tempos::iterator t = _tempos.begin(); t != _tempos.end(); ++t) {
			t->set_beats (t->quarters_at (t->sclock()));
		}
		for (Meters::iterator m = _meters.begin(); m != _meters.end(); ++m) {
			m->set_beats (m->quarters_at (m->sclock()));
		}
	}
#endif

	_time_domain = td;
}

MeterPoint*
TempoMap::add_meter (MeterPoint & mp)
{
	/* CALLER MUST HOLD LOCK */

	Meters::iterator m;

	switch (time_domain()) {
	case AudioTime:
		for (m = _meters.begin(); m != _meters.end() && m->sclock() < mp.sclock(); ++m);
		break;
	case BeatTime:
		for (m = _meters.begin(); m != _meters.end() && m->beats() < mp.beats(); ++m);
		break;
	case BarTime:
		for (m = _meters.begin(); m != _meters.end() && m->bbt() < mp.bbt(); ++m);
		break;

	}

	bool replaced = false;
	MeterPoint* ret = 0;

	if (m != _meters.end()) {
		if (m->sclock() == mp.sclock()) {
			/* overwrite Meter part of this point */
			*((Meter*)&(*m)) = mp;
			ret = &(*m);
			replaced = true;
		}
	}

	if (!replaced) {
		ret = &(*(_meters.insert (m, mp)));
	}

	reset_starting_at (mp.sclock());

	return ret;
}

void
TempoMap::change_tempo (TempoPoint & p, Tempo const & t)
{
	*((Tempo*)&p) = t;
}

TempoPoint &
TempoMap::set_tempo (Tempo const & t, BBT_Time const & bbt)
{
	return set_tempo (t, timepos_t (quarter_note_at (bbt)));
}

TempoPoint &
TempoMap::set_tempo (Tempo const & t, timepos_t const & time)
{
	TempoPoint * ret;

	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("Set tempo @ %1 to %2\n", time, t));

	if (time.is_beats()) {


		/* tempo changes are required to be on-beat */

		Beats on_beat = time.beats().round_to_beat();
		superclock_t sc;
		BBT_Time bbt;

		TempoMetric metric (metric_at_locked (on_beat, false));

		bbt = metric.bbt_at (on_beat);
		sc = metric.superclock_at (on_beat);

		TempoPoint tp (*this, t, sc, on_beat, bbt);
		ret = add_tempo (tp);

	} else {

		Beats beats;
		BBT_Time bbt;
		superclock_t sc = time.superclocks();

		TempoMetric tm (metric_at_locked (sc, false));

		/* tempo changes must be on beat */

		beats = tm.quarters_at (sc).round_to_beat ();
		bbt = tm.bbt_at (beats);

		/* recompute superclock position of rounded beat */
		sc = tm.superclock_at (beats);

		TempoPoint tp (*this, t, sc, beats, bbt);
		ret = add_tempo (tp);

	}

	Changed ();

	return *ret;
}

TempoPoint*
TempoMap::add_tempo (TempoPoint & tp)
{
	/* CALLER MUST HOLD LOCK */

	Tempos::iterator t;

	switch (time_domain()) {
	case AudioTime:
		for (t = _tempos.begin(); t != _tempos.end() && t->sclock() < tp.sclock(); ++t);
		break;
	case BeatTime:
		for (t = _tempos.begin(); t != _tempos.end() && t->beats() < tp.beats(); ++t);
		break;
	case BarTime:
		for (t = _tempos.begin(); t != _tempos.end() && t->bbt() < tp.bbt(); ++t);
		break;
	}

	bool replaced = false;
	TempoPoint* ret = 0;

	if (t != _tempos.end()) {
		if (t->sclock() == tp.sclock()) {
			/* overwrite Tempo part of this point */
			*((Tempo*)&(*t)) = tp;
			ret = &(*t);
			DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("overwrote old tempo with %1\n", tp));
			replaced = true;
		}
	}

	if (!replaced) {
		t = _tempos.insert (t, tp);
		ret = &*t;
		DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("inserted tempo %1\n", tp));
	}

	/* t is guaranteed not to be _tempos.end() : it was either the
	 * TempoPoint we overwrote, or its the one we inserted.
	 */

	assert (t != _tempos.end());

	Tempos::iterator nxt = t;
	++nxt;

	if (t->ramped() && nxt != _tempos.end()) {
		DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("compute ramp over %1 .. %2 aka %3 .. %4\n", t->sclock(), nxt->sclock(), t->beats(), nxt->beats()));
		t->compute_omega (_thread_sample_rate, nxt->superclocks_per_quarter_note (), nxt->beats() - t->beats());
	}

	reset_starting_at (tp.sclock());

	return ret;
}

void
TempoMap::remove_tempo (TempoPoint const & tp)
{
	{
		superclock_t sc (tp.sclock());
		Tempos::iterator t;
		for (t = _tempos.begin(); t != _tempos.end() && t->sclock() < tp.sclock(); ++t);
		if (t->sclock() != tp.sclock()) {
			/* error ... no tempo point at the time of tp */
			return;
		}
		_tempos.erase (t);
		reset_starting_at (sc);
	}

	Changed ();
}

MusicTimePoint &
TempoMap::set_bartime (BBT_Time const & bbt, timepos_t const & pos)
{
	MusicTimePoint * ret;

	assert (pos.time_domain() == AudioTime);

	{
		superclock_t sc (pos.superclocks());

		TempoMetric metric (metric_at_locked (sc));

		MusicTimePoint tp (bbt, Point (*this, sc, metric.quarters_at (sc), bbt));

		ret = add_or_replace_bartime (tp);
	}

	Changed ();

	return *ret;
}

MusicTimePoint*
TempoMap::add_or_replace_bartime (MusicTimePoint & tp)
{
	/* CALLER MUST HOLD LOCK */

	MusicTimes::iterator m;

	for (m = _bartimes.begin(); m != _bartimes.end() && m->sclock() < tp.sclock(); ++m);

	bool replaced = false;
	MusicTimePoint* ret = 0;

	if (m != _bartimes.end()) {
		if (m->sclock() == tp.sclock()) {
			/* overwrite the point with */
			*m = tp;
			ret = &(*m);
			DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("overwrote old bartime with %1\n", tp));
			replaced = true;
		}
	}

	if (!replaced) {
		m = _bartimes.insert (m, tp);
		ret = &*m;
		DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("inserted bartime %1\n", tp));
	}

	/* m is guaranteed not to be _bartimes.end() : it was either the
	 * TempoPoint we overwrote, or its the one we inserted.
	 */

	assert (m != _bartimes.end());

	reset_starting_at (tp.sclock());

	return ret;
}

void
TempoMap::remove_bartime (MusicTimePoint const & tp)
{
	{
		superclock_t sc (tp.sclock());
		MusicTimes::iterator m;
		for (m = _bartimes.begin(); m != _bartimes.end() && m->sclock() < tp.sclock(); ++m);
		if (m->sclock() != tp.sclock()) {
			/* error ... no tempo point at the time of tp */
			return;
		}
		_bartimes.erase (m);
		reset_starting_at (sc);
	}

	Changed ();
}

void
TempoMap::reset_starting_at (superclock_t sc)
{
	/* CALLER MUST HOLD LOCK */

	Tempos::iterator t;
	Meters::iterator m;
	MusicTimes::iterator b;

	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("reset starting at %1\n", sc));

	assert (!_tempos.empty());
	assert (!_meters.empty());

	TempoPoint* current_tempo = 0;
	MeterPoint* current_meter = 0;

	/* our task:

	   1) set t, m and b to the iterators for the tempo, meter and bartime
	   markers (if any) closest to but after @param sc.

	   2) set current_tempo and current_meter to point to the tempo and
	   meter in effect at @param sc
	*/

	if (sc) {
		for (t = _tempos.begin(); t != _tempos.end() && t->sclock() <= sc; ++t) {
			current_tempo = &*t;
		}
		for (m = _meters.begin(); m != _meters.end() && m->sclock() <= sc; ++m) {
			current_meter = &*m;
		}
		for (b = _bartimes.begin(); b != _bartimes.end() && b->sclock() <= sc; ++b);
	} else {
		t = _tempos.begin();
		m = _meters.begin();
		b = _bartimes.begin();

		current_meter = &*m;
		current_tempo = &*t;
	}

	Tempos::iterator nxt_tempo = _tempos.begin();

	while ((t != _tempos.end()) || (m != _meters.end()) || (b != _bartimes.end())) {

               /* UPDATE RAMP COEFFICIENTS WHEN NECESSARY */

		if (t->ramped() && (nxt_tempo != _tempos.begin()) && (nxt_tempo != _tempos.end())) {
		       t->compute_omega (_thread_sample_rate, nxt_tempo->superclocks_per_quarter_note (), nxt_tempo->beats() - t->beats());
	       }

	       /* figure out which of the 1, 2 or 3 possible iterators defines the next explicit point (we want the earliest on the timeline,
                  but there may be more than 1 at the same location).
               */

	       Point* first_of_three = 0;
	       superclock_t limit = INT64_MAX;
	       bool is_bartime = false;

	       if (m != _meters.end() && m->sclock() < limit) {
                       first_of_three = &*m;
                       limit = m->sclock();
               }

	       if (t != _tempos.end() && t->sclock() < limit) {
                       first_of_three = &*t;
                       limit = t->sclock();
               }

               if (b != _bartimes.end() && b->sclock() < limit) {
                       first_of_three = &*b;
                       limit = b->sclock();
                       is_bartime = true;
               }

               assert (first_of_three);

               /* Determine whether a tempo or meter or bartime point (or any combination thereof) is defining this new point */

               bool advance_meter = false;
               bool advance_tempo = false;
               bool advance_bartime = false;

               TempoMetric metric (*current_tempo, *current_meter);

               if (m->sclock() == first_of_three->sclock()) {
                       advance_meter = true;
                       current_meter = &*m;
                       DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("\tcurrent point defines meter %1\n", *current_meter));
               }

               if (t->sclock() == first_of_three->sclock()) {
                       advance_tempo = true;
                       current_tempo = &*t;
                       DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("\tcurrent point defines tempo %1\n", *current_tempo));
               }

               if ((b != _bartimes.end()) && (b->sclock() == first_of_three->sclock())) {
                       advance_bartime = true;
                       DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("\tcurrent point defines bartime %1\n", *b));
               }

               if (!is_bartime) {
	               superclock_t sc = metric.superclock_at (first_of_three->bbt());
	               DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("\tbased on %1 move to %2,%3\n", first_of_three->bbt(), sc, first_of_three->beats()));
	               first_of_three->set (sc, first_of_three->beats(), first_of_three->bbt());
               } else {


               }

               if (advance_meter && (m != _meters.end())) {
                       ++m;
               }
               if (advance_tempo && (t != _tempos.end())) {
                       ++t;
                       nxt_tempo = t;
                       ++nxt_tempo;
               }
               if (advance_bartime && (b != _bartimes.end())) {
                       ++b;
               }
	}

	DEBUG_TRACE (DEBUG::TemporalMap, "reset done\n");
#ifndef NDEBUG
	if (DEBUG_ENABLED (DEBUG::TemporalMap)) {
		dump_locked (cerr);
	}
#endif
}

bool
TempoMap::move_meter (MeterPoint const & mp, timepos_t const & when, bool push)
{
	{
		assert (time_domain() != BarTime);
		assert (!_tempos.empty());
		assert (!_meters.empty());

		if (_meters.size() < 2 || mp == _meters.front()) {
			/* not movable */
			return false;
		}

		superclock_t sc;
		Beats beats;
		BBT_Time bbt;
		TimeDomain td (time_domain());
		bool round_up;

		switch (td) {
		case AudioTime:
			sc = when.superclocks();
			if (sc > mp.sclock()) {
				round_up = true;
			} else {
				round_up = false;
			}
			break;
		case BeatTime:
			beats = when.beats ();
			if (beats > mp.beats ()) {
				round_up = true;
			} else {
				round_up = false;
			}
			break;
		}

		/* Do not allow moving a meter marker to the same position as
		 * an existing one.
		 */

		Tempos::iterator t, prev_t;
		Meters::iterator m, prev_m;

		switch (time_domain()) {
		case AudioTime: {
			for (t = _tempos.begin(), prev_t = _tempos.end(); t != _tempos.end() && t->sclock() < sc; ++t) { prev_t = t; }
			for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end() && m->sclock() < sc && *m != mp; ++m) { prev_m = m; }
			assert (prev_m != _meters.end());
			if (prev_t == _tempos.end()) { prev_t = _tempos.begin(); }
			TempoMetric metric (*prev_t, *prev_m);
			bbt = metric.bbt_at (sc);
			bbt = metric.meter().round_to_bar (bbt);
			for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end() && m->bbt() < bbt && *m != mp; ++m) {prev_m = m; }
			for (t = _tempos.begin(), prev_t = _tempos.end(); t != _tempos.end() && t->bbt() < bbt; ++t) { prev_t = t; }
			assert (prev_m != _meters.end());
			if (prev_t == _tempos.end()) { prev_t = _tempos.begin(); }
			metric = TempoMetric (*prev_t, *prev_m);
			sc = metric.superclock_at (bbt);
			for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end(); ++m) {
				if (&*m != &mp) {
					if (m->sclock() == sc) {
						return false;
					}
				}
			}
			beats = metric.quarters_at (bbt);
			break;
		}

		case BeatTime: {
			/* meter changes must be on bar */
			for (t = _tempos.begin(), prev_t = _tempos.end(); t != _tempos.end() && t->beats() < beats; ++t) { prev_t = t; }
			for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end() && m->beats() < beats && *m != mp; ++m) { prev_m = m; }
			assert (prev_m != _meters.end());
			if (prev_t == _tempos.end()) { prev_t = _tempos.begin(); }
			TempoMetric metric (*prev_t, *prev_m);
			bbt = metric.bbt_at (beats);
			if (round_up) {
				bbt = metric.meter().round_up_to_bar (bbt);
			} else {
				bbt = metric.meter().round_down_to_bar (bbt);
			}
			for (t = _tempos.begin(), prev_t = _tempos.end(); t != _tempos.end() && t->bbt() < bbt; ++t) { prev_t = t; }
			for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end() && m->bbt() < bbt && *m != mp; ++m) { prev_m = m; }
			assert (prev_m != _meters.end());
			if (prev_t == _tempos.end()) { prev_t = _tempos.begin(); }
			metric = TempoMetric (*prev_t, *prev_m);
			beats = metric.quarters_at (bbt);
			for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end(); ++m) {
				if (&*m != &mp) {
					if (m->beats() == beats) {
						return false;
					}
				}
			}
			sc = metric.superclock_at (bbt);
			break;
		}

		default:
			/* NOTREACHED */
			return false;
		}

		if (mp.sclock() == sc && mp.beats() == beats && mp.bbt() == bbt) {
			return false;
		}

		const superclock_t old_sc = mp.sclock();

		Meters::iterator current = _meters.end();
		Meters::iterator insert_before = _meters.end();

		for (Meters::iterator m = _meters.begin(); m != _meters.end(); ++m) {
			if (*m == mp) {
				current = m;
			}
			if (insert_before == _meters.end() && (m->sclock() > sc)) {
				insert_before = m;
			}
		}

		/* existing meter must have been found */
		assert (current != _meters.end());

		/* reset position of this meter */
		current->set (sc, beats, bbt);
		/* reposition in list */
		_meters.splice (insert_before, _meters, current);
		/* recompute 3 domain positions for everything after this */
		reset_starting_at (std::min (sc, old_sc));
	}

	Changed ();

	return true;
}

bool
TempoMap::move_tempo (TempoPoint const & tp, timepos_t const & when, bool push)
{
	{
		assert (time_domain() != BarTime);
		assert (!_tempos.empty());
		assert (!_meters.empty());

		if (_tempos.size() < 2 || tp == _tempos.front()) {
			/* not movable */
			return false;
		}

		superclock_t sc;
		Beats beats;
		BBT_Time bbt;
		TimeDomain td (time_domain());

		switch (td) {
		case AudioTime:
			sc = when.superclocks();
			break;
		case BeatTime:
			beats = when.beats ();
			break;
		}

		/* Do not allow moving a tempo marker to the same position as
		 * an existing one.
		 */

		Tempos::iterator t, prev_t;
		Meters::iterator m, prev_m;

		switch (time_domain()) {
		case AudioTime: {
			for (t = _tempos.begin(), prev_t = _tempos.end(); t != _tempos.end() && t->sclock() < sc && *t != tp; ++t) { prev_t = t; }
			for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end() && m->sclock() < sc; ++m) { prev_m = m; }
			assert (prev_t != _tempos.end());
			if (prev_m == _meters.end()) { prev_m = _meters.begin(); }
			TempoMetric metric (*prev_t, *prev_m);
			beats = metric.quarters_at (sc);
			/* tempo changes must be on beat, so round and then
			 * recompute superclock and BBT with rounded result
			 */
			beats = beats.round_to_beat ();
			for (t = _tempos.begin(), prev_t = _tempos.end(); t != _tempos.end() && t->sclock() < sc && *t != tp; ++t) { prev_t = t; }
			for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end() && m->sclock() < sc; ++m) { prev_m = m; }
			assert (prev_t != _tempos.end());
			if (prev_m == _meters.end()) { prev_m = _meters.begin(); }
			metric = TempoMetric (*prev_t, *prev_m);
			sc = metric.superclock_at (beats);
			bbt = metric.bbt_at (beats);
			break;
		}

		case BeatTime: {
			/* tempo changes must be on beat */
			beats = beats.round_to_beat ();
			for (t = _tempos.begin(), prev_t = _tempos.end(); t != _tempos.end() && t->beats() < beats && *t != tp; ++t) { prev_t = t; }
			for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end() && m->beats() < beats; ++m) { prev_m = m; }
			assert (prev_t != _tempos.end());
			assert (prev_m != _meters.end());
			TempoMetric metric (*prev_t, *prev_m);
			sc = metric.superclock_at (beats);
			bbt = metric.bbt_at (beats);
			break;
		}

		default:
			/* NOTREACHED */
			return false;
		}

		if (tp.sclock() == sc && tp.beats() == beats && tp.bbt() == bbt) {
			return false;
		}

		const superclock_t old_sc = tp.sclock();

		Tempos::iterator current = _tempos.end();
		Tempos::iterator insert_before = _tempos.end();

		for (Tempos::iterator t = _tempos.begin(); t != _tempos.end(); ++t) {
			if (*t == tp) {
				current = t;
			}
			if (insert_before == _tempos.end() && (t->sclock() > sc)) {
				insert_before = t;
			}
		}

		/* existing tempo must have been found */
		assert (current != _tempos.end());

		/* reset position of this tempo */
		current->set (sc, beats, bbt);
		/* reposition in list */
		_tempos.splice (insert_before, _tempos, current);

               /* Update ramp coefficients when necessary */

	       if (current->ramped() && insert_before != _tempos.end()) {
		       current->compute_omega (_thread_sample_rate, insert_before->superclocks_per_quarter_note (), insert_before->beats() - current->beats());
	       }

		/* recompute 3 domain positions for everything after this */
		reset_starting_at (std::min (sc, old_sc));
	}

	Changed ();

	return true;
}

MeterPoint &
TempoMap::set_meter (Meter const & m, timepos_t const & time)
{
	MeterPoint * ret = 0;

	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("Set meter @ %1 to %2\n", time, m));

	if (time.is_beats()) {

		Beats beats (time.beats());
		TempoMetric metric (metric_at_locked (beats));

		/* meter changes are required to be on-bar */

		BBT_Time rounded_bbt = metric.bbt_at (beats);
		rounded_bbt = metric.round_to_bar (rounded_bbt);

		const Beats rounded_beats = metric.quarters_at (rounded_bbt);
		const superclock_t sc = metric.superclock_at (rounded_beats);

		MeterPoint mp (*this, m, sc, rounded_beats, rounded_bbt);

		ret = add_meter (mp);

	} else {

		superclock_t sc (time.superclocks());
		Beats beats;
		BBT_Time bbt;

		TempoMetric metric (metric_at_locked (sc));

		/* meter changes must be on bar */

		bbt = metric.bbt_at (beats);
		bbt = metric.round_to_bar (bbt);

		/* compute beat position */
		beats = metric.quarters_at (bbt);

		/* recompute superclock position of bar-rounded position */
		sc = metric.superclock_at (beats);

		MeterPoint mp (*this, m, sc, beats, bbt);
		ret = add_meter (mp);
	}

	Changed ();

	return *ret;
}

MeterPoint &
TempoMap::set_meter (Meter const & t, BBT_Time const & bbt)
{
	return set_meter (t, timepos_t (quarter_note_at (bbt)));
}

void
TempoMap::remove_meter (MeterPoint const & mp)
{
	{
		superclock_t sc = mp.sclock();
		Meters::iterator m = std::upper_bound (_meters.begin(), _meters.end(), mp, Point::sclock_comparator());
		if (m->sclock() != mp.sclock()) {
			/* error ... no meter point at the time of mp */
			return;
		}
		_meters.erase (m);
		reset_starting_at (sc);
	}

	Changed ();

}

Temporal::BBT_Time
TempoMap::bbt_at (timepos_t const & pos) const
{
	if (pos.is_beats()) {
		return bbt_at (pos.beats());
	}
	return bbt_at (pos.superclocks());
}

Temporal::BBT_Time
TempoMap::bbt_at (superclock_t s) const
{
	return metric_at_locked (s).bbt_at (s);
}

Temporal::BBT_Time
TempoMap::bbt_at (Temporal::Beats const & qn) const
{
	return metric_at_locked (qn).bbt_at (qn);
}

#if 0
samplepos_t
TempoMap::sample_at (Temporal::Beats const & qn) const
{
	return superclock_to_samples (metric_at_locked (qn).superclock_at (qn), _thread_sample_rate);
}

samplepos_t
TempoMap::sample_at (Temporal::BBT_Time const & bbt) const
{
	return samples_to_superclock (metric_at_locked (bbt).superclock_at (bbt), _thread_sample_rate);
}

samplepos_t
TempoMap::sample_at (timepos_t const & pos) const
{
	if (pos.is_beat()) {
		return sample_at (pos.beats ());
	}

	/* somewhat nonsensical to call this under these conditions but ... */

	return pos.superclocks();
}
#endif

superclock_t
TempoMap::superclock_at (Temporal::Beats const & qn) const
{
	return metric_at_locked (qn).superclock_at (qn);
}

superclock_t
TempoMap::superclock_at (Temporal::BBT_Time const & bbt) const
{
	return metric_at_locked (bbt).superclock_at (bbt);
}

superclock_t
TempoMap::superclock_at (timepos_t const & pos) const
{
	if (pos.is_beats()) {
		return superclock_at (pos.beats ());
	}

	/* somewhat nonsensical to call this under these conditions but ... */

	return pos.superclocks();
}

superclock_t
TempoMap::superclock_plus_bbt (superclock_t pos, BBT_Time op) const
{
	BBT_Time pos_bbt = bbt_at (pos);

	pos_bbt.ticks += op.ticks;
	if (pos_bbt.ticks >= ticks_per_beat) {
		++pos_bbt.beats;
		pos_bbt.ticks -= ticks_per_beat;
	}
	pos_bbt.beats += op.beats;

	double divisions_per_bar = metric_at_locked (pos_bbt).divisions_per_bar();
	while (pos_bbt.beats >= divisions_per_bar + 1) {
		++pos_bbt.bars;
		divisions_per_bar = metric_at_locked (pos_bbt).divisions_per_bar();
		pos_bbt.beats -= divisions_per_bar;
	}
	pos_bbt.bars += op.bars;

	return superclock_at (pos_bbt);
}

#define S2Sc(s) (samples_to_superclock ((s), _thread_sample_rate))
#define Sc2S(s) (superclock_to_samples ((s), _thread_sample_rate))

/** Count the number of beats that are equivalent to distance when going forward,
    starting at pos.
*/
Temporal::Beats
TempoMap::scwalk_to_quarters (superclock_t pos, superclock_t distance) const
{
	TempoMetric first (metric_at (pos));
	TempoMetric last (metric_at (pos+distance));
	Temporal::Beats a = first.quarters_at (pos);
	Temporal::Beats b = last.quarters_at (pos+distance);
	return b - a;
}

Temporal::Beats
TempoMap::scwalk_to_quarters (Temporal::Beats const & pos, superclock_t distance) const
{
	/* XXX this converts from beats to superclock and back to beats... which is OK (reversible) */
	superclock_t s = metric_at_locked (pos).superclock_at (pos);
	s += distance;
	return metric_at_locked (s).quarters_at (s);

}

Temporal::Beats
TempoMap::bbtwalk_to_quarters (Beats const & pos, BBT_Offset const & distance) const
{
	return quarter_note_at (bbt_walk (bbt_at (pos), distance)) - pos;
}

void
TempoMap::sample_rate_changed (samplecnt_t new_sr)
{
	const double ratio = new_sr / (double) _thread_sample_rate;

	for (Tempos::iterator t = _tempos.begin(); t != _tempos.end(); ++t) {
		t->map_reset_set_sclock_for_sr_change (llrint (ratio * t->sclock()));
	}

	for (Meters::iterator m = _meters.begin(); m != _meters.end(); ++m) {
		m->map_reset_set_sclock_for_sr_change (llrint (ratio * m->sclock()));
	}

	for (MusicTimes::iterator p = _bartimes.begin(); p != _bartimes.end(); ++p) {
		p->map_reset_set_sclock_for_sr_change (llrint (ratio * p->sclock()));
	}
}

void
TempoMap::dump (std::ostream& ostr) const
{
	dump_locked (ostr);
}

void
TempoMap::dump_locked (std::ostream& ostr) const
{
	for (Tempos::const_iterator t = _tempos.begin(); t != _tempos.end(); ++t) {
		ostr << &*t << ' ' << *t << endl;
	}

	for (Meters::const_iterator m = _meters.begin(); m != _meters.end(); ++m) {
		ostr << &*m << ' ' << *m << endl;
	}
}

void
TempoMap::get_grid (TempoMapPoints& ret, superclock_t start, superclock_t end, uint32_t bar_mod)
{
	assert (!_tempos.empty());
	assert (!_meters.empty());

	DEBUG_TRACE (DEBUG::TemporalMap, string_compose (">>> GRID START %1 .. %2 (barmod = %3)\n", start, end, bar_mod))

	Tempos::iterator t (_tempos.begin());
	Meters::iterator m (_meters.begin());
	MusicTimes::iterator b (_bartimes.begin());

	TempoMetric metric = metric_at_locked (start, false);
	BBT_Time bbt = metric.bbt_at (start);

#ifndef NDEBUG
	/* Sanity Check */

	if (DEBUG_ENABLED(PBD::DEBUG::TemporalMap)) {
		TempoMetric emetric = metric_at_locked (end, false);
		BBT_Time ebbt = metric_at_locked (end).bbt_at (end);

		DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("get grid between %1..%2 [ %4 .. %5 ] { %6 .. %7 } at bar_mod = %3\n",
		                                                 start, end, bar_mod, start, end, bbt, ebbt));

		if (metric.quarters_at (bbt).diff (metric.quarters_at (start)) > Beats::ticks (1)) {
			cerr << "MM1: " << start << " / " << metric.quarters_at (start) << " vs. "
			     << metric.superclock_at (bbt) << " / " << metric.quarters_at (bbt)
			     << " delta "
			     << start - metric.superclock_at (bbt)
			     << " dB "
			     << metric.quarters_at (bbt).diff (metric.quarters_at (start))
			     << "\n\tused " << metric
			     << endl;
			abort ();
		}

		if (emetric.quarters_at (ebbt).diff (emetric.quarters_at (end)) > Beats::ticks (1)) {
			cerr << "MM2: " << end << " / " << emetric.quarters_at (end) << " vs. "
			     << emetric.superclock_at (ebbt) << " / " << emetric.quarters_at (ebbt)
			     << " delta "
			     << end - emetric.superclock_at (ebbt)
			     << " dB "
			     << emetric.quarters_at (ebbt).diff (emetric.quarters_at (end))
			     << "\n\tused " << emetric
			     << endl;
			abort ();
		}

		dump (cerr);
	}
#endif

	/* first task: get to the right starting point for the requested
	 * grid. if bar_mod is zero, then we'll start on the next beat after
	 * @param start. if bar_mod is non-zero, we'll start on the first bar
	 * after @param start. This bar position may or may not be a part of the
	 * grid, depending on whether or not it is a multiple of bar_mod.
	 */

	if (bar_mod == 0) {

		/* round to next beat, then find the tempo/meter/bartime points
		 * in effect at that time.
		 */

		bbt = metric.meter().round_up_to_beat (bbt);

		for (Tempos::iterator tt = _tempos.begin(); tt != _tempos.end() && tt->sclock() < start; ++tt) { t = tt; }
		for (Meters::iterator mm = _meters.begin(); mm != _meters.end() && mm->sclock() < start; ++mm) { m = mm; }
		for (MusicTimes::iterator bb = _bartimes.begin(); bb != _bartimes.end() && bb->sclock() < start; ++bb) { b = bb; }

		/* reset metric */

		metric = TempoMetric (*t, *m);

		/* recompute superclock position */

		superclock_t new_start = metric.superclock_at (bbt);

		if (new_start < start) {
			abort ();
		}

		start = new_start;

	} else {

		/* this rounding cannot change the meter in effect, because it
		   remains within the bar. But it could change the tempo (which
		   are only quantized to grid positions within a bar).
		*/

		BBT_Time bar = bbt.round_down_to_bar ();
		if (bar_mod != 1) {
			bar.bars -= bar.bars % bar_mod;
			++bar.bars;
		}

		bbt = bar;

		for (Tempos::iterator tt = _tempos.begin(); tt != _tempos.end() && tt->bbt() < bbt; ++tt) { t = tt; }
		for (Meters::iterator mm = _meters.begin(); mm != _meters.end() && mm->bbt() < bbt; ++mm) { m = mm; }
		for (MusicTimes::iterator bb = _bartimes.begin(); bb != _bartimes.end() && bb->bbt() < bbt; ++bb) { b = bb; }

		/* t, m and b are now all iterators for the tempo, meter and
		 * position markers BEFORE pos. b may be _bartimes.end(), but
		 * the other two are guaranteed to be valid references into
		 * the tempos and meters
		 */

		metric = TempoMetric (*t, *m);
		start = metric.superclock_at (bbt);
	}

	/* advance t, m and b so that the point to the *next*
	 * tempo/meter/position marker (if any)
	 */

	Tempos::iterator nxt_t = t; ++nxt_t;
	Meters::iterator nxt_m = m; ++nxt_m;
	MusicTimes::iterator nxt_b = b; ++nxt_b;

	/* at this point:
	 *
	 * - metric is a TempoMetric that describes the situation at pos
	 * - t, m and b reference tempo, meter and position markers at or prior to pos (if any)
	 * - nxt_t, nxt_m, nxt_b reference the tempo, meter and position markers after pos (if any)
	 *
	 * t and m must be valid; b, nxt_t, nxt_m, nxt_b may all refer to ::end() of their respective containers.
	 */

	/* outer loop: compute next marker position, if any, and then set limit to the earlier of that position or @param e.
	 * Then run the inner loop to actually add grid points up until limit. Repeat till done.
	 */

	while (start < end) {

		bool advance_tempo = false;
		bool advance_meter = false;
		bool advance_bartime = false;
		Point* first_of_three = 0;
		superclock_t limit = INT64_MAX;

		if (nxt_t != _tempos.end() && limit >= nxt_t->sclock()) {
			first_of_three = &*nxt_t;
			limit = first_of_three->sclock();
		}

		if (nxt_m != _meters.end() && limit >= nxt_m->sclock()) {
			first_of_three = &*nxt_m;
			limit = first_of_three->sclock();
		}

		if (nxt_b != _bartimes.end() && limit >= nxt_b->sclock()) {
			first_of_three = &*nxt_b;
			limit = first_of_three->sclock();
		}

		if (first_of_three) {
			if (nxt_m != _meters.end() && nxt_m->sclock() == first_of_three->sclock()) {
				advance_meter = true;
			}

			if (nxt_t != _tempos.end() && nxt_t->sclock() == first_of_three->sclock()) {
				advance_tempo = true;
			}

			if ((nxt_b != _bartimes.end()) && (b->sclock() == first_of_three->sclock())) {
				advance_bartime = true;
			}

			limit = std::min (end, first_of_three->sclock());
		} else {
			limit = end;
		}

		if (start >= limit) {
			break;
		}

		/* Inner loop: add grid points until we hit limit, which is defined by either @param e or the next marker of some kind */

		do {

			/* we already have the superclock and BBT time for the next point, either computed before the loop, or at the bottom of this one.
			 * So now complete the triplet of (superclock,quarters,bbt)
			 */

			const Temporal::Beats beats = metric.quarters_at (start);

			/* add point to grid */
			ret.push_back (TempoMapPoint (*this, metric, start, beats, bbt));
			DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("G %1\t       %2\n", metric, ret.back()));

			/* Advance by the meter note value size */

			superclock_t step;

			if (bar_mod == 0) {

				step = metric.superclocks_per_note_type_at_superclock (start);
				start += step;

			} else {

				bbt.bars += bar_mod;

				/* could have invalidated the current metric */

				if (first_of_three && (bbt > first_of_three->bbt())) {
					start = first_of_three->sclock();
					break;
				}

				/* move superclock time forward to next (included) bar. Note that we know that metric is still
				   valid because we just checked above if we crossed a marker.
				*/

				start = metric.superclock_at (bbt);
			}

			if (start >= limit) {
				/* go back to outer loop to advance iterators and get a new metric */
				break;
			}

			if (bar_mod == 0) {
				bbt = metric.bbt_at (start);
			}

		} while (true);

		/* back in outer loop. Check to see if we passed a marker */

		if (first_of_three && (start >= first_of_three->sclock())) {

			if (advance_tempo) {
				if (nxt_t != _tempos.end()) {
					t = nxt_t;
					++nxt_t;
				}
			}
			if (advance_meter) {
				if (nxt_m != _meters.end()) {
					m = nxt_m;
					++nxt_m;
				}
			}
			if (advance_bartime) {
				b = nxt_b;
				if (nxt_b != _bartimes.end()) {
					++nxt_b;
				}
			}

			if (advance_tempo || advance_meter || advance_bartime) {

				/* we overstepped a marker

				 * if bar_mod is zero, then by definition any
				 * such marker qualifies as a grid point.
				 *
				 * if bar_mod != zero, then check to see if the new
				 * BBT position matches the interval we've been asked
				 * for. If so, use it, otherwise just continue around
				 * the loop, using the new position and metric.
				 */

				bbt = first_of_three->bbt ();

				if (bar_mod != 0) {

					/* check to see if it matches the interval */

					if (!bbt.is_bar() || (bbt.bars % bar_mod != 0))  {

						/* not usable */

						bbt = bbt.round_up_to_bar ();

						/* reset iterators for new position */

						for (Tempos::iterator tt = t; tt != _tempos.end() && tt->bbt() < bbt; ++tt) { t = tt; }
						for (Meters::iterator mm = m; mm != _meters.end() && mm->bbt() < bbt; ++mm) { m = mm; }
						for (MusicTimes::iterator bb = b; bb != _bartimes.end() && bb->bbt() < bbt; ++bb) { b = bb; }
						nxt_t = t; ++nxt_t;
						nxt_m = m; ++nxt_m;
						nxt_b = b; ++nxt_b;
					}
				}

				metric = TempoMetric (*t, *m);
				start = metric.superclock_at (bbt);

				/* ready to loop because metric, start and bbt are all set correctly, as they were when entering the outer loop */
			}
		}
	}

	DEBUG_TRACE (DEBUG::TemporalMap, "<<< GRID DONE\n");
}

std::ostream&
std::operator<<(std::ostream& str, Meter const & m)
{
	return str << m.divisions_per_bar() << '/' << m.note_value();
}

std::ostream&
std::operator<<(std::ostream& str, Tempo const & t)
{
	return str << t.note_types_per_minute() << " 1/" << t.note_type() << " notes per minute (" << t.superclocks_per_note_type() << " sc-per-1/" << t.note_type() << ')';
}

std::ostream&
std::operator<<(std::ostream& str, Point const & p)
{
	return str << "P@" << p.sclock() << '/' << p.beats() << '/' << p.bbt();
}

std::ostream&
std::operator<<(std::ostream& str, MeterPoint const & m)
{
	return str << *((Meter const *) &m) << ' ' << *((Point const *) &m);
}

std::ostream&
std::operator<<(std::ostream& str, TempoPoint const & t)
{
	str << *((Tempo const *) &t) << ' '  << *((Point const *) &t);
	if (t.ramped()) {
		if (t.actually_ramped()) {
			str << ' ' << " ramp to " << t.end_note_types_per_minute();
		} else {
			str << ' ' << " !ramp to " << t.end_note_types_per_minute();
		}
		str << " omega = " << std::setprecision(12) << t.omega();
	}
	return str;
}

std::ostream&
std::operator<<(std::ostream& str, TempoMetric const & tm)
{
	return str << tm.tempo() << ' '  << tm.meter();
}

std::ostream&
std::operator<<(std::ostream& str, TempoMapPoint const & tmp)
{
	str << '@' << std::setw (12) << tmp.sclock() << ' ' << tmp.sclock() / (double) superclock_ticks_per_second
	    << (tmp.is_explicit_tempo() ? " EXP-T" : " imp-t")
	    << (tmp.is_explicit_meter() ? " EXP-M" : " imp-m")
	    << (tmp.is_explicit_position() ? " EXP-P" : " imp-p")
	    << " qn " << tmp.beats ()
	    << " bbt " << tmp.bbt()
		;

	if (tmp.is_explicit_tempo()) {
		str << " tempo " << tmp.tempo();
	}

	if (tmp.is_explicit_meter()) {
		str << " meter " << tmp.meter();
	}

	if (tmp.is_explicit_tempo() && tmp.tempo().ramped()) {
		str << " ramp omega = " << tmp.tempo().omega();
	}

	return str;
}

superclock_t
TempoMap::superclock_plus_quarters_as_superclock (superclock_t start, Temporal::Beats const & distance) const
{
	TempoMetric metric (metric_at_locked (start));

	const Temporal::Beats start_qn = metric.quarters_at (start);
	const Temporal::Beats end_qn = start_qn + distance;

	TempoMetric end_metric (metric_at (end_qn));

	return superclock_to_samples (end_metric.superclock_at (end_qn), _thread_sample_rate);
}

Temporal::Beats
TempoMap::superclock_delta_as_quarters (superclock_t start, superclock_t distance) const
{
	return quarter_note_at (start + distance) - quarter_note_at (start);
}

Temporal::superclock_t
TempoMap::superclock_quarters_delta_as_superclock (superclock_t start, Temporal::Beats const & distance) const
{
	Temporal::Beats start_qn = metric_at_locked (start).quarters_at (start);
	start_qn += distance;
	return metric_at_locked (start_qn).superclock_at (start_qn);
}

superclock_t
TempoMap::superclock_per_quarter_note_at (superclock_t pos) const
{
	return metric_at_locked (pos).superclocks_per_quarter_note ();
}

BBT_Time
TempoMap::bbt_walk (BBT_Time const & bbt, BBT_Offset const & o) const
{
	BBT_Offset offset (o);
	Tempos::const_iterator t, prev_t, next_t;
	Meters::const_iterator m, prev_m, next_m;

	assert (!_tempos.empty());
	assert (!_meters.empty());

	/* trivial (and common) case: single tempo, single meter */

	if (_tempos.size() == 1 && _meters.size() == 1) {
		return _meters.front().bbt_add (bbt, o);
	}

	/* Find tempo,meter pair for bbt, and also for the next tempo and meter
	 * after each (if any)
	 */

	/* Yes, linear search because the typical size of _tempos and _meters
	 * is 1, and extreme sizes are on the order of 10
	 */

	next_t = _tempos.end();
	next_m = _meters.end();

	for (t = _tempos.begin(), prev_t = t; t != _tempos.end() && t->bbt() < bbt;) {
		prev_t = t;
		++t;

		if (t != _tempos.end()) {
			next_t = t;
			++next_t;
		}
	}

	for (m = _meters.begin(), prev_m = m; m != _meters.end() && m->bbt() < bbt;) {
		prev_m = m;
		++m;

		if (m != _meters.end()) {
			next_m = m;
			++next_m;
		}
	}

	/* may have found tempo and/or meter precisely at the tiem given */

	if (t != _tempos.end() && t->bbt() == bbt) {
		prev_t = t;
	}

	if (m != _meters.end() && m->bbt() == bbt) {
		prev_m = m;
	}

	/* see ::metric_at_locked() for comments about the use of const_cast here
	 */

	TempoMetric metric (*const_cast<TempoPoint*>(&*prev_t), *const_cast<MeterPoint*>(&*prev_m));
	superclock_t pos = metric.superclock_at (bbt);

	/* normalize possibly too-large ticks count */

	const int32_t tpg = metric.meter().ticks_per_grid ();

	if (offset.ticks > tpg) {
		/* normalize */
		offset.beats += offset.ticks / tpg;
		offset.ticks %= tpg;
	}

	/* add tick count, now guaranteed to be less than 1 grid unit */

	if (offset.ticks) {
		pos += metric.superclocks_per_ppqn () * offset.ticks;
	}

	/* add each beat, 1 by 1, rechecking to see if there's a new
	 * TempoMetric in effect after each addition
	 */

#define TEMPO_CHECK_FOR_NEW_METRIC                                      \
	if (((next_t != _tempos.end()) && (pos >= next_t->sclock())) || \
	    ((next_m != _meters.end()) && (pos >= next_m->sclock()))) { \
		/* need new metric */ \
		if (pos >= next_t->sclock()) { \
			if (pos >= next_m->sclock()) { \
				metric = TempoMetric (*const_cast<TempoPoint*>(&*next_t), *const_cast<MeterPoint*>(&*next_m)); \
				++next_t; \
				++next_m; \
			} else { \
				metric = TempoMetric (*const_cast<TempoPoint*>(&*next_t), metric.meter()); \
				++next_t; \
			} \
		} else if (pos >= next_m->sclock()) { \
			metric = TempoMetric (metric.tempo(), *const_cast<MeterPoint*>(&*next_m)); \
			++next_m; \
		} \
	}

	for (int32_t b = 0; b < offset.beats; ++b) {

		TEMPO_CHECK_FOR_NEW_METRIC;
		pos += metric.superclocks_per_grid (_thread_sample_rate);
	}

	/* add each bar, 1 by 1, rechecking to see if there's a new
	 * TempoMetric in effect after each addition
	 */

	for (int32_t b = 0; b < offset.bars; ++b) {

		TEMPO_CHECK_FOR_NEW_METRIC;

		pos += metric.superclocks_per_bar (_thread_sample_rate);
	}

	return metric.bbt_at (pos);
}

Temporal::Beats
TempoMap::quarter_note_at (timepos_t const & pos) const
{
	if (pos.is_beats()) {
		/* a bit redundant */
		return pos.beats();
	}
	return quarter_note_at (pos);
}

Temporal::Beats
TempoMap::quarter_note_at (Temporal::BBT_Time const & bbt) const
{
	return metric_at_locked (bbt).quarters_at (bbt);
}

Temporal::Beats
TempoMap::quarter_note_at (superclock_t pos) const
{
	return metric_at_locked (pos).quarters_at (pos);
}

XMLNode&
TempoMap::get_state ()
{
	XMLNode* node = new XMLNode (X_("TempoMap"));

	node->set_property (X_("time-domain"), _time_domain);
	node->set_property (X_("superclocks-per-second"), superclock_ticks_per_second);

	XMLNode* children;

	children = new XMLNode (X_("Tempos"));
	node->add_child_nocopy (*children);
	for (Tempos::const_iterator t = _tempos.begin(); t != _tempos.end(); ++t) {
		children->add_child_nocopy (t->get_state());
	}

	children = new XMLNode (X_("Meters"));
	node->add_child_nocopy (*children);
	for (Meters::const_iterator m = _meters.begin(); m != _meters.end(); ++m) {
		children->add_child_nocopy (m->get_state());
	}

	children = new XMLNode (X_("MusicTimes"));
	node->add_child_nocopy (*children);
	for (MusicTimes::const_iterator b = _bartimes.begin(); b != _bartimes.end(); ++b) {
		children->add_child_nocopy (b->get_state());
	}

	return *node;
}

int
TempoMap::set_state (XMLNode const & node, int /*version*/)
{
	{
		/* global map properties */

		/* XXX this should probably be at the global level in the session file because it affects a lot more than just the tempo map, potentially */
		node.get_property (X_("superclocks-per-second"), superclock_ticks_per_second);

		node.get_property (X_("time-domain"), _time_domain);

		XMLNodeList const & children (node.children());

		for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {
			if ((*c)->name() == X_("Tempos")) {
				if (set_tempos_from_state (**c)) {
					return -1;
				}
			}

			if ((*c)->name() == X_("Meters")) {
				if (set_meters_from_state (**c)) {
					return -1;
				}
			}

			if ((*c)->name() == X_("MusicTimes")) {
				if (set_music_times_from_state (**c)) {
					return -1;
				}
			}
		}
	}

	Changed ();

	return 0;
}

int
TempoMap::set_music_times_from_state (XMLNode const& tempos_node)
{
	return 0;
}

int
TempoMap::set_tempos_from_state (XMLNode const& tempos_node)
{
	/* CALLER MUST HOLD LOCK */

	XMLNodeList const & children (tempos_node.children());

	try {
		_tempos.clear ();
		for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {
			TempoPoint tp (*this, **c);
			// _tempos.push_back (TempoPoint (*this, **c));
			_tempos.push_back (tp);
		}
	} catch (...) {
		_tempos.clear (); /* remove any that were created */
		return -1;
	}

	return 0;
}

int
TempoMap::set_meters_from_state (XMLNode const& meters_node)
{
	/* CALLER MUST HOLD LOCK */

	XMLNodeList const & children (meters_node.children());

	try {
		_meters.clear ();
		for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {
			MeterPoint mp (*this, **c);
			_meters.push_back (mp);
		}
	} catch (...) {
		_meters.clear (); /* remove any that were created */
		return -1;
	}

	return 0;
}

bool
TempoMap::can_remove (TempoPoint const & t) const
{
	return !is_initial (t);
}

bool
TempoMap::is_initial (TempoPoint const & t) const
{
	return t.sclock() == 0;
}

bool
TempoMap::is_initial (MeterPoint const & m) const
{
	return m.sclock() == 0;
}

bool
TempoMap::can_remove (MeterPoint const & m) const
{
	return !is_initial (m);
}

/** returns the duration (using the domain of @param pos) of the supplied BBT time at a specified sample position in the tempo map.
 * @param pos the frame position in the tempo map.
 * @param bbt the distance in BBT time from pos to calculate.
 * @param dir the rounding direction..
 * @return the timecnt_t that @param bbt represents when starting at @param pos, in
 * the time domain of @param pos
*/
timecnt_t
TempoMap::bbt_duration_at (timepos_t const & pos, BBT_Offset const & dur) const
{
	if (pos.time_domain() == AudioTime) {
		return timecnt_t::from_superclock (superclock_at (bbt_walk (bbt_at (pos), dur)) - pos.superclocks(), pos);
	}
	return timecnt_t (bbtwalk_to_quarters (pos.beats(), dur) - pos.beats(), pos);

}

/** Takes a duration (in any time domain) and considers it as a distance from the given position.
 *  Returns a distance in the requested domain, taking tempo changes into account.
 *
 *  Obviously, if the given distance is in the same time domain as the requested domain,
 *  the returned distance is identical to the given one.
 */

timecnt_t
TempoMap::full_duration_at (timepos_t const & pos, timecnt_t const & duration, TimeDomain return_domain) const
{
	timepos_t p (return_domain);
	Beats b;
	superclock_t s;

	assert (pos.time_domain() != BarTime);
	assert (duration.time_domain() != BarTime);
	assert (return_domain != BarTime);

	if (return_domain == duration.time_domain()) {
		return duration;
	}

	switch (return_domain) {
	case AudioTime:
		switch (duration.time_domain()) {
		case AudioTime:
			/*NOTREACHED*/
			break;
		case BeatTime:
			/* duration is in beats but we're asked to return superclocks */
			switch (pos.time_domain()) {
			case BeatTime:
				/* pos is already in beats */
				p = pos;
				break;
			case AudioTime:
				/* Determine beats at sc pos, so that we can add beats */
				p = metric_at (pos).quarters_at (pos.superclocks());
				break;
			}
			/* add beats */
			p += duration;
			/* determine superclocks */
			s = metric_at (p).superclock_at (p.beats());
			/* return duration in sc */
			return timecnt_t::from_superclock (s - pos.superclocks(), pos);
			break;
		}
		break;

	case BeatTime:
		switch (duration.time_domain()) {
		case AudioTime:
			/* duration is in superclocks but we're asked to return beats */
			switch (pos.time_domain ()) {
			case AudioTime:
				/* pos is already in superclocks */
				p = pos;
				break;
			case BeatTime:
				/* determined sc at beat position so we can add superclocks */
				p = metric_at (pos).superclock_at (pos.beats());
				break;
			}
			/* add superclocks */
			p += duration;
			/* determine beats */
			b = metric_at (p).quarters_at (p.superclocks());
			/* return duration in beats */
			return timecnt_t (b - pos.beats(), pos);
			break;
		case BeatTime:
			/*NOTREACHED*/
			break;
		}
		break;
	}

	/*NOTREACHED*/
	abort ();
	/*NOTREACHED*/

	return timecnt_t::from_superclock (0);

}

Tempo const *
TempoMap::next_tempo (Tempo const & t) const
{
	Tempos::const_iterator p = _tempos.begin();

	while (p != _tempos.end()) {
		if (&t == &*p) {
			break;
		}
		++p;
	}

	if (p != _tempos.end()) {
		++p;

		if (p != _tempos.end()) {
			return &*p;;
		}
	}

	return 0;
}

uint32_t
TempoMap::n_meters () const
{
	return _meters.size();
}

uint32_t
TempoMap::n_tempos () const
{
	return _tempos.size();
}

void
TempoMap::insert_time (timepos_t const & pos, timecnt_t const & duration)
{
	assert (time_domain() != BarTime);
	assert (!_tempos.empty());
	assert (!_meters.empty());

	if (pos == std::numeric_limits<timepos_t>::min()) {
		/* can't insert time at the front of the map: those entries are fixed */
		return;
	}

	{
		Tempos::iterator     t (_tempos.begin());
		Meters::iterator     m (_meters.begin());
		MusicTimes::iterator b (_bartimes.begin());

		TempoPoint current_tempo = *t;
		MeterPoint current_meter = *m;
		MusicTimePoint current_time_point (*this);

		if (_bartimes.size() > 0) {
			current_time_point = *b;
		}

		superclock_t sc;
		Beats beats;
		BBT_Time bbt;

		/* set these to true so that we set current_* on our first pass
		 * through the while loop(s)
		 */

		bool moved_tempo = true;
		bool moved_meter = true;
		bool moved_bartime = true;

		switch (duration.time_domain()) {
		case AudioTime:
			sc = pos.superclocks();

			/* handle a common case quickly */

			if ((_tempos.size() < 2 || sc > _tempos.back().sclock()) &&
			    (_meters.size() < 2 || sc > _meters.back().sclock()) &&
			    (_bartimes.size() < 2 || (_bartimes.empty() || sc > _bartimes.back().sclock()))) {

				/* only one tempo, plus one meter and zero or
				   one bartimes, or insertion point is after last
				   item. nothing to do here.
				*/

				return;
			}

			/* advance fundamental iterators to correct position */

			while (t != _tempos.end()   && t->sclock() < sc) ++t;
			while (m != _meters.end()   && m->sclock() < sc) ++m;
			while (b != _bartimes.end() && b->sclock() < sc) ++b;

			while (t != _tempos.end() && m != _meters.end() && b != _bartimes.end()) {

				if (moved_tempo && t != _tempos.end()) {
					current_tempo = *t;
					moved_tempo = false;
				}
				if (moved_meter && m != _meters.end()) {
					current_meter = *m;
					moved_meter = false;
				}
				if (moved_bartime && b != _bartimes.end()) {
					current_time_point = *b;
					moved_bartime = false;
				}

				/* for each of t, m and b:

				   if the point is earlier than the other two,
				   recompute the superclock, beat and bbt
				   positions, and reset the point.
				*/

				if (t->sclock() < m->sclock() && t->sclock() < b->sclock()) {

					sc = t->sclock() + duration.superclocks();
					beats = current_tempo.quarters_at (sc);
					/* round tempo to beats */
					beats = beats.round_to_beat ();
					sc = current_tempo.superclock_at (beats);
					bbt = current_meter.bbt_at (beats);

					t->set (sc, beats, bbt);
					++t;
					moved_tempo = true;
				}

				if (m->sclock() < t->sclock() && m->sclock() < b->sclock()) {

					sc = m->sclock() + duration.superclocks();
					beats = current_tempo.quarters_at (sc);
					/* round meter to bars */
					bbt = current_meter.bbt_at (beats);
					beats = current_meter.quarters_at (current_meter.round_to_bar(bbt));
					/* recompute */
					sc = current_tempo.superclock_at (beats);

					m->set (sc, beats, bbt);
					++m;
					moved_meter = true;
				}

				if (b->sclock() < t->sclock() && b->sclock() < m->sclock()) {

					sc = b->sclock() + duration.superclocks();
					beats = current_tempo.quarters_at (sc);
					/* round bartime to beats */
					beats = beats.round_to_beat();
					sc = current_tempo.superclock_at (beats);
					bbt = current_meter.bbt_at (beats);

					m->set (sc, beats, bbt);
					++m;
					moved_meter = true;
				}

			}
			break;

		case BeatTime:
			break;
		}
	}

	Changed ();
}

bool
TempoMap::remove_time (timepos_t const & pos, timecnt_t const & duration)
{
	bool moved = false;

	if (moved) {
		Changed ();
	}

	return moved;
}

TempoPoint const *
TempoMap::previous_tempo (TempoPoint const & point) const
{
	Tempos::const_iterator t = _tempos.begin();
	Tempos::const_iterator prev = _tempos.end();

	while (t != _tempos.end()) {
		if (t->sclock() == point.sclock()) {
			if (prev != _tempos.end()) {
				return &*prev;
			}
		}
		prev = t;
		++t;
	}

	return 0;
}

TempoMetric
TempoMap::metric_at (timepos_t const & pos) const
{
	if (pos.is_beats()) {
		return metric_at (pos.beats());
	}

	return metric_at (pos.superclocks());
}

TempoMetric
TempoMap::metric_at (superclock_t s) const
{
	return metric_at_locked (s);
}

TempoMetric
TempoMap::metric_at (Beats const & b) const
{
	return metric_at_locked (b);
}

TempoMetric
TempoMap::metric_at (BBT_Time const & bbt) const
{
	return metric_at_locked (bbt);
}

TempoMetric
TempoMap::metric_at_locked (superclock_t sc, bool can_match) const
{
	Tempos::const_iterator t, prev_t;
	Meters::const_iterator m, prev_m;

	assert (!_tempos.empty());
	assert (!_meters.empty());

	/* Yes, linear search because the typical size of _tempos and _meters
	 * is 1, and extreme sizes are on the order of 10
	 */

	for (t = _tempos.begin(), prev_t = t; t != _tempos.end() && t->sclock() < sc; ++t) { prev_t = t; }
	for (m = _meters.begin(), prev_m = m; m != _meters.end() && m->sclock() < sc; ++m) { prev_m = m; }

	if (can_match || sc == 0) {
		/* may have found tempo and/or meter precisely at @param sc */

		if (t != _tempos.end() && t->sclock() == sc) {
			prev_t = t;
		}

		if (m != _meters.end() && m->sclock() == sc) {
			prev_m = m;
		}
	}

	/* I hate doing this const_cast<>, but making this method non-const
	 * propagates into everything that just calls metric_at(), and that's a
	 * bit ridiculous. Yes, the TempoMetric returned here can be used to
	 * change the map, and that's bad, but the non-const propagation is
	 * worse.
	 */

	return TempoMetric (*const_cast<TempoPoint*>(&*prev_t), *const_cast<MeterPoint*> (&*prev_m));
}

TempoMetric
TempoMap::metric_at_locked (Beats const & b, bool can_match) const
{
	Tempos::const_iterator t, prev_t;
	Meters::const_iterator m, prev_m;

	assert (!_tempos.empty());
	assert (!_meters.empty());

	/* Yes, linear search because the typical size of _tempos and _meters
	 * is 1, and extreme sizes are on the order of 10
	 */

	for (t = _tempos.begin(), prev_t = t; t != _tempos.end() && t->beats() < b; ++t) { prev_t = t; }
	for (m = _meters.begin(), prev_m = m; m != _meters.end() && m->beats() < b; ++m) { prev_m = m; }

	if (can_match || b == Beats()) {
		/* may have found tempo and/or meter precisely at @param b */
		if (t != _tempos.end() && t->beats() == b) {
			prev_t = t;
		}

		if (m != _meters.end() && m->beats() == b) {
			prev_m = m;
		}
	}

	/* I hate doing this const_cast<>, but making this method non-const
	 * propagates into everything that just calls metric_at(), and that's a
	 * bit ridiculous. Yes, the TempoMetric returned here can be used to
	 * change the map, and that's bad, but the non-const propagation is
	 * worse.
	 */

	return TempoMetric (*const_cast<TempoPoint*>(&*prev_t), *const_cast<MeterPoint*> (&*prev_m));
}

TempoMetric
TempoMap::metric_at_locked (BBT_Time const & bbt, bool can_match) const
{
	Tempos::const_iterator t, prev_t;
	Meters::const_iterator m, prev_m;

	assert (!_tempos.empty());
	assert (!_meters.empty());

	/* Yes, linear search because the typical size of _tempos and _meters
	 * is 1, and extreme sizes are on the order of 10
	 */

	for (t = _tempos.begin(), prev_t = t; t != _tempos.end() && t->bbt() < bbt; ++t) { prev_t = t; }
	for (m = _meters.begin(), prev_m = m; m != _meters.end() && m->bbt() < bbt; ++m) { prev_m = m; }

	if (can_match || bbt == BBT_Time()) {
		/* may have found tempo and/or meter precisely at @param bbt */

		if (t != _tempos.end() && t->bbt() == bbt) {
			prev_t = t;
		}

		if (m != _meters.end() && m->bbt() == bbt) {
			prev_m = m;
		}
	}

	/* I hate doing this const_cast<>, but making this method non-const
	 * propagates into everything that just calls metric_at(), and that's a
	 * bit ridiculous. Yes, the TempoMetric returned here can be used to
	 * change the map, and that's bad, but the non-const propagation is
	 * worse.
	 */

	return TempoMetric (*const_cast<TempoPoint*>(&*prev_t), *const_cast<MeterPoint*> (&*prev_m));
}

bool
TempoMap::set_ramped (TempoPoint & tp, bool yn)
{
	Rampable & r (tp);
	bool ret = r.set_ramped (yn);
	if (ret) {
		reset_starting_at (tp.sclock());
	}
	return ret;
}

#if 0
bool
TempoMap::twist_tempi (TempoSection* ts, const Tempo& bpm, const framepos_t frame, const framepos_t end_frame)
{
	TempoSection* next_t = 0;
	TempoSection* next_to_next_t = 0;
	Metrics future_map;
	bool can_solve = false;

	/* minimum allowed measurement distance in frames */
	framepos_t const min_dframe = 2;

	{
		if (!ts) {
			return false;
		}

		TempoSection* tempo_copy = copy_metrics_and_point (_metrics, future_map, ts);
		TempoSection* prev_to_prev_t = 0;
		const frameoffset_t fr_off = end_frame - frame;

		if (!tempo_copy) {
			return false;
		}

		if (tempo_copy->pulse() > 0.0) {
			prev_to_prev_t = const_cast<TempoSection*>(&tempo_section_at_minute_locked (future_map, minute_at_frame (tempo_copy->frame() - 1)));
		}

		for (Metrics::const_iterator i = future_map.begin(); i != future_map.end(); ++i) {
			if ((*i)->is_tempo() && (*i)->minute() >  tempo_copy->minute()) {
				next_t = static_cast<TempoSection*> (*i);
				break;
			}
		}

		if (!next_t) {
			return false;
		}

		for (Metrics::const_iterator i = future_map.begin(); i != future_map.end(); ++i) {
			if ((*i)->is_tempo() && (*i)->minute() >  next_t->minute()) {
				next_to_next_t = static_cast<TempoSection*> (*i);
				break;
			}
		}

		if (!next_to_next_t) {
			return false;
		}

		double prev_contribution = 0.0;

		if (next_t && prev_to_prev_t && prev_to_prev_t->type() == TempoSection::Ramp) {
			prev_contribution = (tempo_copy->frame() - prev_to_prev_t->frame()) / (double) (next_t->frame() - prev_to_prev_t->frame());
		}

		const frameoffset_t tempo_copy_frame_contribution = fr_off - (prev_contribution * (double) fr_off);


		framepos_t old_tc_minute = tempo_copy->minute();
		double old_next_minute = next_t->minute();
		double old_next_to_next_minute = next_to_next_t->minute();

		double new_bpm;
		double new_next_bpm;
		double new_copy_end_bpm;

		if (frame > tempo_copy->frame() + min_dframe && (frame + tempo_copy_frame_contribution) > tempo_copy->frame() + min_dframe) {
			new_bpm = tempo_copy->note_types_per_minute() * ((frame - tempo_copy->frame())
										       / (double) (end_frame - tempo_copy->frame()));
		} else {
			new_bpm = tempo_copy->note_types_per_minute();
		}

		/* don't clamp and proceed here.
		   testing has revealed that this can go negative,
		   which is an entirely different thing to just being too low.
		*/
		if (new_bpm < 0.5) {
			return false;
		}

		new_bpm = min (new_bpm, (double) 1000.0);

		tempo_copy->set_note_types_per_minute (new_bpm);
		if (tempo_copy->type() == TempoSection::Constant) {
			tempo_copy->set_end_note_types_per_minute (new_bpm);
		}

		recompute_tempi (future_map);

		if (check_solved (future_map)) {

			if (!next_t) {
				return false;
			}

			ts->set_note_types_per_minute (new_bpm);
			if (ts->type() == TempoSection::Constant) {
				ts->set_end_note_types_per_minute (new_bpm);
			}

			recompute_map (_metrics);

			can_solve = true;
		}

		if (next_t->type() == TempoSection::Constant || next_t->c() == 0.0) {
			if (frame > tempo_copy->frame() + min_dframe && end_frame > tempo_copy->frame() + min_dframe) {

				new_next_bpm = next_t->note_types_per_minute() * ((next_to_next_t->minute() - old_next_minute)
										  / (double) ((old_next_to_next_minute) - old_next_minute));

			} else {
				new_next_bpm = next_t->note_types_per_minute();
			}

			next_t->set_note_types_per_minute (new_next_bpm);
			recompute_tempi (future_map);

			if (check_solved (future_map)) {
				for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
					if ((*i)->is_tempo() && (*i)->minute() >  ts->minute()) {
						next_t = static_cast<TempoSection*> (*i);
						break;
					}
				}

				if (!next_t) {
					return false;
				}
				next_t->set_note_types_per_minute (new_next_bpm);
				recompute_map (_metrics);
				can_solve = true;
			}
		} else {
			double next_frame_ratio = 1.0;
			double copy_frame_ratio = 1.0;

			if (next_to_next_t) {
				next_frame_ratio = (next_to_next_t->minute() - old_next_minute) / (old_next_to_next_minute -  old_next_minute);

				copy_frame_ratio = ((old_tc_minute - next_t->minute()) / (double) (old_tc_minute - old_next_minute));
			}

			new_next_bpm = next_t->note_types_per_minute() * next_frame_ratio;
			new_copy_end_bpm = tempo_copy->end_note_types_per_minute() * copy_frame_ratio;

			tempo_copy->set_end_note_types_per_minute (new_copy_end_bpm);

			if (next_t->clamped()) {
				next_t->set_note_types_per_minute (new_copy_end_bpm);
			} else {
				next_t->set_note_types_per_minute (new_next_bpm);
			}

			recompute_tempi (future_map);

			if (check_solved (future_map)) {
				for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
					if ((*i)->is_tempo() && (*i)->minute() >  ts->minute()) {
						next_t = static_cast<TempoSection*> (*i);
						break;
					}
				}

				if (!next_t) {
					return false;
				}

				if (next_t->clamped()) {
					next_t->set_note_types_per_minute (new_copy_end_bpm);
				} else {
					next_t->set_note_types_per_minute (new_next_bpm);
				}

				ts->set_end_note_types_per_minute (new_copy_end_bpm);
				recompute_map (_metrics);
				can_solve = true;
			}
		}
	}

	Metrics::const_iterator d = future_map.begin();
	while (d != future_map.end()) {
		delete (*d);
		++d;
	}

	MetricPositionChanged (PropertyChange ()); // Emit Signal

	return can_solve;
}

#endif

void
TempoMap::MementoBinder::set_state (XMLNode const & node, int version) const
{
	/* fetch a writable copy of this thread's tempo map */
	TempoMap::SharedPtr map (write_copy());
	/* change the state of the copy */
	map->set_state (node, version);
	/* do the update step of RCU */
	update (map);
	/* now update this thread's view of the current tempo map */
	fetch ();
}

	
