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
#include <vector>

#include <inttypes.h>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/enumwriter.h"
#include "pbd/failed_constructor.h"
#include "pbd/stacktrace.h"

#include "temporal/debug.h"
#include "temporal/tempo.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace Temporal;
using std::cerr;
using std::cout;
using std::endl;
using Temporal::superclock_t;

std::string Tempo::xml_node_name = X_("Tempo");
std::string Meter::xml_node_name = X_("Meter");

SerializedRCUManager<TempoMap> TempoMap::_map_mgr (0);
thread_local TempoMap::SharedPtr TempoMap::_tempo_map_p;
PBD::Signal0<void> TempoMap::MapChanged;

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
	if (_map->time_domain() == AudioTime) {
		return timepos_t::from_superclock (sclock());
	}

	return timepos_t (beats());
}

Tempo::Tempo (XMLNode const & node)
{
	assert (node.name() == xml_node_name);


	node.get_property (X_("npm"), _npm);
	node.get_property (X_("enpm"), _enpm);

	_superclocks_per_note_type = double_npm_to_scpn (_npm);
	_end_superclocks_per_note_type = double_npm_to_scpn (_enpm);
	_super_note_type_per_second = double_npm_to_snps (_npm);
	_end_super_note_type_per_second = double_npm_to_snps (_enpm);

	if (!node.get_property (X_("note-type"), _note_type)) {
		throw failed_constructor ();
	}
	if (!node.get_property (X_("type"), _type)) {
		throw failed_constructor ();
	}
	if (!node.get_property (X_("active"), _active)) {
		throw failed_constructor ();
	}
	if (!node.get_property (X_("locked-to-meter"), _locked_to_meter)) {
		_locked_to_meter = true;
	}
	if (!node.get_property (X_("clamped"), _clamped)) {
		_clamped = false;
	}
}

void
Tempo::set_ramped (bool yn)
{
	_type = (yn ? Ramped : Constant);
}

void
Tempo::set_end (uint64_t n, superclock_t s)
{
	_end_super_note_type_per_second = n;
	_end_superclocks_per_note_type = s;
}

void
Tempo::set_clamped (bool yn)
{
	_clamped = yn;
}

XMLNode&
Tempo::get_state () const
{
	XMLNode* node = new XMLNode (xml_node_name);

	node->set_property (X_("npm"), note_types_per_minute());
	node->set_property (X_("enpm"), end_note_types_per_minute());
	node->set_property (X_("note-type"), note_type());
	node->set_property (X_("type"), type());
	node->set_property (X_("active"), active());
	node->set_property (X_("locked-to-meter"), _locked_to_meter);
	node->set_property (X_("clamped"), _clamped);

	return *node;
}

int
Tempo::set_state (XMLNode const & node, int /*version*/)
{
	if (node.name() != xml_node_name) {
		return -1;
	}

	node.get_property (X_("npm"), _npm);
	node.get_property (X_("enpm"), _enpm);

	_superclocks_per_note_type = double_npm_to_scpn (_npm);
	_end_superclocks_per_note_type = double_npm_to_scpn (_enpm);
	_super_note_type_per_second = double_npm_to_snps (_npm);
	_end_super_note_type_per_second = double_npm_to_snps (_enpm);

	node.get_property (X_("note-type"), _note_type);
	node.get_property (X_("type"), _type);
	node.get_property (X_("active"), _active);

	if (!node.get_property (X_("locked-to-meter"), _locked_to_meter)) {
		_locked_to_meter = true;
	}

	if (!node.get_property (X_("clamped"), _clamped)) {
		_clamped = false;
	}

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
	Beats b (bbt.beats, bbt.ticks);
	Beats half (Beats::ticks (Beats::PPQN + ((_divisions_per_bar * Beats::PPQN / 2))));

	if (b >= half) {
		return BBT_Time (bbt.bars+1, 1, 0);
	}

	return BBT_Time (bbt.bars, 1, 0);
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
	: Point (map, node)
	, Tempo (node)
	, _omega (0)
{
	node.get_property (X_("omega"), _omega);
}

/* To understand the math(s) behind ramping, see the file doc/tempo.{pdf,tex}
 */

void
TempoPoint::compute_omega (TempoPoint const & next)
{
	cerr << "********* COMPUTING OMEGA for " << *this << " next = " << next << endl;

	superclock_t end_scpqn;

	if (_clamped) {
		end_scpqn = end_superclocks_per_quarter_note();
	} else {
		end_scpqn = next.superclocks_per_quarter_note ();
	}

	const DoubleableBeats quarter_duration = next.beats() - beats();

	if ((_type == Constant) || (superclocks_per_quarter_note () == end_scpqn)) {
		cerr << " no ramp, my start == " << superclocks_per_quarter_note() << " next end " << end_scpqn << " type " << enum_2_string (_type) << " clamped ? " << _clamped << endl;
		_omega = 0.0;
		return;
	}

	_omega = ((1.0/end_scpqn) - (1.0/superclocks_per_quarter_note())) / quarter_duration.to_double();

	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("computed omega = %1%2 dur was %3\n", std::setprecision(12),_omega, DoubleableBeats (quarter_duration).to_double()));
}

superclock_t
TempoPoint::superclock_at (Temporal::Beats const & qn) const
{
	if (qn == _quarters) {
		return _sclock;
	}

	if (qn < Beats()) {
		/* negative */
		assert (_quarters == Beats());
	} else {
		/* positive */
		assert (qn >= _quarters);
	}

	if (!actually_ramped()) {
		/* not ramped, use linear */
		const Beats delta = qn - _quarters;
		const superclock_t spqn = superclocks_per_quarter_note ();
		return _sclock + (spqn * delta.get_beats()) + int_div_round ((spqn * delta.get_ticks()), superclock_t (Temporal::ticks_per_beat));
	}

	return _sclock + llrint (log1p (superclocks_per_quarter_note() * _omega * DoubleableBeats (qn - _quarters).to_double()) / _omega);
}

superclock_t
TempoPoint::superclocks_per_note_type_at (timepos_t const &pos) const
{
	if (!actually_ramped()) {
		return _superclocks_per_note_type;
	}

	return _superclocks_per_note_type * exp (-_omega * (pos.superclocks() - sclock()));
}

Temporal::Beats
TempoPoint::quarters_at_superclock (superclock_t sc) const
{
	/* catch a special case. The maximum superclock_t value cannot be
	   converted into a 32bit beat + 32 bit tick value for common tempos.
	   Obviously, values less than this can also cause overflow, but are
	   unlikely to be encountered.

	   A longer term/big picture solution for this is likely required in
	   order to deal with longer sessions. Still, even at 300bpm, a 32 bit
	   integer should cover 165 days. The problem is that a 62 bit (int62_t)
	   superclock counter can cover 105064 days, so the theoretical
	   potential for errors here is real.
	*/

	if (sc >= int62_t::max) {
		return std::numeric_limits<Beats>::max();
	}

	if (!actually_ramped()) {

		// assert (sc >= _sclock);
		superclock_t sc_delta = sc - _sclock;

		/* convert sc into superbeats, given that sc represents some number of seconds */
		const superclock_t whole_seconds = sc_delta / superclock_ticks_per_second;
		const superclock_t remainder = sc_delta - (whole_seconds * superclock_ticks_per_second);

		const int64_t supernotes = ((_super_note_type_per_second) * whole_seconds) + int_div_round (superclock_t ((_super_note_type_per_second) * remainder), superclock_ticks_per_second);
		/* multiply after divide to reduce overflow risk */
		const int64_t superbeats = int_div_round (supernotes, (superclock_t) _note_type) * 4;

		/* convert superbeats to beats:ticks */
		int32_t b;
		int32_t t;

		Tempo::superbeats_to_beats_ticks (superbeats, b, t);

		if (sc < _sclock) {
			std::cout << string_compose ("%8 => \nsc %1 delta %9 = %2 secs rem = %3 rem snotes %4 sbeats = %5 => %6 : %7\n", sc, whole_seconds, remainder, supernotes, superbeats, b , t, *this, sc_delta);
		}

		DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("%8 => \nsc %1 delta %9 = %2 secs rem = %3 rem snotes %4 sbeats = %5 => %6 : %7\n", sc, whole_seconds, remainder, supernotes, superbeats, b , t, *this, sc_delta));

		return _quarters + Beats (b, t);
	}

	const double b = (exp (_omega * (sc - _sclock)) - 1) / (superclocks_per_quarter_note() * _omega);
	return _quarters + Beats::from_double (b);
}

MeterPoint::MeterPoint (TempoMap const & map, XMLNode const & node)
	: Point (map, node)
	, Meter (node)
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
TempoMetric::bbt_at (timepos_t const & pos) const
{
	if (pos.is_beats()) {
		return bbt_at (pos.beats());
	}

	superclock_t sc = pos.superclocks();

	const Beats dq = _tempo->quarters_at_superclock (sc) - _meter->beats();

	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("qn @ %1 = %2, meter @ %3 , delta %4\n", sc, _tempo->quarters_at_superclock (sc), _meter->beats(), dq));

	/* dq is delta in quarters (beats). Convert to delta in note types of
	   the current meter, which we'll call "grid"
	*/

	const int64_t note_value_count = int_div_round (dq.get_beats() * _meter->note_value(), 4);

	/* now construct a BBT_Offset using the count in grid units */

	const BBT_Offset bbt_offset (0, note_value_count, dq.get_ticks());

	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("BBT offset from meter @ %1: %2\n", _meter->bbt(), bbt_offset));
	return _meter->bbt_add (_meter->bbt(), bbt_offset);
}

superclock_t
TempoMetric::superclock_at (BBT_Time const & bbt) const
{
	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("get quarters for %1 = %2\n", bbt, _meter->quarters_at (bbt)));
	return _tempo->superclock_at (_meter->quarters_at (bbt));
}

MusicTimePoint::MusicTimePoint (TempoMap const & map, XMLNode const & node)
	: Point (map, node)
	, TempoPoint (map, *node.child (Tempo::xml_node_name.c_str()))
	, MeterPoint (map, *node.child (Meter::xml_node_name.c_str()))
{
}

XMLNode&
MusicTimePoint::get_state () const
{
	XMLNode* node = new XMLNode (X_("MusicTime"));

	Point::add_state (*node);

	node->add_child_nocopy (Tempo::get_state());
	node->add_child_nocopy (Meter::get_state());

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
	: _time_domain (AudioTime)
{
	TempoPoint* tp = new TempoPoint (*this, initial_tempo, 0, Beats(), BBT_Time());
	MeterPoint* mp = new MeterPoint (*this, initial_meter, 0, Beats(), BBT_Time());

	_tempos.push_back   (*tp);
	_meters.push_back   (*mp);

	_points.push_back (*tp);
	_points.push_back (*mp);
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
	copy_points (other);
}

TempoMap&
TempoMap::operator= (TempoMap const & other)
{
	_time_domain = other.time_domain();
	copy_points (other);
	return *this;
}

void
TempoMap::copy_points (TempoMap const & other)
{
	std::vector<Point*> p;

	p.reserve (other._meters.size() + other._tempos.size() + other._bartimes.size());

	for (Meters::const_iterator m = other._meters.begin(); m != other._meters.end(); ++m) {
		MeterPoint* mp = new MeterPoint (*m);
		_meters.push_back (*mp);
		p.push_back (mp);
	}

	for (Tempos::const_iterator t = other._tempos.begin(); t != other._tempos.end(); ++t) {
		TempoPoint* tp = new TempoPoint (*t);
		_tempos.push_back (*tp);
		p.push_back (tp);
	}

	for (MusicTimes::const_iterator mt = other._bartimes.begin(); mt != other._bartimes.end(); ++mt) {
		MusicTimePoint* mtp = new MusicTimePoint (*mt);
		_bartimes.push_back (*mtp);
		p.push_back (mtp);
	}

	sort (p.begin(), p.end(), Point::ptr_sclock_comparator());

	for (std::vector<Point*>::iterator pi = p.begin(); pi != p.end(); ++pi) {
		_points.push_back (**pi);
	}
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
			t->set_beats (t->quarters_at_superclock (t->sclock()));
		}
		for (Meters::iterator m = _meters.begin(); m != _meters.end(); ++m) {
			m->set_beats (m->quarters_at_superclock (m->sclock()));
		}
	}
#endif

	_time_domain = td;
}

MeterPoint*
TempoMap::add_meter (MeterPoint* mp)
{
	Meters::iterator m;
	Points::iterator p;
	const superclock_t sclock_limit = mp->sclock();
	const Beats beats_limit = mp->beats ();

	switch (time_domain()) {
	case AudioTime:
		for (m = _meters.begin(); m != _meters.end() && m->sclock() < sclock_limit; ++m);
		for (p = _points.begin(); p != _points.end() && p->sclock() < sclock_limit; ++p);
		break;
	case BeatTime:
		for (m = _meters.begin(); m != _meters.end() && m->beats() < beats_limit; ++m);
		for (p = _points.begin(); p != _points.end() && p->beats() < beats_limit; ++p);
		break;
	}

	bool replaced = false;
	MeterPoint* ret = 0;

	if (m != _meters.end()) {
		if (m->sclock() == sclock_limit) {
			/* overwrite Meter part of this point */
			*((Meter*)&(*m)) = *mp;
			delete mp;
			ret = &(*m);
			replaced = true;
		}
	}

	if (!replaced) {
		ret = &(*(_meters.insert (m, *mp)));
		_points.insert (p, *mp);
	}

	reset_starting_at (sclock_limit);

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
	return set_tempo (t, timepos_t (quarters_at (bbt)));
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

		TempoMetric metric (metric_at (on_beat, false));

		bbt = metric.bbt_at (on_beat);
		sc = metric.superclock_at (on_beat);

		TempoPoint* tp = new TempoPoint (*this, t, sc, on_beat, bbt);
		ret = add_tempo (tp);

	} else {

		Beats beats;
		BBT_Time bbt;
		superclock_t sc = time.superclocks();

		TempoMetric tm (metric_at (sc, false));

		/* tempo changes must be on beat */

		beats = tm.quarters_at_superclock (sc).round_to_beat ();
		bbt = tm.bbt_at (beats);

		/* recompute superclock position of rounded beat */
		sc = tm.superclock_at (beats);

		TempoPoint* tp = new TempoPoint (*this, t, sc, beats, bbt);
		ret = add_tempo (tp);

	}

	dump (cerr);

	return *ret;
}

TempoPoint*
TempoMap::add_tempo (TempoPoint * tp)
{
	Tempos::iterator t;
	Points::iterator p;
	const superclock_t sclock_limit = tp->sclock();
	const Beats beats_limit = tp->beats ();

	switch (time_domain()) {
	case AudioTime:
		for (t = _tempos.begin(); t != _tempos.end() && t->sclock() < sclock_limit; ++t);
		for (p = _points.begin(); p != _points.end() && p->sclock() < sclock_limit; ++p);
		break;
	case BeatTime:
		for (t = _tempos.begin(); t != _tempos.end() && t->beats() < beats_limit; ++t);
		for (p = _points.begin(); p != _points.end() && p->beats() < beats_limit; ++p);
		break;
	}

	bool replaced = false;
	TempoPoint* ret = 0;

	if (t != _tempos.end()) {
		if (t->sclock() == sclock_limit) {
			/* overwrite Tempo part of this point */
			*((Tempo*)&(*t)) = *tp;
			delete tp;
			ret = &(*t);
			DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("overwrote old tempo with %1\n", tp));
			replaced = true;
		}
	}

	if (!replaced) {
		t = _tempos.insert (t, *tp);
		p = _points.insert (p, *tp);
		ret = &*t;
		DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("inserted tempo %1\n", tp));
	}

	/* t is guaranteed not to be _tempos.end() : it was either the
	 * TempoPoint we overwrote, or its the one we inserted.
	 */

	assert (t != _tempos.end());
	assert (p != _points.end());

	Tempos::iterator nxt = t;
	++nxt;

	reset_starting_at (sclock_limit);

	return ret;
}

void
TempoMap::remove_tempo (TempoPoint const & tp)
{
	superclock_t sc (tp.sclock());
	Tempos::iterator t;

	assert (_tempos.size() > 1);

	/* the argument is likely to be a Point-derived object that doesn't
	 * actually exist in this TempoMap, since the caller called
	 * TempoMap::write_copy() in order to perform an RCU operation, but
	 * will be removing an element known from the original map.
	 *
	 * However, since we do not allow points of the same type (Tempo,
	 * Meter, BarTime) at the same time, we can effectively search here
	 * using what is effectively a duple of (type,time) for the
	 * comparison.
	 *
	 * Once/if found, we will have a pointer to the actual Point-derived
	 * object in this TempoMap, and we can then remove that from the
	 * _points list.
	 */

	for (t = _tempos.begin(); t != _tempos.end() && t->sclock() < tp.sclock(); ++t);

	if (t == _tempos.end()) {
		/* not found */
		return;
	}

	if (t->sclock() != tp.sclock()) {
		/* error ... no tempo point at the time of tp */
		return;
	}

	Tempos::iterator nxt = _tempos.begin();
	Tempos::iterator prev = _tempos.end();

	if (t != _tempos.end()) {
		nxt = t;
		++nxt;
	}

	if (t != _tempos.begin()) {
		prev = t;
		--prev;
	}

	const bool was_end = (nxt == _tempos.end());

	_tempos.erase (t);
	remove_point (*t);

	if (prev != _tempos.end() && was_end) {
		Rampable& r (*prev);
		r.set_ramped (false);
	} else {
		reset_starting_at (sc);
	}
}

MusicTimePoint &
TempoMap::set_bartime (BBT_Time const & bbt, timepos_t const & pos)
{
	MusicTimePoint * ret;

	assert (pos.time_domain() == AudioTime);

	superclock_t sc (pos.superclocks());
	TempoMetric metric (metric_at (sc));
	MusicTimePoint* tp = new MusicTimePoint (*this, sc, metric.quarters_at_superclock (sc), bbt, metric.tempo(), metric.meter());

	ret = add_or_replace_bartime (*tp);

	return *ret;
}

MusicTimePoint*
TempoMap::add_or_replace_bartime (MusicTimePoint & tp)
{
	MusicTimes::iterator m;
	Points::iterator p;
	superclock_t sclock_limit = tp.sclock();

	for (m = _bartimes.begin(); m != _bartimes.end() && m->sclock() < sclock_limit; ++m);
	for (p = _points.begin(); p != _points.end() && p->sclock() < sclock_limit; ++p);

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
		_points.insert (p, tp);

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
	superclock_t sc (tp.sclock());
	MusicTimes::iterator m;

	/* the argument is likely to be a Point-derived object that doesn't
	 * actually exist in this TempoMap, since the caller called
	 * TempoMap::write_copy() in order to perform an RCU operation, but
	 * will be removing an element known from the original map.
	 *
	 * However, since we do not allow points of the same type (Tempo,
	 * Meter, BarTime) at the same time, we can effectively search here
	 * using what is effectively a duple of (type,time) for the
	 * comparison.
	 *
	 * Once/if found, we will have a pointer to the actual Point-derived
	 * object in this TempoMap, and we can then remove that from the
	 * _points list.
	 */

	for (m = _bartimes.begin(); m != _bartimes.end() && m->sclock() < tp.sclock(); ++m);

	if (m->sclock() != tp.sclock()) {
		/* error ... no music time point at the time of tp */
		return;
	}

	_bartimes.erase (m);
	remove_point (*m);
	reset_starting_at (sc);
}

void
TempoMap::remove_point (Point const & point)
{
	Points::iterator p;
	Point const * tpp (&point);

	/* note that the point passed here must be an element of the _points
	 * list, which is not true for the point passed to the callees
	 * (remove_tempo(), remove_meter(), remove_bartime().
	 *
	 * in those methods, we effectively search for a match on a duple of
	 * (type, time), but here we are comparing pointer addresses.
	 */

	for (p = _points.begin(); p != _points.end(); ++p) {
		if (&(*p) == tpp) {
			// XXX need to fix this leak delete tpp;
			_points.erase (p);
			break;
		}
	}
}

void
TempoMap::reset_starting_at (superclock_t sc)
{
	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("reset starting at %1\n", sc));
	cerr << "RESET starting at " << sc << endl;
	dump (cerr);

	assert (!_tempos.empty());
	assert (!_meters.empty());

	TempoPoint* current_tempo;
	MeterPoint* current_meter;

	/* final argument = false means "return the _points iterator
	   corresponding to the latter of the tempo or meter point used."
	   (i.e. the point *at* the latter of the two, not the one after it)
	*/
	Points::iterator p = get_tempo_and_meter (current_tempo, current_meter, sc, true, false);

	TempoMetric metric (*current_tempo, *current_meter);
	cerr << "We begin with metric = " << metric << endl;

	Tempos::iterator nxt_tempo;

	for (nxt_tempo = _tempos.begin(); nxt_tempo != _tempos.end(); ++nxt_tempo) {
		if (*nxt_tempo == *current_tempo) {
			break;
		}
	}

	assert (nxt_tempo != _tempos.end());
	++nxt_tempo;

	if (nxt_tempo != _tempos.end() && current_tempo->sclock() != sc) {
		++nxt_tempo;
	}

	if (nxt_tempo != _tempos.end()) {
		cerr << ":::: next tempo is " << *nxt_tempo << endl;
	} else {
		cerr << ";;;; next tempo at end\n";
	}

	TempoPoint*     tp;
	MeterPoint*     mp;
	MusicTimePoint* mtp;

	cerr << "after all that, p @ end ? " << (p == _points.end()) << endl;

	for (; p != _points.end(); ++p) {

		bool reset_metric = false;
		bool reset_point = false;
		bool is_tempo = false;

		cerr << "Now looking at " << *p << endl;

		if ((mtp = dynamic_cast<MusicTimePoint*> (&*p)) != 0) {

			/* nothing to do here. We do not reset music time (bar
			 * time) points since everything their position is set
			 * by the user. The only time we would change them is
			 * if we alter the time domain of the tempo map.
			 */

			cerr << "MTP!\n";

		} else if ((tp = dynamic_cast<TempoPoint*> (&*p)) != 0) {
			reset_metric = true;
			reset_point = true;
			is_tempo = true;
			current_tempo = tp;
			cerr << "Tempo! " << *tp << endl;
		} else if ((mp = dynamic_cast<MeterPoint*> (&*p)) != 0) {
			reset_metric = true;
			reset_point = true;
			current_meter = mp;
			cerr << "Meter! " << *mp << endl;
		}

		if (is_tempo) {
			if (tp->ramped() && nxt_tempo != _tempos.end()) {
				tp->compute_omega (*nxt_tempo);
			}
			++nxt_tempo;
		}

		if (reset_point) {
			superclock_t sc = metric.superclock_at (p->bbt());
			DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("\tbased on %1 move to %2,%3\n", p->bbt(), sc, p->beats()));
			p->set (sc, p->beats(), p->bbt());
		}

		if (reset_metric) {
			metric = TempoMetric (*current_tempo, *current_meter);
		}
	}

	cerr << "RESET DONE\n\n\n";
}

bool
TempoMap::move_meter (MeterPoint const & mp, timepos_t const & when, bool push)
{
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

		/* Find TempoMetric *prior* to the intended new location, * using superclock position */

		for (t = _tempos.begin(), prev_t = _tempos.end(); t != _tempos.end() && t->sclock() < sc; ++t) { prev_t = t; }
		for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end() && m->sclock() < sc && *m != mp; ++m) { prev_m = m; }
		assert (prev_m != _meters.end());
		if (prev_t == _tempos.end()) { prev_t = _tempos.begin(); }
		TempoMetric metric (*prev_t, *prev_m);

		/* check the duration of 1 bar here. If we're not more than
		 * half-way to the next bar (in whatever the appropriate
		 * direction is), don't move
		 */

		const superclock_t one_bar = metric.superclocks_per_bar ();
		if (abs (sc - mp.sclock()) < one_bar / 2) {
			return false;
		}

		/* compute the BBT at the given superclock position, given the prior TempoMetric */

		bbt = metric.bbt_at (timepos_t::from_superclock (sc));

		/* meter changes must fall on a bar change */

		if (round_up) {
			bbt = metric.meter().round_up_to_bar (bbt);
		} else {
			bbt = metric.meter().round_down_to_bar (bbt);
		}

		/* Repeat using the computed (new) BBT location */

		for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end() && m->bbt() < bbt && *m != mp; ++m) {prev_m = m; }
		for (t = _tempos.begin(), prev_t = _tempos.end(); t != _tempos.end() && t->bbt() < bbt; ++t) { prev_t = t; }
		if (prev_m == _meters.end()) {
			/* given position is going to put us over the initial
			meter. Not allowed for a meter move.
			*/
			return false;
		}
		if (prev_t == _tempos.end()) { prev_t = _tempos.begin(); }
		metric = TempoMetric (*prev_t, *prev_m);

		/* recompute the superclock position of the new BBT position,
		 * since this is what we'll use to set the meter point.
		 */


		sc = metric.superclock_at (bbt);

		/* check to see if there's already a meter point at that location */

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

	return true;
}

bool
TempoMap::move_tempo (TempoPoint const & tp, timepos_t const & when, bool push)
{
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
		beats = metric.quarters_at_superclock (sc);
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

	/* recompute 3 domain positions for everything after this */
	reset_starting_at (std::min (sc, old_sc));

	return true;
}

MeterPoint &
TempoMap::set_meter (Meter const & m, timepos_t const & time)
{
	MeterPoint * ret = 0;

	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("Set meter @ %1 to %2\n", time, m));

	if (time.is_beats()) {

		Beats beats (time.beats());
		TempoMetric metric (metric_at (beats));

		/* meter changes are required to be on-bar */

		BBT_Time rounded_bbt = metric.bbt_at (beats);
		rounded_bbt = metric.round_to_bar (rounded_bbt);

		const Beats rounded_beats = metric.quarters_at (rounded_bbt);
		const superclock_t sc = metric.superclock_at (rounded_beats);

		MeterPoint* mp = new MeterPoint (*this, m, sc, rounded_beats, rounded_bbt);

		ret = add_meter (mp);

	} else {

		superclock_t sc (time.superclocks());
		Beats beats;
		BBT_Time bbt;

		TempoMetric metric (metric_at (sc));

		/* meter changes must be on bar */

		bbt = metric.bbt_at (time);
		bbt = metric.round_to_bar (bbt);

		/* compute beat position */
		beats = metric.quarters_at (bbt);

		/* recompute superclock position of bar-rounded position */
		sc = metric.superclock_at (beats);

		MeterPoint* mp = new MeterPoint (*this, m, sc, beats, bbt);

		ret = add_meter (mp);
	}

	return *ret;
}

MeterPoint &
TempoMap::set_meter (Meter const & t, BBT_Time const & bbt)
{
	return set_meter (t, timepos_t (quarters_at (bbt)));
}

void
TempoMap::remove_meter (MeterPoint const & mp)
{
	superclock_t sc = mp.sclock();

	/* the argument is likely to be a Point-derived object that doesn't
	 * actually exist in this TempoMap, since the caller called
	 * TempoMap::write_copy() in order to perform an RCU operation, but
	 * will be removing an element known from the original map.
	 *
	 * However, since we do not allow points of the same type (Tempo,
	 * Meter, BarTime) at the same time, we can effectively search here
	 * using what is effectively a duple of (type,time) for the
	 * comparison.
	 *
	 * Once/if found, we will have a pointer to the actual Point-derived
	 * object in this TempoMap, and we can then remove that from the
	 * _points list.
	 */

	Meters::iterator m = std::upper_bound (_meters.begin(), _meters.end(), mp, Point::sclock_comparator());

	if (m->sclock() != mp.sclock()) {
		/* error ... no meter point at the time of mp */
		return;
	}

	_meters.erase (m);
	remove_point (*m);
	reset_starting_at (sc);
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
	return metric_at (s).bbt_at (timepos_t::from_superclock (s));
}

Temporal::BBT_Time
TempoMap::bbt_at (Temporal::Beats const & qn) const
{
	return metric_at (qn).bbt_at (qn);
}

#if 0
samplepos_t
TempoMap::sample_at (Temporal::Beats const & qn) const
{
	return superclock_to_samples (metric_at (qn).superclock_at (qn), TEMPORAL_SAMPLE_RATE);
}

samplepos_t
TempoMap::sample_at (Temporal::BBT_Time const & bbt) const
{
	return samples_to_superclock (metric_at (bbt).superclock_at (bbt), TEMPORAL_SAMPLE_RATE);
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
	return metric_at (qn).superclock_at (qn);
}

superclock_t
TempoMap::superclock_at (Temporal::BBT_Time const & bbt) const
{
	return metric_at (bbt).superclock_at (bbt);
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

#define S2Sc(s) (samples_to_superclock ((s), TEMPORAL_SAMPLE_RATE))
#define Sc2S(s) (superclock_to_samples ((s), TEMPORAL_SAMPLE_RATE))

/** Count the number of beats that are equivalent to distance when going forward,
    starting at pos.
*/
Temporal::Beats
TempoMap::scwalk_to_quarters (superclock_t pos, superclock_t distance) const
{
	TempoMetric first (metric_at (pos));
	TempoMetric last (metric_at (pos+distance));
	Temporal::Beats a = first.quarters_at_superclock (pos);
	Temporal::Beats b = last.quarters_at_superclock (pos+distance);
	return b - a;
}

Temporal::Beats
TempoMap::scwalk_to_quarters (Temporal::Beats const & pos, superclock_t distance) const
{
	/* XXX this converts from beats to superclock and back to beats... which is OK (reversible) */
	superclock_t s = metric_at (pos).superclock_at (pos);
	s += distance;
	return metric_at (s).quarters_at_superclock (s);

}

Temporal::Beats
TempoMap::bbtwalk_to_quarters (Beats const & pos, BBT_Offset const & distance) const
{
	return quarters_at (bbt_walk (bbt_at (pos), distance)) - pos;
}

void
TempoMap::sample_rate_changed (samplecnt_t new_sr)
{
	const double ratio = new_sr / (double) TEMPORAL_SAMPLE_RATE;

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
	ostr << "\n\nTEMPO MAP @ " << this << ":\n";
	for (Tempos::const_iterator t = _tempos.begin(); t != _tempos.end(); ++t) {
		ostr << &*t << ' ' << *t << endl;
	}

	for (Meters::const_iterator m = _meters.begin(); m != _meters.end(); ++m) {
		ostr << &*m << ' ' << *m << endl;
	}

	for (MusicTimes::const_iterator m = _bartimes.begin(); m != _bartimes.end(); ++m) {
		ostr << &*m << ' ' << *m << endl;
	}
	ostr << "... all points ...\n";
	for (Points::const_iterator p = _points.begin(); p != _points.end(); ++p) {
		ostr << &*p << ' ' << *p << endl;
	}
	ostr << "------------\n\n\n";
}

template<class const_traits_t>  typename const_traits_t::iterator_type
TempoMap::_get_tempo_and_meter (typename const_traits_t::tempo_point_type & tp,
                                typename const_traits_t::meter_point_type & mp,
                                typename const_traits_t::time_reference (Point::*method)() const,
                                typename const_traits_t::time_type arg,
                                typename const_traits_t::iterator_type begini,
                                typename const_traits_t::iterator_type endi,
                                typename const_traits_t::tempo_point_type tstart,
                                typename const_traits_t::meter_point_type mstart,
                                bool can_match, bool ret_iterator_after_not_at) const
{
	typename const_traits_t::iterator_type p;
	typename const_traits_t::iterator_type last_used = endi;
	bool tempo_done = false;
	bool meter_done = false;

	assert (!_tempos.empty());
	assert (!_meters.empty());
	assert (!_points.empty());

	/* If the starting position is the beginning of the timeline (indicated
	 * by the default constructor value for the time_type (superclock_t,
	 * Beats, BBT_Time), then we are always allowed to use the tempo &
	 * meter at that position.
	 *
	 * Without this, it would be necessary to special case "can_match" in
	 * the caller if the start is "zero". Instead we do that here, since
	 * common cases (e.g. ::get_grid()) will use can_match = false, but may
	 * pass in a zero start point.
	 */

	can_match = (can_match || arg == typename const_traits_t::time_type ());

	for (tp = tstart, mp = mstart, p = begini; p != endi; ++p) {

		typename const_traits_t::tempo_point_type tpp;
		typename const_traits_t::meter_point_type mpp;

		if (!tempo_done && (tpp = dynamic_cast<typename const_traits_t::tempo_point_type> (&(*p))) != 0) {
			if ((can_match && (((*p).*method)() > arg)) || (!can_match && (((*p).*method)() >= arg))) {
				tempo_done = true;
			} else {
				tp = tpp;
				last_used = p;
			}
		}

		if (!meter_done && (mpp = dynamic_cast<typename const_traits_t::meter_point_type> (&(*p))) != 0) {
			if ((can_match && (((*p).*method)() > arg)) || (!can_match && (((*p).*method)() >= arg))) {
				meter_done = true;
			} else {
				mp = mpp;
				last_used = p;
			}
		}

		if (meter_done && tempo_done) {
			break;
		}
	}

	if (!tp || !mp) {
		return endi;
	}

	if (ret_iterator_after_not_at) {

		p = last_used;

		if (can_match) {
			while ((p != endi) && ((*p).*method)() <= arg) ++p;
		} else {
			while ((p != endi) && ((*p).*method)() < arg) ++p;
		}

		return p;
	}

	return last_used;
}

void
TempoMap::get_grid (TempoMapPoints& ret, superclock_t start, superclock_t end, uint32_t bar_mod)
{
	/* note: @param bar_mod is "bar modulo", and describes the N in "give
	   me every Nth bar". If the caller wants every 4th bar, bar_mod ==
	   4. If we want every point defined by the tempo note type (e.g. every
	   quarter not, then bar_mod is zero.
	*/

	assert (!_tempos.empty());
	assert (!_meters.empty());
	assert (!_points.empty());

	DEBUG_TRACE (DEBUG::Grid, string_compose (">>> GRID START %1 .. %2 (barmod = %3)\n", start, end, bar_mod));

	TempoPoint const * tp = 0;
	MeterPoint const * mp = 0;
	Points::const_iterator p;

	/* first task: get to the right starting point for the requested
	 * grid. if bar_mod is zero, then we'll start on the next beat after
	 * @param start. if bar_mod is non-zero, we'll start on the first bar
	 * after @param start. This bar position may or may not be a part of the
	 * grid, depending on whether or not it is a multiple of bar_mod.
	 *
	 * final argument = true means "return the iterator corresponding the
	 * point after the latter of the tempo/meter points"
	 */

	p = get_tempo_and_meter (tp, mp, start, true, true);
	TempoMetric metric = TempoMetric (*tp, *mp);

	DEBUG_TRACE (DEBUG::Grid, string_compose ("metric in effect at %1 = %2\n", start, metric));

	/* p now points to either the point *after* start, or the end of the
	 * _points list.
	 *
	 * metric is the TempoMetric that is in effect at start
	 */

	/* determine the BBT at start */

	BBT_Time bbt = metric.bbt_at (timepos_t::from_superclock (start));

	DEBUG_TRACE (DEBUG::Grid, string_compose ("start %1 is %2\n", start, bbt));

	if (bar_mod == 0) {

		/* round to next beat, then find the tempo/meter/bartime points
		 * in effect at that time.
		 */

		const BBT_Time new_bbt = metric.meter().round_up_to_beat (bbt);

		if (new_bbt != bbt) {

			bbt = new_bbt;

			/* rounded up, determine new starting superclock position */

			p = get_tempo_and_meter (tp, mp, bbt, false, true);

			metric = TempoMetric (*tp, *mp);

			DEBUG_TRACE (DEBUG::Grid, string_compose ("metric in effect(2) at %1 = %2\n", bbt, metric));

			/* recompute superclock position */

			superclock_t new_start = metric.superclock_at (bbt);

			DEBUG_TRACE (DEBUG::Grid, string_compose ("metric %1 says that %2 is at %3\n", metric, bbt, new_start));

			if (new_start < start) {
				DEBUG_TRACE (DEBUG::Grid, string_compose ("we've gone backwards, new is %1 start is %2\n", new_start, start));
				abort ();
			}

			start = new_start;

		} else {
			DEBUG_TRACE (DEBUG::Grid, string_compose ("%1 was on a beat, no rounding up necessary\n", bbt));
		}

	} else {

		BBT_Time bar = bbt.round_down_to_bar ();

		/* adjust to match bar_mod (i.e. we only want every 4th bar)
		 */

		if (bar_mod != 1) {
			bar.bars -= bar.bars % bar_mod;
			++bar.bars;
		}

		/* the rounding we've just done cannot change the meter in
		   effect, because it remains within the bar. But it could
		   change the tempo (which are only quantized to grid positions
		   within a bar). So if it has generated a new BBT time,
		   recompute the metric.
		*/

		if (bar != bbt) {

			bbt = bar;

			p = get_tempo_and_meter (tp, mp, bbt, true, true);
			metric = TempoMetric (*tp, *mp);

			DEBUG_TRACE (DEBUG::Grid, string_compose ("metric in effect(3) at %1 = %2\n", start, metric));
			start = metric.superclock_at (bbt);

		} else {
			DEBUG_TRACE (DEBUG::Grid, string_compose ("%1 was on a bar, no round down to bar necessary\n", bbt));
		}
	}

	/* at this point:
	 *
	 * - metric is a TempoMetric that describes the situation at start
	 * - p is an iterator pointin to either the end of the _points list, or
	 *   the next point in the list after start.
	 */

	DEBUG_TRACE (DEBUG::Grid, string_compose ("start filling points with start = %1 end = %2 with limit @ %3\n", start, end, *p));

	Temporal::Beats beats = metric.quarters_at_superclock (start);

	while (start < end) {

		DEBUG_TRACE (DEBUG::Grid, string_compose ("start %1 end %2 bbt %3 find first/limit with limit @ = %4\n", start, end, bbt, *p));

		const superclock_t limit = (p == _points.end()) ? end : std::min (p->sclock(), end);

		while (start < limit) {

			/* add point to grid, perhaps */

			if (bar_mod != 0) {
				if (bbt.is_bar() && (bar_mod == 1 || ((bbt.bars % bar_mod == 0)))) {
					ret.push_back (TempoMapPoint (*this, metric, start, beats, bbt));
					DEBUG_TRACE (DEBUG::Grid, string_compose ("G %1\t       %2\n", metric, ret.back()));
				} else {
					DEBUG_TRACE (DEBUG::Grid, string_compose ("-- skip %1 not on bar_mod %2\n", bbt, bar_mod));
				}
			} else {
				ret.push_back (TempoMapPoint (*this, metric, start, beats, bbt));
				DEBUG_TRACE (DEBUG::Grid, string_compose ("G %1\t       %2\n", metric, ret.back()));
			}

			superclock_t step;

			if (bar_mod == 0) {

				/* Advance beats by the meter note value size, and
				 * then recompute the BBT and superclock
				 * position corresponding to that musical time
				 */

				beats += metric.tempo().note_type_as_beats ();
				bbt = metric.bbt_at (beats);
				start = metric.superclock_at (beats);
				DEBUG_TRACE (DEBUG::Grid, string_compose ("step at %3 for note type was %1, now @ %2 beats = %4\n", step, start, bbt, beats));

			} else {

				/* Advance by the number of bars specified by
				   bar_mod, then recompute the beats and
				   superclock position corresponding to that
				   BBT time.
				*/

				bbt.bars += bar_mod;
				start = metric.superclock_at (bbt);
				beats = metric.quarters_at_superclock (start);
				DEBUG_TRACE (DEBUG::Grid, string_compose ("bar mod %1 moved to %2 (start %3)\n", bar_mod, bbt, start))
			}
		}

		/* we might be finished ...*/

		if (start >= end) {
			break;
		}

		DEBUG_TRACE (DEBUG::Grid, string_compose ("stopped fill with start %1 and point at %2\n", start, (p == _points.end() ? -1 : p->sclock())));

		while ((p != _points.end()) && (start >= p->sclock())) {

			DEBUG_TRACE (DEBUG::Grid, string_compose ("pausing to deal with point => %1\n", *p));

			/* have just passed or arrived at the next
			 * point. Consider adding a grid point for this point.
			 */

			start = p->sclock();
			bbt = p->bbt();
			beats = p->beats();

			if (bar_mod != 0) {
				if (bbt.is_bar() && (bar_mod == 1 || ((bbt.bars % bar_mod == 0)))) {
					ret.push_back (TempoMapPoint (*this, metric, start, beats, bbt));
					DEBUG_TRACE (DEBUG::Grid, string_compose ("G %1\t       %2\n", metric, ret.back()));
				} else {
					DEBUG_TRACE (DEBUG::Grid, string_compose ("-- skip %1 not on bar_mod %2\n", bbt, bar_mod));
				}
			} else {
				ret.push_back (TempoMapPoint (*this, metric, start, beats, bbt));
				DEBUG_TRACE (DEBUG::Grid, string_compose ("G %1\t       %2\n", metric, ret.back()));
			}

			/* But there may be multiple points here, and we have
			 * to check them all (Tempo/Meter/MusicTime ... which
			 * is itself both a Tempo *and* Meter point) before
			 * proceeding.
			 */

			const superclock_t pos = p->sclock();

			Points::const_iterator nxt = p;
			++nxt;

			TempoPoint const * tpp;
			MeterPoint const * mpp;

			/* use this point */

			if ((tpp = dynamic_cast<TempoPoint const *> (&(*p))) != 0) {
				tp = tpp;
			}

			if ((mpp = dynamic_cast<MeterPoint const *> (&(*p))) != 0) {
				mp = mpp;
			}

			/* use any subsequent ones at the same location */

			while ((nxt != _points.end()) && (nxt->sclock() == pos)) {

				/* Set up the new metric given the new point */

				if ((tpp = dynamic_cast<TempoPoint const *> (&(*nxt))) != 0) {
					tp = tpp;
				}

				if ((mpp = dynamic_cast<MeterPoint const *> (&(*nxt))) != 0) {
					mp = mpp;
				}

				++nxt;
			}

			/* Build a new metric from the composite of all the
			 * points at this position.
			 */

			metric = TempoMetric (*tp, *mp);
			p = nxt;
		}

		/* If we've reached the end of the points list, break and let
		 * the final phase below fill out the rest of the grid
		 */

		if (p == _points.end()) {
			break;
		}
	}

	/* reached the end or no more points to consider, so just
	 * finish by filling the grid to the end, if necessary.
	 */

	if (start < end) {

		DEBUG_TRACE (DEBUG::Grid, string_compose ("reached end, no more map points, finish between %1 .. %2\n", start, end));

		/* note: if start < end, then p == _points.end(). This means there are
		 * no more Points beyond the current value of start.
		 *
		 * Since there are no more Points beyond start, the current metric
		 * cannot involve a ramp, so the step size per grid element is
		 * constant. metric will also remain constant until we finish.
		 */

		const superclock_t step  = metric.superclocks_per_grid_at (start);

		do {
			const Temporal::Beats beats = metric.quarters_at_superclock (start);

			if (bar_mod != 0) {
				if (bbt.is_bar() && (bar_mod == 1 || ((bbt.bars % bar_mod == 0)))) {
					ret.push_back (TempoMapPoint (*this, metric, start, beats, bbt));
					DEBUG_TRACE (DEBUG::Grid, string_compose ("Gend %1\t       %2\n", metric, ret.back()));
				}
			} else {
				ret.push_back (TempoMapPoint (*this, metric, start, beats, bbt));
				DEBUG_TRACE (DEBUG::Grid, string_compose ("Gend %1\t       %2\n", metric, ret.back()));
			}

			start += step;
			bbt = metric.bbt_at (timepos_t::from_superclock (start));

		} while (start < end);

		/* all done */
	} else {
		if (p == _points.end()) {
			DEBUG_TRACE (DEBUG::Grid, string_compose ("ended loop with start %1 end %2, p @ END\n", start, end));
		} else {
			DEBUG_TRACE (DEBUG::Grid, string_compose ("ended loop with start %1 end %2, p=> %3\n", start, end, *p));
		}
	}

	DEBUG_TRACE (DEBUG::Grid, "<<< GRID DONE\n");
}

uint32_t
TempoMap::count_bars (Beats const & start, Beats const & end)
{
	TempoMapPoints bar_grid;
	superclock_t s (superclock_at (start));
	superclock_t e (superclock_at (end));
	get_grid (bar_grid, s, e, 1);
	return bar_grid.size();
}

std::ostream&
std::operator<<(std::ostream& str, Meter const & m)
{
	return str << m.divisions_per_bar() << '/' << m.note_value();
}

std::ostream&
std::operator<<(std::ostream& str, Tempo const & t)
{
	if (t.ramped()) {
		return str << t.note_types_per_minute() << " 1/" << t.note_type() << " RAMPED notes per minute [ " << t.super_note_type_per_second() << " => " << t.end_super_note_type_per_second() << " sntpm ] (" << t.superclocks_per_note_type() << " sc-per-1/" << t.note_type() << ')';
	} else {
		return str << t.note_types_per_minute() << " 1/" << t.note_type() << " notes per minute [" << t.super_note_type_per_second() << " sntpm] (" << t.superclocks_per_note_type() << " sc-per-1/" << t.note_type() << ')';
	}
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
std::operator<<(std::ostream& str, MusicTimePoint const & p)
{
	str << "MP@";
	str << *((Point const *) &p);
	str << *((Tempo const *) &p);
	str << *((Meter const *) &p);
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
	    << " secs " << tmp.sample (TEMPORAL_SAMPLE_RATE) << " samples"
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

BBT_Time
TempoMap::bbt_walk (BBT_Time const & bbt, BBT_Offset const & o) const
{
	BBT_Offset offset (o);
	BBT_Time start (bbt);
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

	/* see ::metric_at() for comments about the use of const_cast here
	 */

	TempoMetric metric (*const_cast<TempoPoint*>(&*prev_t), *const_cast<MeterPoint*>(&*prev_m));


	/* normalize possibly too-large ticks count */

	const int32_t tpg = metric.meter().ticks_per_grid ();

	if (offset.ticks > tpg) {
		/* normalize */
		offset.beats += offset.ticks / tpg;
		offset.ticks %= tpg;
	}

	/* add each beat, 1 by 1, rechecking to see if there's a new
	 * TempoMetric in effect after each addition
	 */

#define TEMPO_CHECK_FOR_NEW_METRIC                                      \
	if (((next_t != _tempos.end()) && (start >= next_t->bbt())) || \
	    ((next_m != _meters.end()) && (start >= next_m->bbt()))) { \
		/* need new metric */ \
		if (start >= next_t->bbt()) { \
			if (start >= next_m->bbt()) { \
				metric = TempoMetric (*const_cast<TempoPoint*>(&*next_t), *const_cast<MeterPoint*>(&*next_m)); \
				++next_t; \
				++next_m; \
			} else { \
				metric = TempoMetric (*const_cast<TempoPoint*>(&*next_t), metric.meter()); \
				++next_t; \
			} \
		} else if (start >= next_m->bbt()) { \
			metric = TempoMetric (metric.tempo(), *const_cast<MeterPoint*>(&*next_m)); \
			++next_m; \
		} \
	}

	for (int32_t b = 0; b < offset.bars; ++b) {

		TEMPO_CHECK_FOR_NEW_METRIC;
		start.bars += 1;
	}

	for (int32_t b = 0; b < offset.beats; ++b) {

		TEMPO_CHECK_FOR_NEW_METRIC;
		start.beats += 1;
		if (start.beats > metric.divisions_per_bar()) {
			start.bars += 1;
			start.beats = 1;
		}
	}

	start.ticks += offset.ticks;

	if (start.ticks >= ticks_per_beat) {
		start.beats += 1;
		start.ticks %= ticks_per_beat;
	}


	return start;
}

Temporal::Beats
TempoMap::quarters_at (timepos_t const & pos) const
{
	if (pos.is_beats()) {
		/* a bit redundant */
		return pos.beats();
	}
	return quarters_at_superclock (pos.superclocks());
}

Temporal::Beats
TempoMap::quarters_at (Temporal::BBT_Time const & bbt) const
{
	return metric_at (bbt).quarters_at (bbt);
}

Temporal::Beats
TempoMap::quarters_at_superclock (superclock_t pos) const
{
	return metric_at (pos).quarters_at_superclock (pos);
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
TempoMap::set_state (XMLNode const & node, int version)
{
	cerr << "\n\n\n TMAP set state\n\n";
	if (version <= 6000) {
		cerr << "Old version " << version << "\n";
		return set_state_3x (node);
	}

	/* global map properties */

	/* XXX this should probably be at the global level in the session file because it affects a lot more than just the tempo map, potentially */
	node.get_property (X_("superclocks-per-second"), superclock_ticks_per_second);

	node.get_property (X_("time-domain"), _time_domain);

	XMLNodeList const & children (node.children());

	for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {
		if ((*c)->name() == X_("Tempos")) {
			if (set_tempos_from_state (**c)) {
				cerr << "tempo fail\n";
				return -1;
			}
		}

		if ((*c)->name() == X_("Meters")) {
			if (set_meters_from_state (**c)) {
				cerr << "meter fail\n";
				return -1;
			}
		}

		if ((*c)->name() == X_("MusicTimes")) {
			if (set_music_times_from_state (**c)) {
				cerr << "bartimes fail\n";
				return -1;
			}
		}
	}

	for (Tempos::iterator t = _tempos.begin(); t != _tempos.end(); ++t) { _points.push_back (*t); }
	for (Meters::iterator m = _meters.begin(); m != _meters.end(); ++m) { _points.push_back (*m); }
	for (MusicTimes::iterator b = _bartimes.begin(); b != _bartimes.end(); ++b) { _points.push_back (*b); }

	_points.sort (Point::sclock_comparator());

	cerr << "MAP LOADED\n";
	dump (cerr);
	return 0;
}

int
TempoMap::set_music_times_from_state (XMLNode const& mt_node)
{
	XMLNodeList const & children (mt_node.children());

	try {
		_bartimes.clear ();
		for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {
			MusicTimePoint* mp = new MusicTimePoint (*this, **c);
			_bartimes.push_back (*mp);
		}
	} catch (...) {
		_bartimes.clear (); /* remove any that were created */
		return -1;
	}

	return 0;
}

int
TempoMap::set_tempos_from_state (XMLNode const& tempos_node)
{
	XMLNodeList const & children (tempos_node.children());

	try {
		_tempos.clear ();
		for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {
			TempoPoint* tp = new TempoPoint (*this, **c);
			_tempos.push_back (*tp);
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
	XMLNodeList const & children (meters_node.children());

	try {
		_meters.clear ();
		for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {
			MeterPoint* mp = new MeterPoint (*this, **c);
			_meters.push_back (*mp);
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

/** Takes a position and distance (both in any time domain), and returns a timecnt_t
 * that describes that distance from that position in a specified time domain.
 *
 * This method is used when converting a (distance,pos) pair (a timecnt_t) into
 * (distance,pos') in a different time domain. For an english example: "given a
 * distance of N beats from position P, what is that same distance measured in
 * superclocks from P' ?"
 *
 * A different example: "given a distance N superclocks from position P, what
 * is that same distance measured in beats from P' ?"
 *
 * There is a trivial case in which the requested return domain is the same as
 * the domain for the distance. In english: "given a distance of N beats from
 * P, what is the same distance measured in beats from P' ?" In this case, we
 * can simply construct a new timecnt_t that uses P' instead of P, since the
 * distance component must necessarily be same. Notice that this true no matter
 * what domain is used.
 */

timecnt_t
TempoMap::convert_duration (timecnt_t const & duration, timepos_t const & new_position, TimeDomain return_domain) const
{
	timepos_t p (return_domain);
	Beats b;
	superclock_t s;

	if (return_domain == duration.time_domain()) {
		/* new timecnt_t: same distance, but new position */
		return timecnt_t (duration.distance(), new_position);
	}

	switch (return_domain) {
	case AudioTime:
		switch (duration.time_domain()) {
		case AudioTime:
			/*NOTREACHED*/
			break;
		case BeatTime:
			/* duration is in beats but we're asked to return superclocks */
			switch (new_position.time_domain()) {
			case BeatTime:
				/* new_position is already in beats */
				p = new_position;
				break;
			case AudioTime:
				/* Determine beats at sc pos, so that we can add beats */
				p = timepos_t (metric_at (new_position).quarters_at_superclock (new_position.superclocks()));
				break;
			}
			/* add beats */
			p += duration;
			/* determine superclocks */
			s = metric_at (p).superclock_at (p.beats());
			/* return duration in sc */
			return timecnt_t::from_superclock (s - new_position.superclocks(), new_position);
			break;
		}
		break;

	case BeatTime:
		switch (duration.time_domain()) {
		case AudioTime:
			/* duration is in superclocks but we're asked to return beats */
			switch (new_position.time_domain ()) {
			case AudioTime:
				/* pos is already in superclocks */
				p = new_position;
				break;
			case BeatTime:
				/* determined sc at beat position so we can add superclocks */
				p = timepos_t (metric_at (new_position).sample_at (new_position.beats()));
				break;
			}
			/* add superclocks */
			p += duration;
			/* determine beats */
			b = metric_at (p).quarters_at_superclock (p.superclocks());
			/* return duration in beats */
			return timecnt_t (b - new_position.beats(), new_position);
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
	assert (!_tempos.empty());
	assert (!_meters.empty());

	if (pos == std::numeric_limits<timepos_t>::min()) {
		/* can't insert time at the front of the map: those entries are fixed */
		return;
	}

	Tempos::iterator     t (_tempos.begin());
	Meters::iterator     m (_meters.begin());
	MusicTimes::iterator b (_bartimes.begin());

	TempoPoint current_tempo = *t;
	MeterPoint current_meter = *m;
	MusicTimePoint current_time_point (*this, 0, Beats(), BBT_Time(), current_tempo, current_meter);

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
				beats = current_tempo.quarters_at_superclock (sc);
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
				beats = current_tempo.quarters_at_superclock (sc);
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
				beats = current_tempo.quarters_at_superclock (sc);
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

bool
TempoMap::remove_time (timepos_t const & pos, timecnt_t const & duration)
{
	superclock_t start (pos.superclocks());
	superclock_t end ((pos + duration).superclocks());
	superclock_t shift (duration.superclocks());

	TempoPoint* last_tempo = 0;
	MeterPoint* last_meter = 0;
	TempoPoint* tempo_after = 0;
	MeterPoint* meter_after = 0;

	bool moved = false;

	for (Tempos::iterator t = _tempos.begin(); t != _tempos.end(); ) {

		if (t->sclock() >= start && t->sclock() < end) {

			last_tempo = &*t;
			t = _tempos.erase (t);
			moved = true;

		} else if (t->sclock() >= start) {
			t->set (t->sclock() - shift, t->beats(), t->bbt());
			moved = true;
			if (t->sclock() == start) {
				tempo_after = &*t;
			}
			++t;
		}
	}

	for (Meters::iterator m = _meters.begin(); m != _meters.end(); ) {

		if (m->sclock() >= start && m->sclock() < end) {

			last_meter = &*m;
			m = _meters.erase (m);
			moved = true;

		} else if (m->sclock() >= start) {
			m->set (m->sclock() - shift, m->beats(), m->bbt());
			moved = true;
			if (m->sclock() == start) {
				meter_after = &*m;
			}
			++m;
		}
	}

	if (last_tempo && !tempo_after) {
		last_tempo->set (start, last_tempo->beats(), last_tempo->bbt());
		moved = true;
	}

	if (last_meter && !meter_after) {
		last_tempo->set (start, last_meter->beats(), last_meter->bbt());
		moved = true;
	}

	if (moved) {
		reset_starting_at (start);
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

double
TempoMap::quarters_per_minute_at (timepos_t const & pos) const
{
	TempoPoint const & tp (tempo_at (pos));
	const double val = tp.note_types_per_minute_at_DOUBLE (pos) * (4.0 / tp.note_type());
	return val;
}


TempoPoint const &
TempoMap::tempo_at (timepos_t const & pos) const
{
	return pos.is_beats() ? tempo_at (pos.beats()) : tempo_at (pos.superclocks());
}

MeterPoint const &
TempoMap::meter_at (timepos_t const & pos) const
{
	return pos.is_beats() ? meter_at (pos.beats()) : meter_at (pos.superclocks());
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
TempoMap::metric_at (superclock_t sc, bool can_match) const
{
	TempoPoint const * tp = 0;
	MeterPoint const * mp = 0;

	(void) get_tempo_and_meter (tp, mp, sc, can_match, false);

	return TempoMetric (*tp,* mp);
}

TempoMetric
TempoMap::metric_at (Beats const & b, bool can_match) const
{
	TempoPoint const * tp = 0;
	MeterPoint const * mp = 0;

	(void) get_tempo_and_meter (tp, mp, b, can_match, false);

	return TempoMetric (*tp, *mp);
}

TempoMetric
TempoMap::metric_at (BBT_Time const & bbt, bool can_match) const
{
	TempoPoint const * tp = 0;
	MeterPoint const * mp = 0;

	(void) get_tempo_and_meter (tp, mp, bbt, can_match, false);

	return TempoMetric (*tp, *mp);
}

void
TempoMap::set_ramped (TempoPoint & tp, bool yn)
{
	assert (!_tempos.empty());

	Rampable & r (tp);

	if (tp.ramped() == yn) {
		return;
	}

	Tempos::iterator nxt = _tempos.begin();
	++nxt;

	for (Tempos::iterator t = _tempos.begin(); nxt != _tempos.end(); ++t, ++nxt) {
		if (tp == *t) {
			break;
		}
	}

	if (yn) {
		r.set_end (nxt->end_super_note_type_per_second(), nxt->end_superclocks_per_note_type());
	} else {
		r.set_end (tp.super_note_type_per_second(), tp.superclocks_per_note_type());
	}

	r.set_ramped (yn);

	reset_starting_at (tp.sclock());
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
	/* do the update step of RCU. This will also update this thread's map pointer */
	update (map);
}

void
TempoMap::init ()
{
	SharedPtr new_map (new TempoMap (Tempo (120, 4), Meter (4, 4)));
	_map_mgr.init (new_map);
	fetch ();
}

TempoMap::SharedPtr
TempoMap::write_copy()
{
	return _map_mgr.write_copy();
}

int
TempoMap::update (TempoMap::SharedPtr m)
{
	if (!_map_mgr.update (m)) {
		return -1;
	}

	cerr << "************** new tempo map @ " << m << endl;

	/* update thread local map pointer in the calling thread */
	update_thread_tempo_map ();

	MapChanged (); /* EMIT SIGNAL */

	return 0;
}

void
TempoMap::abort_update ()
{
	/* drop lock taken by write_copy() */
	_map_mgr.abort ();
	/* update thread local map pointer in calling thread. Note that this
	   will reset _tempo_map_p, which is (almost guaranteed to be) the only
	   reference to the copy of the map made in ::write_copy(), so it will
	   be destroyed here.
	*/
	TempoMap::fetch ();
}

void
TempoMap::midi_clock_beat_at_or_after (samplepos_t const pos, samplepos_t& clk_pos, uint32_t& clk_beat)
{
	/* Sequences are always assumed to start on a MIDI Beat of 0 (ie, the downbeat).
	 *
	 * There are 24 MIDI clock per quarter note (1 Temporal::Beat)
	 *
	 * from http://midi.teragonaudio.com/tech/midispec/seq.htm
	 */

	Temporal::Beats b = (quarters_at_sample (pos)).round_up_to_beat ();

	clk_pos = sample_at (b);
	clk_beat = b.get_beats () * 24;

	assert (clk_pos >= pos);
}

/******** OLD STATE LOADING CODE SECTION *************/

static bool
bbt_time_to_string (const BBT_Time& bbt, std::string& str)
{
	char buf[256];
	int retval = snprintf (buf, sizeof(buf), "%" PRIu32 "|%" PRIu32 "|%" PRIu32, bbt.bars, bbt.beats,
	                       bbt.ticks);

	if (retval <= 0 || retval >= (int)sizeof(buf)) {
		return false;
	}

	str = buf;
	return true;
}

static bool
string_to_bbt_time (const std::string& str, BBT_Time& bbt)
{
	if (sscanf (str.c_str (), "%" PRIu32 "|%" PRIu32 "|%" PRIu32, &bbt.bars, &bbt.beats,
	            &bbt.ticks) == 3) {
		return true;
	}
	return false;
}

int
TempoMap::parse_tempo_state_3x (const XMLNode& node, LegacyTempoState& lts)
{
	BBT_Time bbt;
	std::string start_bbt;

	// _legacy_bbt.bars = 0; // legacy session check compars .bars != 0; default BBT_Time c'tor uses 1.

	if (node.get_property ("start", start_bbt)) {
		if (string_to_bbt_time (start_bbt, bbt)) {
			/* legacy session - start used to be in bbt*/
			// _legacy_bbt = bbt;
			// set_pulse(-1.0);
			info << _("Legacy session detected. TempoSection XML node will be altered.") << endmsg;
		}
	}

	/* position is the only data we extract from older XML */

	if (!node.get_property ("frame", lts.sample)) {
		error << _("Legacy tempo section XML does not have a \"frame\" node - map will be ignored") << endmsg;
		cerr << _("Legacy tempo section XML does not have a \"frame\" node - map will be ignored") << endl;
		return -1;
	}

	if (node.get_property ("beats-per-minute", lts.note_types_per_minute)) {
		if (lts.note_types_per_minute < 0.0) {
			error << _("TempoSection XML node has an illegal \"beats_per_minute\" value") << endmsg;
			return -1;
		}
	}

	if (!node.get_property ("note-type", lts.note_type)) {
		if (lts.note_type < 1.0) {
			error << _("TempoSection XML node has an illegal \"note-type\" value") << endmsg;
			cerr << _("TempoSection XML node has an illegal \"note-type\" value") << endl;
			return -1;
		}
	} else {
		/* older session, make note type be quarter by default */
		lts.note_type = 4.0;
	}

	if (!node.get_property ("clamped", lts.clamped)) {
		lts.clamped = false;
	}

	if (node.get_property ("end-beats-per-minute", lts.end_note_types_per_minute)) {
		if (lts.end_note_types_per_minute < 0.0) {
			info << _("TempoSection XML node has an illegal \"end-beats-per-minute\" value") << endmsg;
			cerr << _("TempoSection XML node has an illegal \"end-beats-per-minute\" value") << endl;
			return -1;
		}
	}

	Tempo::Type old_type;
	if (node.get_property ("tempo-type", old_type)) {
		/* sessions with a tempo-type node contain no end-beats-per-minute.
		   if the legacy node indicates a constant tempo, simply fill this in with the
		   start tempo. otherwise we need the next neighbour to know what it will be.
		*/

		if (old_type == Tempo::Constant) {
			lts.end_note_types_per_minute = lts.note_types_per_minute;
		} else {
			lts.end_note_types_per_minute = -1.0;
		}
	}

	if (!node.get_property ("active", lts.active)) {
		warning << _("TempoSection XML node has no \"active\" property") << endmsg;
		lts.active = true;
	}

	return 0;
}

int
TempoMap::parse_meter_state_3x (const XMLNode& node, LegacyMeterState& lms)
{
	std::string bbt_str;
	if (node.get_property ("start", bbt_str)) {
		if (string_to_bbt_time (bbt_str, lms.bbt)) {
			/* legacy session - start used to be in bbt*/
			info << _("Legacy session detected - MeterSection XML node will be altered.") << endmsg;
			// set_pulse (-1.0);
		} else {
			error << _("MeterSection XML node has an illegal \"start\" value") << endmsg;
		}
	}

	/* position is the only data we extract from older XML */

	if (!node.get_property ("frame", lms.sample)) {
		error << _("Legacy tempo section XML does not have a \"frame\" node - map will be ignored") << endmsg;
		return -1;
	}

	if (!node.get_property ("beat", lms.beat)) {
		lms.beat = 0.0;
	}

	if (node.get_property ("bbt", bbt_str)) {
		if (!string_to_bbt_time (bbt_str, lms.bbt)) {
			error << _("MeterSection XML node has an illegal \"bbt\" value") << endmsg;
			return -1;
		}
	} else {
		warning << _("MeterSection XML node has no \"bbt\" property") << endmsg;
	}

	/* beats-per-bar is old; divisions-per-bar is new */

	if (!node.get_property ("divisions-per-bar", lms.divisions_per_bar)) {
		if (!node.get_property ("beats-per-bar", lms.divisions_per_bar)) {
			error << _("MeterSection XML node has no \"beats-per-bar\" or \"divisions-per-bar\" property") << endmsg;
			return -1;
		}
	}

	if (lms.divisions_per_bar < 0.0) {
		error << _("MeterSection XML node has an illegal \"divisions-per-bar\" value") << endmsg;
		return -1;
	}

	if (!node.get_property ("note-type", lms.note_type)) {
		error << _("MeterSection XML node has no \"note-type\" property") << endmsg;
		return -1;
	}

	if (lms.note_type < 0.0) {
		error << _("MeterSection XML node has an illegal \"note-type\" value") << endmsg;
		return -1;
	}

	return 0;
}

int
TempoMap::set_state_3x (const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;

	nlist = node.children();

	/* Need initial tempo & meter points, because subsequent ones will use
	 * set_tempo() and set_meter() which require pre-existing data
	 */

	int32_t initial_tempo_index = -1;
	int32_t initial_meter_index = -1;
	int32_t index;
	bool need_points_clear = true;

	for (niter = nlist.begin(), index = 0; niter != nlist.end(); ++niter, ++index) {
		XMLNode* child = *niter;

		if ((initial_tempo_index < 0) && (child->name() == Tempo::xml_node_name)) {

			LegacyTempoState lts;

			if (parse_tempo_state_3x (*child, lts)) {
				error << _("Tempo map: could not set new state, restoring old one.") << endmsg;
				break;
			}

			Tempo t (lts.note_types_per_minute,
			         lts.end_note_types_per_minute,
			         lts.note_type);
			TempoPoint* tp = new TempoPoint (*this, t, samples_to_superclock (0, TEMPORAL_SAMPLE_RATE), Beats(), BBT_Time());
			_tempos.clear ();
			if (need_points_clear) {
				_points.clear ();
				need_points_clear = false;
			}
			_tempos.push_back (*tp);
			_points.push_back (*tp);
			initial_tempo_index = index;

		}

		if ((initial_meter_index < 0) && (child->name() == Meter::xml_node_name)) {

			LegacyMeterState lms;

			if (parse_meter_state_3x (*child, lms)) {
				error << _("Tempo map: could not use old meter state, restoring old one.") << endmsg;
				break;
			}

			Meter m (lms.divisions_per_bar, lms.note_type);
			MeterPoint *mp = new MeterPoint (*this, m, 0, Beats(), BBT_Time());
			_meters.clear();
			if (need_points_clear) {
				_points.clear ();
				need_points_clear = false;
			}
			_meters.push_back (*mp);
			_points.push_back (*mp);
			initial_meter_index = index;
		}

		if (initial_tempo_index >= 0 && initial_meter_index >= 0) {
			cerr << "Post initial tempo & meter:";
			dump (cerr);
			break;
		}
	}

	if (initial_tempo_index < 0 || initial_meter_index < 0) {
		error << _("Old tempo map information is missing either tempo or meter information - ignored") << endmsg;
		return -1;
	}

	for (niter = nlist.begin(), index = 0; niter != nlist.end(); ++niter, ++index) {
		XMLNode* child = *niter;

		if (child->name() == Tempo::xml_node_name) {

			LegacyTempoState lts;

			if (parse_tempo_state_3x (*child, lts)) {
				error << _("Tempo map: could not set new state, restoring old one.") << endmsg;
				break;
			}

			if (index == initial_tempo_index) {
				/* already added */
				continue;
			}

			Tempo t (lts.note_types_per_minute,
			         lts.end_note_types_per_minute,
			         lts.note_type);

			set_tempo (t, timepos_t (lts.sample));

		} else if (child->name() == Meter::xml_node_name) {

			LegacyMeterState lms;

			if (parse_meter_state_3x (*child, lms)) {
				error << _("Tempo map: could not use old meter state, restoring old one.") << endmsg;
				break;
			}

			if (index == initial_meter_index) {
				/* already added */
				continue;
			}

			Meter m (lms.divisions_per_bar, lms.note_type);
			set_meter (m, timepos_t (lms.sample));
		}
	}

#if 0
	/* check for legacy sessions where bbt was the base musical unit for tempo */
	for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		TempoSection* t;
		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
			if (t->legacy_bbt().bars != 0) {
				fix_legacy_session();
				break;
			}

			if (t->end_note_types_per_minute() < 0.0) {
				fix_legacy_end_session();
				break;
			}
		}
	}

	if (niter == nlist.end()) {
		MetricSectionSorter cmp;
		_metrics.sort (cmp);
	}
#endif
#if 0

	/* check for multiple tempo/meters at the same location, which
	   ardour2 somehow allowed.
	*/

	{
		Tempos::iterator prev = _tempos.end();
		for (Tempos::iterator i = _tempos.begin(); i != _tempos.end(); ++i) {
			if (prev != _tempos.end()) {
			MeterSection* ms;
			MeterSection* prev_m;
			TempoSection* ts;
			TempoSection* prev_t;
			if ((prev_m = dynamic_cast<MeterSection*>(*prev)) != 0 && (ms = dynamic_cast<MeterSection*>(*i)) != 0) {
				if (prev_m->beat() == ms->beat()) {
					cerr << string_compose (_("Multiple meter definitions found at %1"), prev_m->beat()) << endmsg;
					error << string_compose (_("Multiple meter definitions found at %1"), prev_m->beat()) << endmsg;
					return -1;
				}
			} else if ((prev_t = dynamic_cast<TempoSection*>(*prev)) != 0 && (ts = dynamic_cast<TempoSection*>(*i)) != 0) {
				if (prev_t->pulse() == ts->pulse()) {
					cerr << string_compose (_("Multiple tempo definitions found at %1"), prev_t->pulse()) << endmsg;
					error << string_compose (_("Multiple tempo definitions found at %1"), prev_t->pulse()) << endmsg;
					return -1;
				}
			}
		}
		prev = i;
	}
#endif

	return 0;
}
#if 0
void
TempoMap::fix_legacy_session ()
{
	MeterSection* prev_m = 0;
	TempoSection* prev_t = 0;
	bool have_initial_t = false;

	for (Metrics::iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		MeterSection* m;
		TempoSection* t;

		if ((m = dynamic_cast<MeterSection*>(*i)) != 0) {
			if (m->initial()) {
				pair<double, BBT_Time> bbt = make_pair (0.0, BBT_Time (1, 1, 0));
				m->set_beat (bbt);
				m->set_pulse (0.0);
				m->set_minute (0.0);
				m->set_position_lock_style (AudioTime);
				prev_m = m;
				continue;
			}
			if (prev_m) {
				pair<double, BBT_Time> start = make_pair (((m->bbt().bars - 1) * prev_m->note_divisor())
									  + (m->bbt().beats - 1)
									  + (m->bbt().ticks / BBT_Time::ticks_per_beat)
									  , m->bbt());
				m->set_beat (start);
				const double start_beat = ((m->bbt().bars - 1) * prev_m->note_divisor())
					+ (m->bbt().beats - 1)
					+ (m->bbt().ticks / BBT_Time::ticks_per_beat);
				m->set_pulse (start_beat / prev_m->note_divisor());
			}
			prev_m = m;
		} else if ((t = dynamic_cast<TempoSection*>(*i)) != 0) {

			if (!t->active()) {
				continue;
			}
			/* Ramp type never existed in the era of this tempo section */
			t->set_end_note_types_per_minute (t->note_types_per_minute());

			if (t->initial()) {
				t->set_pulse (0.0);
				t->set_minute (0.0);
				t->set_position_lock_style (AudioTime);
				prev_t = t;
				have_initial_t = true;
				continue;
			}

			if (prev_t) {
				/* some 4.x sessions have no initial (non-movable) tempo. */
				if (!have_initial_t) {
					prev_t->set_pulse (0.0);
					prev_t->set_minute (0.0);
					prev_t->set_position_lock_style (AudioTime);
					prev_t->set_initial (true);
					prev_t->set_locked_to_meter (true);
					have_initial_t = true;
				}

				const double beat = ((t->legacy_bbt().bars - 1) * ((prev_m) ? prev_m->note_divisor() : 4.0))
					+ (t->legacy_bbt().beats - 1)
					+ (t->legacy_bbt().ticks / BBT_Time::ticks_per_beat);
				if (prev_m) {
					t->set_pulse (beat / prev_m->note_divisor());
				} else {
					/* really shouldn't happen but.. */
					t->set_pulse (beat / 4.0);
				}
			}
			prev_t = t;
		}
	}
}
void
TempoMap::fix_legacy_end_session ()
{
	TempoSection* prev_t = 0;

	for (Metrics::iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		TempoSection* t;

		if ((t = dynamic_cast<TempoSection*>(*i)) != 0) {

			if (!t->active()) {
				continue;
			}

			if (prev_t) {
				if (prev_t->end_note_types_per_minute() < 0.0) {
					prev_t->set_end_note_types_per_minute (t->note_types_per_minute());
				}
			}

			prev_t = t;
		}
	}

	if (prev_t) {
		prev_t->set_end_note_types_per_minute (prev_t->note_types_per_minute());
	}
}

#endif
