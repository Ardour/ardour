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
#include <cmath>
#include <vector>

#include <inttypes.h>

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/enumwriter.h"
#include "pbd/error.h"
#include "pbd/integer_division.h"
#include "pbd/failed_constructor.h"
#include "pbd/stacktrace.h"
#include "pbd/string_convert.h"

#include "temporal/debug.h"
#include "temporal/tempo.h"
#include "temporal/types_convert.h"

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

#ifndef NDEBUG
#define TEMPO_MAP_ASSERT(expr) TempoMap::map_assert(expr, #expr, __FILE__, __LINE__)
#else
#define TEMPO_MAP_ASSERT(expr)
#endif

void
Point::add_state (XMLNode & node) const
{
	node.set_property (X_("sclock"), _sclock);
	node.set_property (X_("quarters"), _quarters);
	node.set_property (X_("bbt"), _bbt);
}

Point::Point (TempoMap const & map, XMLNode const & node)
	: MapOwned (map)
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

Tempo::Tempo (XMLNode const & node)
{
	TEMPO_MAP_ASSERT (node.name() == xml_node_name);


	node.get_property (X_("npm"), _npm);
	node.get_property (X_("enpm"), _enpm);

	_superclocks_per_note_type = double_npm_to_scpn (_npm);
	_end_superclocks_per_note_type = double_npm_to_scpn (_enpm);

	if (!node.get_property (X_("note-type"), _note_type)) {
		throw failed_constructor ();
	}

	if (!node.get_property (X_("locked-to-meter"), _locked_to_meter)) {
		_locked_to_meter = true;
	}

	/* older versions used "clamped" as the property name here */

	if (!node.get_property (X_("continuing"), _continuing) && !node.get_property (X_("clamped"), _continuing)) {
		_continuing = false;
	}
}

void
Tempo::set_note_types_per_minute (double npm)
{
	_npm = npm;
	_superclocks_per_note_type = double_npm_to_scpn (_npm);
}

void
Tempo::set_end_npm (double npm)
{
	_enpm = npm;
	_end_superclocks_per_note_type = double_npm_to_scpn (_enpm);
}

void
Tempo::set_continuing (bool yn)
{
	_continuing = yn;
}

XMLNode&
Tempo::get_state () const
{
	XMLNode* node = new XMLNode (xml_node_name);

	node->set_property (X_("npm"), note_types_per_minute());
	node->set_property (X_("enpm"), end_note_types_per_minute());
	node->set_property (X_("note-type"), note_type());
	node->set_property (X_("type"), type());
	node->set_property (X_("locked-to-meter"), _locked_to_meter);
	node->set_property (X_("continuing"), _continuing);

	/* We don't have an _active property any more, but earlier versions of
	   Ardour will crash during session loading if this property is not
	   provided. For the 7.5 - 8.0 transition, there was theoretically no
	   file format change, so leave this in place till at least the next
	   format version change.
	*/
	node->set_property (X_("active"), true);

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

	node.get_property (X_("note-type"), _note_type);

	if (!node.get_property (X_("locked-to-meter"), _locked_to_meter)) {
		_locked_to_meter = true;
	}

	/* older versions used "clamped" as the property name here */

	if (!node.get_property (X_("continuing"), _continuing) && !node.get_property (X_("continuing"), _continuing)) {
		_continuing = false;
	}

	return 0;
}

Meter::Meter (XMLNode const & node)
{
	TEMPO_MAP_ASSERT (node.name() == xml_node_name);
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
		const int32_t tpB = tpg * _divisions_per_bar;

		if (r.ticks >= tpB) {
			r.bars += r.ticks / tpB;
			r.ticks %= tpB;
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
		r.beats += floor ((double) r.ticks / tpg);
		r.ticks = tpg + (r.ticks % Temporal::Beats::PPQN);
	}

	if (r.beats <= 0) {
		r.bars += floor ((r.beats - 1.0) / _divisions_per_bar);
		r.beats = _divisions_per_bar + (r.beats % _divisions_per_bar);
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
Meter::round_up_to_beat_div (Temporal::BBT_Time const & bbt, int beat_div) const
{
	Temporal::BBT_Time b = bbt.round_up_to_beat_div (beat_div);
	if (b.beats > _divisions_per_bar) {
		b.bars++;
		b.beats = 1;
	}
	return b;
}

Temporal::BBT_Time
Meter::round_to_beat (Temporal::BBT_Time const & bbt) const
{
	Temporal::BBT_Time b = bbt.round_to_beat ();
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
		if (node.get_property (X_("omega"), _omega)) {
			if (node.get_property (X_("omega"), _omega)) {
			/* Older versions only defined a single omega value */
				/* ???? */
			}
		}
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
	, _omega (0.)
{
	if (node.get_property (X_("omega"), _omega)) {
		/* Older versions only defined a single omega value */
		if (node.get_property (X_("omega"), _omega)) {
			/* ???? */
		}
	}
}

void
TempoPoint::set_omega (double ob)
{
	_omega = ob;
}


/* To understand the math(s) behind ramping, see the file doc/tempo.{pdf,tex}
 */

void
TempoPoint::compute_omega_from_next_tempo (TempoPoint const & next)
{
	compute_omega_from_distance_and_next_tempo (next.beats() - beats(), next);
}

void
TempoPoint::compute_omega_from_distance_and_next_tempo (Beats const & quarter_duration, TempoPoint const & next)
{
	superclock_t end_scpqn;

	if (!_continuing) {
		/* tempo is defined by our own start and end */
		end_scpqn = end_superclocks_per_quarter_note();
	} else {
		/* tempo is defined by our own start the start of the next tempo */
		end_scpqn = next.superclocks_per_quarter_note ();
	}

	if (superclocks_per_quarter_note () == end_scpqn) {
		_omega = 0.0;
		return;
	}

	compute_omega_from_quarter_duration (quarter_duration, end_scpqn);
}

void
TempoPoint::compute_omega_from_quarter_duration (Beats const & quarter_duration, superclock_t end_scpqn)
{
	const double old = _omega;

	if (!std::isfinite (_omega = ((1.0/end_scpqn) - (1.0/superclocks_per_quarter_note())) / DoubleableBeats (quarter_duration).to_double())) {
		DEBUG_TRACE (DEBUG::TemporalMap, "quarter-computed omega out of bounds\n");
		_omega = old;
	}
	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("quarter-computed omega from qtr duration = %1 dur was %2 start speed %3 end speed [%4]\n", _omega, quarter_duration.str(), superclocks_per_quarter_note(), end_scpqn));
}

superclock_t
TempoPoint::superclock_at (Temporal::Beats const & qn) const
{
	if (qn == _quarters) {
		return _sclock;
	}

	if (qn < Beats()) {
		/* negative */

		TEMPO_MAP_ASSERT (_quarters == Beats());
	} else {
		/* positive */
		TEMPO_MAP_ASSERT (qn >= _quarters);
	}

	if (!actually_ramped()) {
		/* not ramped, use linear */
		const Beats delta = qn - _quarters;
		const superclock_t spqn = superclocks_per_quarter_note ();
		return _sclock + (spqn * delta.get_beats()) + muldiv_round (spqn, delta.get_ticks(), superclock_t (Temporal::ticks_per_beat));
	}

	superclock_t r;
	const double log_expr = superclocks_per_quarter_note() * _omega * DoubleableBeats (qn - _quarters).to_double();

	// std::cerr << "logexpr " << log_expr << " from " << superclocks_per_quarter_note() << " * " << _omega << " * " << (qn - _quarters) << std::endl;

	if (log_expr < -1) {

		r = _sclock + llrint (log (-log_expr - 1.0) / -_omega);

		if (r < 0) {
			std::cerr << "CASE 1: " << *this << endl << " scpqn = " << superclocks_per_quarter_note() << std::endl;
			std::cerr << " for " << qn << " @ " << _quarters << " | " << _sclock << " + log (" << log_expr << ") "
			          << log (-log_expr - 1.0)
			          << " - omega = " << -_omega
			          << " => "
			          << r << std::endl;
			abort ();
		}

	} else {
		r = _sclock + llrint (log1p (log_expr) / _omega);

		// std::cerr << "r = " << _sclock << " + " << log1p (log_expr) / _omega << " => " << r << std::endl;

		if (r < 0) {
			std::cerr << "CASE 2: scpqn = " << superclocks_per_quarter_note() << std::endl;
			std::cerr << " for " << qn << " @ " << _quarters << " | " << _sclock << " + log1p (" << superclocks_per_quarter_note() * _omega * DoubleableBeats (qn - _quarters).to_double() << " = "
			          << log1p (superclocks_per_quarter_note() * _omega * DoubleableBeats (qn - _quarters).to_double())
			          << " => "
			          << r << std::endl;
			_map->dump (std::cerr);
			abort ();
		}
	}

	return r;
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
	   converted into a 64 bit tick value for common tempos.
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


		/* The simple expression of the math we're computing here is
		 * (if we could use floating point):
		 *
		 * double sc_delta = sc - _sclock;                                              // how far in superclocks from this point's superclock position?
		 * double seconds = sc_delta / superclock_ticks_per_second;                     // how long is that in seconds?
		 * double note_types_per_second = npm / 60.;                                    // how many note types in 1 second ?
		 * double quarter_delta = note_types_per_second * (4. / * _note_type);          // how many quarters is that?
		 * return _quarters + Beats (floor (quarter_delta), fmod (quarter_delta, 1.));  // add to this point's position in quarters
		 *
		 * But we can't use doubles, so this gets quite a bit more complex.
		 */

		// TEMPO_MAP_ASSERT (sc >= _sclock);
		superclock_t sc_delta = sc - _sclock;


		/* convert sc into superbeats, given that sc represents some number of seconds */
		const superclock_t whole_seconds = sc_delta / superclock_ticks_per_second();
		const superclock_t remainder = sc_delta - (whole_seconds * superclock_ticks_per_second());

		/* big number to allow most (fractional) BPMs to be represented as an integer "super note type per second"
		 *
		 * It is not required that big_numerator equal superclock_ticks_per_second but since the values in both cases have similar
		 * desired properties (many, many factors), it doesn't hurt to use the same number.
		 */
		const superclock_t big_numerator = 508032000; // 2^10 * 3^4 * 5^3 * 7^2

		uint64_t _super_note_type_per_second = (uint64_t) llround (_npm * big_numerator / 60);

		const int64_t supernotes = ((_super_note_type_per_second) * whole_seconds) + muldiv_round (superclock_t (_super_note_type_per_second), remainder, superclock_ticks_per_second());
		const int64_t superbeats = muldiv_round (supernotes, 4, (superclock_t) _note_type);

		/* convert superbeats to beats:ticks */
		int32_t b = superbeats / big_numerator;
		int64_t remain = superbeats - (b * big_numerator);
		int32_t t = PBD::muldiv_round (Temporal::ticks_per_beat, remain, big_numerator);

		DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("%8 => \nsc %1 delta %9 = %2 secs rem = %3 rem snotes %4 sbeats = %5 => %6 : %7\n", sc, whole_seconds, remainder, supernotes, superbeats, b , t, *this, sc_delta));

		const Beats ret = _quarters + Beats (b, t);

		/* positive superclock can never generate negative beats unless
		 * it is too large. If that happens, handle it the same way as
		 * the opening special case in this method.
		 */

		if (sc >= 0 && ret < Beats()) {
			return std::numeric_limits<Beats>::max();
		}

		return ret;
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

TempoMetric::TempoMetric (TempoPoint const & t, MeterPoint const & m)
	: _tempo (&t)
	, _meter (&m)
{
	_reftime = _tempo->map().reftime (t, m);
}

superclock_t
TempoMap::reftime (TempoPoint const & t, MeterPoint const & m) const
{
	Points::const_iterator pi;

	if (m.sclock() < t.sclock()) {
		pi = _points.s_iterator_to (*(static_cast<const Point*> (&m)));
	} else {
		pi = _points.s_iterator_to (*(static_cast<const Point*> (&t)));
	}

	/* Walk backwards through points to find a BBT markers, or the start */

	while (pi != _points.begin()) {
		if (dynamic_cast<const MusicTimePoint*> (&*pi)) {
			break;
		}
		--pi;
	}

	return pi->sclock();
}

Temporal::BBT_Argument
TempoMetric::bbt_at (timepos_t const & pos) const
{
	if (pos.is_beats()) {
		return bbt_at (pos.beats());
	}

	superclock_t sc = pos.superclocks();

	/* Use the later of the tempo or meter as the reference point to
	 * compute the BBT distance. All map points are fully defined by all 3
	 * time types, but we need the latest one to avoid incorrect
	 * computations of quarter duration.
	 */

	const Point* reference_point;

	if (_tempo->beats() < _meter->beats()) {
		reference_point = _meter;
	} else {
		reference_point = _tempo;
	}

	const Beats dq = _tempo->quarters_at_superclock (sc) - reference_point->beats();

	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("qn @ %1 = %2, meter @ %3 , delta %4\n", sc, _tempo->quarters_at_superclock (sc), _meter->beats(), dq));

	/* dq is delta in quarters (beats). Convert to delta in note types of
	   the current meter, which we'll call "grid"
	*/

	const int64_t note_value_count = muldiv_round (dq.get_beats(), _meter->note_value(), int64_t (4));

	/* now construct a BBT_Offset using the count in grid units */

	const BBT_Offset bbt_offset (0, note_value_count, dq.get_ticks());

	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("BBT offset from %3 @ %1: %2\n", (_tempo->beats() < _meter->beats() ?  _meter->bbt() : _tempo->bbt()), bbt_offset,
	                                                 (_tempo->beats() < _meter->beats() ? "meter" : "tempo")));
	superclock_t ref (std::min (_meter->sclock(), _tempo->sclock()));

	return BBT_Argument (ref, _meter->bbt_add (reference_point->bbt(), bbt_offset));
}

superclock_t
TempoMetric::superclock_at (BBT_Time const & bbt) const
{
	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("get quarters for %1 = %2 using %3\n", bbt, _meter->quarters_at (bbt), *this));
	return _tempo->superclock_at (_meter->quarters_at (bbt));
}

MusicTimePoint::MusicTimePoint (TempoMap const & map, XMLNode const & node)
	: Point (map, node)
	, TempoPoint (map, *node.child (Tempo::xml_node_name.c_str()))
	, MeterPoint (map, *node.child (Meter::xml_node_name.c_str()))
{
	node.get_property (X_("name"), _name); /* may fail, leaves name empty */
}

XMLNode&
MusicTimePoint::get_state () const
{
	XMLNode* node = new XMLNode (X_("MusicTime"));

	Point::add_state (*node);

	node->add_child_nocopy (Tempo::get_state());
	node->add_child_nocopy (Meter::get_state());

	node->set_property (X_("name"), _name); /* failure is OK */

	return *node;
}

void
MusicTimePoint::set_name (std::string const & str)
{
	_name = str;
	/* XXX need a signal or something to announce change */
}

bool
GridIterator::valid_for (TempoMap const & m, superclock_t start, uint32_t bmod, uint32_t bdiv) const
{
	if (!valid || start != end || map != &m || bar_mod != bmod || beat_div != bdiv) {
		return false;
	}

	return true;
}


/* TEMPOMAP */

TempoMap::TempoMap (Tempo const & initial_tempo, Meter const & initial_meter)
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
{
	copy_points (other);
}

TempoMap&
TempoMap::operator= (TempoMap const & other)
{
	copy_points (other);
	return *this;
}

void
TempoMap::copy_points (TempoMap const & other)
{
	MusicTimePoint const * mt;
	TempoPoint const * tp;
	MeterPoint const * mp;

	for (auto const & point : other._points) {
		if ((mt = dynamic_cast<MusicTimePoint const *> (&point))) {
			MusicTimePoint* mtp = new MusicTimePoint (*mt);
			_bartimes.push_back (*mtp);
			_meters.push_back (*mtp);
			_tempos.push_back (*mtp);
			_points.push_back (*mtp);
			mtp->set_map (*this);
		} else if ((mp = dynamic_cast<MeterPoint const *> (&point))) {
			MeterPoint* mpp = new MeterPoint (*mp);
			_meters.push_back (*mpp);
			_points.push_back (*mpp);
			mpp->set_map (*this);
		} else if ((tp = dynamic_cast<TempoPoint const *> (&point))) {
			TempoPoint* tpp = new TempoPoint (*tp);
			_tempos.push_back (*tpp);
			_points.push_back (*tpp);
			tpp->set_map (*this);
		}
	}

#ifndef NDEBUG
	for (auto & p : _points) {
		assert (&p.map () == this);
	}
	for (auto & t : _tempos) {
		assert (&t.map () == this);
	}
	for (auto & m : _meters) {
		assert (&m.map () == this);
	}
#endif

}

TempoMapCutBuffer*
TempoMap::cut (timepos_t const & start, timepos_t const & end, bool ripple)
{
	return cut_copy (start, end, false, ripple);
}

TempoMapCutBuffer*
TempoMap::copy ( timepos_t const & start, timepos_t const & end)
{
	return cut_copy (start, end, true, false);
}


TempoMapCutBuffer*
TempoMap::cut_copy (timepos_t const & start, timepos_t const & end, bool copy, bool ripple)
{
	if (n_tempos() == 1 && n_meters() == 1) {
		return nullptr;
	}

	TempoMetric sm (metric_at (start));
	TempoMetric em (metric_at (end));
	timecnt_t dur = start.distance (end);

	TempoMapCutBuffer* cb = new TempoMapCutBuffer (dur);

	superclock_t start_sclock = start.superclocks();
	superclock_t end_sclock = end.superclocks();
	bool removed = false;

	Tempo start_tempo (tempo_at (start));
	Tempo end_tempo (tempo_at (end));
	Meter start_meter (meter_at (start));
	Meter end_meter (meter_at (end));

	MusicTimePoint* mtp;
	BBT_Time bbt (bbt_at (start));
	Beats b (quarters_at (start));

	if (!copy) {
		mtp = new MusicTimePoint (*this, start_sclock, b, bbt, em.tempo(), em.meter(), _("cut"));
	} else {
		mtp = nullptr;
	}

	for (Points::iterator p = _points.begin(); p != _points.end(); ) {


		/* XXX might to check time domain of start/end, and use beat
		 * time here.
		 */

		if (p->sclock() >= end_sclock) {
			break;
		}

		if (p->sclock() < start_sclock) {
			++p;
			continue;
		}

		Points::iterator nxt (p);
		++nxt;

		TempoPoint const * tp;
		MeterPoint const * mp;
		MusicTimePoint const * mtp;

		if ((mtp = dynamic_cast<MusicTimePoint const *> (&*p))) {
			cb->add (*mtp);
			if (!copy && mtp->sclock() != 0) {
				core_remove_bartime (*mtp);
				remove_point (*mtp);
				removed = true;
			}
		} else {
			if ((tp = dynamic_cast<TempoPoint const *> (&*p))) {
				cb->add (*tp);
				if (!copy && tp->sclock() != 0) {
					core_remove_tempo (*tp);
					remove_point (*tp);
					removed = true;
				}
			} else if ((mp = dynamic_cast<MeterPoint const *> (&*p))) {
				cb->add (*mp);
				if (!copy && mp->sclock() != 0) {
					core_remove_meter (*mp);
					remove_point (*mp);
					removed = true;
				}
			}
		}

		p = nxt;
	}

	if (!copy && ripple) {
		shift (start, -start.distance (end));
	}

	if (mtp) {
		// add_or_replace_bartime (mtp);
	}

	if (!copy && removed) {
		reset_starting_at (start_sclock);
	}


	return cb;
}

void
TempoMap::paste (TempoMapCutBuffer const & cb, timepos_t const & position, bool ripple, std::string suggested_name)
{
	if (cb.empty()) {
		return;
	}

	if (ripple) {
		shift (position, cb.duration());
	}


	/* We need to look these up first, before we change the map */
	const timepos_t end_position = position + cb.duration();
	const Tempo end_tempo = tempo_at (end_position);
	const Meter end_meter = meter_at (end_position);

	/* iterate over _points since they are already in sclock order, and we
	 * won't need to post-sort the way we would if we handled tempos,
	 * meters, bartimes separately.
	 */

	BBT_Time        pos_bbt = bbt_at (position);
	Beats           pos_beats = quarters_at (position);
	bool            ignored;
	bool            replaced;
	MusicTimePoint* mtp;
	superclock_t    s;
	std::string     name;

	/* Do not try to put a BBT marker at absolute zero or anywhere on a bar */

	if (!position.is_zero() && (pos_bbt.ticks != 0 || pos_bbt.beats != 1)) {

		if (suggested_name.empty()) {
			name = _("paste>");
		} else {
			name = string_compose (X_("%1>"), suggested_name);
		}

		mtp = new MusicTimePoint (*this, position.superclocks(), pos_beats, pos_bbt, tempo_at (position), meter_at (position), name);

		core_add_bartime (mtp, replaced);

		if (!replaced) {
			core_add_tempo (mtp, ignored);
			core_add_meter (mtp, ignored);
			core_add_point (mtp);
		}
	}


	for (auto const & p : cb.points()) {
		TempoPoint const * tp;
		MeterPoint const * mp;
		MusicTimePoint const * mtp;
		Beats b;
		BBT_Time bb;

		s = p.sclock() + position.superclocks();
		b = quarters_at_superclock (s);
		bb = bbt_at (s);

		if ((mtp = dynamic_cast<MusicTimePoint const *> (&p))) {

			tp = dynamic_cast<TempoPoint const *> (&p);
			mp = dynamic_cast<MeterPoint const *> (&p);

			MusicTimePoint *ntp = new MusicTimePoint (*this, s, b, bb, *tp, *mp, mtp->name());
			core_add_bartime (ntp, replaced);

			if (!replaced) {
				core_add_tempo (ntp, ignored);
				core_add_meter (ntp, ignored);
				core_add_point (ntp);
			}

		} else {

			if ((tp = dynamic_cast<TempoPoint const *> (&p))) {
				TempoPoint *ntp = new TempoPoint (*this, *tp, s, b, bb);
				core_add_tempo (ntp, replaced);
				if (!replaced) {
					core_add_point (ntp);
				}
			} else if ((mp = dynamic_cast<MeterPoint const *> (&p))) {
				MeterPoint *ntp = new MeterPoint (*this, *mp, s, b, bb);
				core_add_meter (ntp, replaced);
				if (!replaced) {
					core_add_point (ntp);
				}
			}
		}
	}

	pos_bbt = bbt_at (end_position);
	pos_beats = quarters_at (end_position);

	if (pos_bbt.ticks != 0 || pos_bbt.beats != 1) {

		if (suggested_name.empty()) {
			name = _("<paste");
		} else {
			name = string_compose (X_("<%1"), suggested_name);
		}

		mtp = new MusicTimePoint (*this,  end_position.superclocks(), pos_beats, pos_bbt, end_tempo, end_meter, name);
		core_add_bartime (mtp, replaced);
		if (!replaced) {
			core_add_tempo (mtp, ignored);
			core_add_meter (mtp, ignored);
			core_add_point (mtp);
		}
	}

	reset_starting_at (s);
}

void
TempoMap::shift (timepos_t const & at, timecnt_t const & by)
{
	timecnt_t abs_by (by.abs());
	superclock_t distance = abs_by.superclocks ();
	superclock_t at_superclocks = abs_by.superclocks ();

	if (distance == 0) {
		return;
	}

	for (auto & p : _points) {

		if (p.sclock() >= at_superclocks) {

			if (distance >= 0 || p.sclock() > distance) {

				if (dynamic_cast<MusicTimePoint*> (&p)) {
					continue;
				}

				superclock_t s = p.sclock() + distance;
				BBT_Time bb = bbt_at (s);
				Beats b = quarters_at_superclock (s);
				p.set (s, b, bb);
			}
		}
	}

	reset_starting_at (at_superclocks + distance);
}

void
TempoMap::shift (timepos_t const & at, BBT_Offset const & offset)
{
	/* for now we require BBT-based shifts to be in units of whole bars */

	if (std::abs (offset.bars) < 1) {
		return;
	}

	if (offset.beats || offset.ticks) {
		return;
	}

	const superclock_t at_superclocks = at.superclocks();

	for (Points::iterator p = _points.begin(); p != _points.end(); ) {

		Points::iterator nxt = p;
		++nxt;

		if (p->sclock() >= at_superclocks) {
			if (offset.bars > p->bbt().bars) {

				TempoPoint* tp;
				MeterPoint* mp;

				if (dynamic_cast<MusicTimePoint*> (&*p)) {
					break;
				} else if ((mp = dynamic_cast<MeterPoint*> (&*p))) {
					core_remove_meter (*mp);
				} else if ((tp = dynamic_cast<TempoPoint*> (&*p))) {
					core_remove_tempo (*tp);
				}
			} else {
				BBT_Time new_bbt (p->bbt().bars + offset.bars, p->bbt().beats, p->bbt().ticks);
				p->set (p->sclock(), p->beats(), new_bbt);
			}
		}

		p = nxt;
	}

	reset_starting_at (at_superclocks);
}

MeterPoint*
TempoMap::add_meter (MeterPoint* mp)
{
	bool replaced;
	MeterPoint* ret = core_add_meter (mp, replaced);

	if (!replaced) {
		core_add_point (mp);
	} else {
		delete mp;
	}

	reset_starting_at (ret->sclock());
	return ret;
}

bool
TempoMap::clear_tempos_before (timepos_t const & t, bool stop_at_music_time)
{
	if (_tempos.size() < 2) {
		return false;
	}

	bool removed = false;
	superclock_t sc = t.superclocks();
	Tempos::iterator tp = _tempos.end();
	--tp;
	MusicTimePoint* mtp (nullptr);

	while (tp != _tempos.begin()) {

		if (tp->sclock() > sc) {
			--tp;
			mtp = nullptr;
			continue;
		}

		if ((mtp = dynamic_cast<MusicTimePoint*> (&*tp)) && stop_at_music_time) {
			break;
		}

		Tempos::iterator nxt = tp;
		--nxt;

		if (mtp) {
			Meters::iterator mpi = _meters.s_iterator_to (*(static_cast<MeterPoint*> (&*mtp)));
			_meters.erase (mpi);
			MusicTimes::iterator mtpi = _bartimes.s_iterator_to (*mtp);
			_bartimes.erase (mtpi);
		}

		Points::iterator pi = _points.s_iterator_to (*(static_cast<Point*> (&*tp)));

		if (pi != _points.end()) {
			_points.erase (pi);
		}

		_tempos.erase (tp);
		removed = true;
		tp = nxt;
		mtp = nullptr;
	}

	if (removed) {
		reset_starting_at (sc);
	}

	return removed;
}

bool
TempoMap::clear_tempos_after (timepos_t const & t, bool stop_at_music_time)
{
	if (_tempos.size() < 2) {
		return false;
	}

	bool removed = false;
	superclock_t sc = t.superclocks();
	Tempos::iterator tp = _tempos.begin();
	MusicTimePoint* mtp (nullptr);
	++tp;

	while (tp != _tempos.end()) {

		if (tp->sclock() < sc) {
			++tp;
			mtp = nullptr;
			continue;
		}

		if ((mtp = dynamic_cast<MusicTimePoint*> (&*tp)) && stop_at_music_time) {
			break;
		}

		Tempos::iterator nxt = tp;
		++nxt;

		if (mtp) {
			Meters::iterator mpi = _meters.s_iterator_to (*(static_cast<MeterPoint*> (&*mtp)));
			_meters.erase (mpi);
			MusicTimes::iterator mtpi = _bartimes.s_iterator_to (*mtp);
			_bartimes.erase (mtpi);
		}

		Points::iterator pi = _points.s_iterator_to (*(static_cast<Point*> (&*tp)));

		if (pi != _points.end()) {
			_points.erase (pi);
		}

		_tempos.erase (tp);
		removed = true;
		tp = nxt;
		mtp = nullptr;
	}

	if (removed) {
		reset_starting_at (sc);
	}

	return removed;
}

void
TempoMap::change_tempo (TempoPoint & p, Tempo const & t)
{
	*((Tempo*)&p) = t;
	reset_starting_at (p.sclock());
}

void
TempoMap::replace_tempo (TempoPoint const & old, Tempo const & t, timepos_t const & time)
{
	if (old.sclock() == 0) {
		_tempos.front() = t;
		reset_starting_at (0);
		return;
	}

	remove_tempo (old, false);
	set_tempo (t, time);
}

TempoPoint &
TempoMap::set_tempo (Tempo const & t, BBT_Argument const & bbt)
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

#ifndef NDEBUG
	if (DEBUG_ENABLED (DEBUG::TemporalMap)) {
		dump (cerr);
	}
#endif

	return *ret;
}

void
TempoMap::core_add_point (Point* pp)
{
	Points::iterator p;
	const Beats beats_limit = pp->beats();

	for (p = _points.begin(); p != _points.end() && p->beats() < beats_limit; ++p);
	_points.insert (p, *pp);
}

TempoPoint*
TempoMap::core_add_tempo (TempoPoint* tp, bool& replaced)
{
	Tempos::iterator t;
	const superclock_t sclock_limit = tp->sclock();
	const Beats beats_limit = tp->beats ();

	for (t = _tempos.begin(); t != _tempos.end() && t->beats() < beats_limit; ++t);

	if (t != _tempos.end()) {
		if (t->sclock() == sclock_limit) {
			/* overwrite Tempo part of this point */
			*((Tempo*)&(*t)) = *tp;
			/* caller must delete tp when replaced is true */
			DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("overwrote old tempo with %1\n", *tp));
			replaced = true;
			return &(*t);
		}
	}

	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("inserted tempo %1\n", *tp));

	replaced = false;
	return &(* _tempos.insert (t, *tp));
}

MeterPoint*
TempoMap::core_add_meter (MeterPoint* mp, bool& replaced)
{
	Meters::iterator m;
	const superclock_t sclock_limit = mp->sclock();
	const Beats beats_limit = mp->beats ();

	for (m = _meters.begin(); m != _meters.end() && m->beats() < beats_limit; ++m);

	if (m != _meters.end()) {
		if (m->sclock() == sclock_limit) {
			/* overwrite Meter part of this point */
			*((Meter*)&(*m)) = *mp;
			/* caller must delete mp when replaced is true */
			replaced = true;
			return &(*m);
		}
	}

	replaced = false;
	return &(*(_meters.insert (m, *mp)));
}

MusicTimePoint*
TempoMap::core_add_bartime (MusicTimePoint* mtp, bool& replaced)
{
	MusicTimes::iterator m;
	const superclock_t sclock_limit = mtp->sclock();

	for (m = _bartimes.begin(); m != _bartimes.end() && m->sclock() < sclock_limit; ++m);

	if (m != _bartimes.end()) {
		if (m->sclock() == sclock_limit) {
			/* overwrite Tempo part of this point */
			*m = *mtp;
			/* caller must delete mtp when replaced is true */
			DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("overwrote old bartime with %1\n", mtp));
			replaced = true;
			return &(*m);
		}
	}

	DEBUG_TRACE (DEBUG::TemporalMap, string_compose ("inserted bartime %1\n", mtp));

	replaced = false;
	return &(* _bartimes.insert (m, *mtp));
}

TempoPoint*
TempoMap::add_tempo (TempoPoint * tp)
{
	bool replaced;
	TempoPoint* ret = core_add_tempo (tp, replaced);

	if (!replaced) {
		core_add_point (tp);
	} else {
		delete tp;
	}

	TempoPoint* prev = const_cast<TempoPoint*> (previous_tempo (*ret));
	if (prev) {
		reset_starting_at (prev->sclock());
	} else {
		reset_starting_at (ret->sclock());
	}

	return ret;
}

void
TempoMap::remove_tempo (TempoPoint const & tp, bool with_reset)
{
	if (_tempos.size() < 2) {
		return;
	}

	if (!core_remove_tempo (tp)) {
		return;
	}

	superclock_t sc (tp.sclock());

	remove_point (tp);

	if (with_reset) {
		reset_starting_at (sc);
	}

}

bool
TempoMap::core_remove_tempo (TempoPoint const & tp)
{
	Tempos::iterator t;

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
		return false;
	}

	if (t->sclock() != tp.sclock()) {
		/* error ... no tempo point at the time of tp */
		std::cerr << "not point at time\n";
		return false;
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

	if (prev != _tempos.end() && was_end) {
		prev->set_end_npm (prev->note_types_per_minute()); /* remove any ramp */
	}

	return true;
}

void
TempoMap::set_bartime (BBT_Time const & bbt, timepos_t const & pos, std::string name)
{
	TEMPO_MAP_ASSERT (pos.time_domain() == AudioTime);

	superclock_t sc (pos.superclocks());
	TempoMetric metric (metric_at (sc));

	/* MusicTimePoints define a beat position (even if it is not predicted
	 * by the prior tempo map elements.
	 */

	Beats b = metric.quarters_at_superclock (sc).round_up_to_beat ();;

	MusicTimePoint* tp = new MusicTimePoint (*this, sc, b, bbt, metric.tempo(), metric.meter(), name);

	add_or_replace_bartime (tp);
}

void
TempoMap::replace_bartime (MusicTimePoint & mtp, bool with_reset)
{
	bool ignored;

	core_add_bartime (&mtp, ignored);

	if (with_reset) {
		reset_starting_at (mtp.sclock());
	}
}

MusicTimePoint*
TempoMap::add_or_replace_bartime (MusicTimePoint* mtp)
{
	bool replaced;

	/* A MusicTimePoint by definition defines a beat position. It's not
	 * zero, but it must be "on beat". So ensure that this is true.
	 */

	mtp->set (mtp->sclock(), mtp->beats().round_up_to_beat(), mtp->bbt());

	MusicTimePoint* ret = core_add_bartime (mtp, replaced);

	if (!replaced) {
		bool ignore;
		(void) core_add_tempo (mtp, ignore);
		(void) core_add_meter (mtp, ignore);
		core_add_point (mtp);
	} else {
		delete mtp;
	}

	reset_starting_at (ret->sclock());

	return ret;
}

bool
TempoMap::core_remove_bartime (MusicTimePoint const & mtp)
{
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

	for (m = _bartimes.begin(); m != _bartimes.end() && m->sclock() < mtp.sclock(); ++m);

	if (m == _bartimes.end()) {
		/* error ... not found */
		return false;
	}

	if  (m->sclock() != mtp.sclock()) {
		/* error ... no music time point at the time of tp */
		return false;
	}

	remove_point (mtp);
	core_remove_tempo (mtp);
	core_remove_meter (mtp);
	_bartimes.erase (m);

	return true;

}
void
TempoMap::remove_bartime (MusicTimePoint const & mtp, bool with_reset)
{
	superclock_t sc (mtp.sclock());

	core_remove_bartime (mtp);

	if (with_reset) {
		reset_starting_at (sc);
	}
}

void
TempoMap::remove_point (Point const & point)
{
	Points::iterator p;

	/* Again, we do not allow multiple MusicTimePoints at the same
	 * location, so if sclock() matches, @param point matches
	 * the point in the list.
	 */

	for (p = _points.begin(); p != _points.end(); ++p) {
		if (p->sclock() == point.sclock()) {
			// XXX need to fix this leak delete tpp;
			_points.erase (p);
			break;
		}
	}
}

void
TempoMap::reset_starting_at (superclock_t sc)
{
	DEBUG_TRACE (DEBUG::MapReset, string_compose ("reset starting at %1\n", sc));
#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::MapReset)) {
		dump (std::cerr);
	}
#endif

	TEMPO_MAP_ASSERT (!_tempos.empty());
	TEMPO_MAP_ASSERT (!_meters.empty());


	TempoPoint*     tp;
	MeterPoint*     mp;
	MusicTimePoint* mtp;
	TempoMetric metric (_tempos.front(), _meters.front());
	Points::iterator p;
	bool need_initial_ramp_reset = false;

	DEBUG_TRACE (DEBUG::MapReset, string_compose ("we begin at %1 with metric %2\n", sc, metric));

	/* Setup the metric that is in effect at the starting point */

	for (p = _points.begin(); p != _points.end(); ++p) {

		DEBUG_TRACE (DEBUG::MapReset, string_compose ("Now looking at %1 => %2 \n", &(*p), *p));

		if (p->sclock() > sc) {
			break;
		}

		mtp = 0;
		tp = 0;
		mp = 0;

		if ((mtp = dynamic_cast<MusicTimePoint*> (&*p)) != 0) {
			metric = TempoMetric (*mtp, *mtp);
			DEBUG_TRACE (DEBUG::MapReset, string_compose ("Bartime!, used tempo @ %1\n", (TempoPoint*) mtp));
			need_initial_ramp_reset = false;
		} else if ((tp = dynamic_cast<TempoPoint*> (&*p)) != 0) {
			metric = TempoMetric (*tp, metric.meter());
			if (tp->ramped()) {
				need_initial_ramp_reset = true;
			} else {
				need_initial_ramp_reset = true;
			}
			DEBUG_TRACE (DEBUG::MapReset, string_compose ("Tempo! @ %1, metric's tempo is %2\n", tp, &metric.tempo()));
		} else if ((mp = dynamic_cast<MeterPoint*> (&*p)) != 0) {
			metric = TempoMetric (metric.tempo(), *mp);
			DEBUG_TRACE (DEBUG::MapReset, "Meter!\n");
		}
	}

	/* if the tempo point the defines our starting metric for position
	 * @param sc is ramped, recompute its omega value based on the beat
	 * time of the following tempo point. If we do not do this before we
	 * start, then ::superclock_at() for subsequent points will be
	 * incorrect.
	 */

	if (need_initial_ramp_reset) {
		const TempoPoint *nxt = next_tempo (metric.tempo());
		if (nxt) {
			const_cast<TempoPoint*> (&metric.tempo())->compute_omega_from_next_tempo (*nxt);
		}
		need_initial_ramp_reset = false;
	}

	MusicTimes::iterator next_mtp = _bartimes.begin();
	superclock_t current_section_limit;

	while (next_mtp != _bartimes.end() && (next_mtp->sclock() <= sc)) {
		++next_mtp;
	}

	if (next_mtp != _bartimes.end()) {
		DEBUG_TRACE (DEBUG::MapReset, string_compose ("start rset with section defined by MTP @ %1 %2\n", &*next_mtp, *next_mtp));
		current_section_limit = next_mtp->sclock();
	} else {
		current_section_limit = std::numeric_limits<superclock_t>::max();
		DEBUG_TRACE (DEBUG::MapReset, "start rset with no next MTP (run to end)\n");
	}

	/* Now iterate over remaining points and recompute their audio time
	 * and beat time positions.
	 */

	while (p != _points.end()) {

		if (next_mtp != _bartimes.end()) {
			current_section_limit = next_mtp->sclock();
		} else {
			current_section_limit = std::numeric_limits<superclock_t>::max();
		}

		Points::iterator section_start = p;
		DEBUG_TRACE (DEBUG::MapReset, string_compose ("start section at %1 with limit at %2\n", *p, current_section_limit));;

		while (p != _points.end() && (p->sclock() < current_section_limit)) {
			++p;
		}

		reset_section (section_start, p, current_section_limit, metric);

		if (next_mtp != _bartimes.end()) {
			DEBUG_TRACE (DEBUG::MapReset, string_compose ("reset MTP %1 using %2 to %3\n", *next_mtp, metric, metric.tempo().quarters_at_superclock (next_mtp->sclock())));
			next_mtp->set (next_mtp->sclock(), metric.tempo().quarters_at_superclock (next_mtp->sclock()), next_mtp->bbt());
		}

		if (next_mtp != _bartimes.end()) {
			++next_mtp;
		}
	}

	DEBUG_TRACE (DEBUG::MapReset, "RESET DONE\n");
#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::MapReset)) {
		dump (std::cerr);
	}
#endif
}

void
TempoMap::reset_section (Points::iterator& begin, Points::iterator& end, superclock_t section_limit, TempoMetric& metric)
{
	TempoPoint*     tp;
	TempoPoint*     nxt_tempo = 0;
	MeterPoint*     mp;
	MusicTimePoint* mtp;

	DEBUG_TRACE (DEBUG::MapReset, string_compose ("reset a section of %1 points, ending at %2\n", std::distance (begin, end), section_limit));

	for (Points::iterator p = begin; p != end; ) {

		mtp = 0;
		tp = 0;
		mp = 0;

		if ((mtp = dynamic_cast<MusicTimePoint*> (&*p)) == 0) {
			if ((tp = dynamic_cast<TempoPoint*> (&*p)) == 0) {
				mp = dynamic_cast<MeterPoint*> (&*p);
			}
		}

		DEBUG_TRACE (DEBUG::MapReset, string_compose ("workong on it! tp = %1 mp %2 mtp %3\n", tp, mp, mtp));

		if (tp) {

			Points::iterator pp = p;
			nxt_tempo = 0;
			++pp;

			while (pp != _points.end()) {
				TempoPoint* nt = dynamic_cast<TempoPoint*> (&*pp);
				if (nt) {
					nxt_tempo = nt;
					break;
				}
				++pp;
			}

			DEBUG_TRACE (DEBUG::MapReset, string_compose ("considering omega comp for %1 with nxt = %2\n", *tp, nxt_tempo));
			if (tp->ramped() && nxt_tempo) {
				tp->compute_omega_from_next_tempo (*nxt_tempo);
			}
		}

		Points::iterator nxt = p;
		++nxt;

		if (!mtp) {
			DEBUG_TRACE (DEBUG::MapReset, string_compose ("recompute %1 using %2\n", p->bbt(), metric));
			superclock_t sc = metric.superclock_at (p->bbt());

			if (sc >= section_limit) {
				if (tp) {
					core_remove_tempo (*tp);
				} else {
					core_remove_meter (*mp);
				}
			} else {

				if (mp) {
					/* Meter markers must be on-bar */
					BBT_Time rounded = metric.meter().round_to_bar (p->bbt());
					p->set (sc, metric.meter().quarters_at (rounded), rounded);
					DEBUG_TRACE (DEBUG::MapReset, string_compose ("\tbased on %1 move meter point to %2,%3\n", p->bbt(), sc, p->beats()));
				} else {
					/* Tempo markers must be on-beat */
					BBT_Time rounded = metric.meter().round_to_beat (p->bbt());
					p->set (sc, metric.meter().quarters_at (rounded), rounded);
					DEBUG_TRACE (DEBUG::MapReset, string_compose ("\tbased on %1 move tempo point to %2,%3\n", p->bbt(), sc, p->beats()));
				}
			}

		} else {
			DEBUG_TRACE (DEBUG::MapReset, "\tnot recomputing this one\n");
		}

		/* Now ensure that metric is correct moving forward */

		if ((mtp = dynamic_cast<MusicTimePoint*> (&*p)) != 0) {
			metric = TempoMetric (*mtp, *mtp);
		} else if ((tp = dynamic_cast<TempoPoint*> (&*p)) != 0) {
			metric = TempoMetric (*tp, metric.meter());
		} else if ((mp = dynamic_cast<MeterPoint*> (&*p)) != 0) {
			metric = TempoMetric (metric.tempo(), *mp);
		}

		p = nxt;
	}
}


bool
TempoMap::move_meter (MeterPoint const & mp, timepos_t const & when, bool push)
{
	TEMPO_MAP_ASSERT (!_tempos.empty());
	TEMPO_MAP_ASSERT (!_meters.empty());

	if (_meters.size() < 2 || mp == _meters.front()) {
		/* not movable */
		return false;
	}

	superclock_t sc;
	Beats beats;
	BBT_Time bbt;

	beats = when.beats ();

	/* Do not allow moving a meter marker to the same position as
	 * an existing one.
	 */

	Tempos::iterator t, prev_t;
	Meters::iterator m, prev_m;

	/* meter changes must be on bar */
	for (t = _tempos.begin(), prev_t = _tempos.end(); t != _tempos.end() && t->beats() < beats; ++t) { prev_t = t; }
	for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end() && m->beats() < beats && *m != mp; ++m) { prev_m = m; }

	if (prev_m == _meters.end()) {
		return false;
	}

	if (prev_t == _tempos.end()) {
		prev_t = _tempos.begin();
	}

	TempoMetric metric (*prev_t, *prev_m);
	bbt = metric.bbt_at (beats);

	bbt = metric.round_to_bar (bbt);

	/* Now find the correct TempoMetric for the new BBT position (which may
	 * differ from the one we determined earlier.
	 *
	 * The search for the correct meter will be limited by the meter we're
	 * dragging. But the search for the correct tempo needs to bounded by
	 * both the BBT *and* the beat position, in case there is an upcoming
	 * BBT marker.
	 */

	for (t = _tempos.begin(), prev_t = _tempos.end(); t != _tempos.end() && t->bbt() < bbt && t->beats() < beats; ++t) { prev_t = t; }
	for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end() && m->bbt() < bbt && *m != mp; ++m) { prev_m = m; }


	if (prev_m == _meters.end()) {
		return false;
	}

	if (prev_t == _tempos.end()) {
		prev_t = _tempos.begin();
	}

	if (dynamic_cast<MusicTimePoint*> (&(*prev_t)) || dynamic_cast<MusicTimePoint*> (&(*prev_m))) {
		/* game over ... cannot drag meter through a BBT Marker */
		return false;
	}

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

	if (mp.sclock() == sc && mp.beats() == beats && mp.bbt() == bbt) {
		return false;
	}

	const superclock_t old_sc = mp.sclock();

	/* reset position of this meter */
	const_cast<MeterPoint*> (&mp)->set (sc, beats, bbt);

	{

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
		TEMPO_MAP_ASSERT (current != _meters.end());

		/* reposition in list */
		_meters.splice (insert_before, _meters, current);

	}

	{

		Points::iterator current = _points.end();
		Points::iterator insert_before = _points.end();

		for (Points::iterator m = _points.begin(); m != _points.end(); ++m) {
			if (*m == mp) {
				current = m;
			}
			if (insert_before == _points.end() && (m->sclock() > sc)) {
				insert_before = m;
			}
		}

		/* existing meter must have been found */
		TEMPO_MAP_ASSERT (current != _points.end());

		/* reposition in list */
		_points.splice (insert_before, _points, current);

	}

	/* recompute 3 domain positions for everything after this */
	reset_starting_at (std::min (sc, old_sc));

	return true;
}

bool
TempoMap::move_tempo (TempoPoint const & tp, timepos_t const & when, bool push)
{
	TEMPO_MAP_ASSERT (!_tempos.empty());
	TEMPO_MAP_ASSERT (!_meters.empty());

	if (_tempos.size() < 2 || tp == _tempos.front()) {
		/* not movable */
		return false;
	}

	superclock_t sc;
	Beats beats;
	BBT_Time bbt;

	/* tempo changes must be on beat */

	beats = when.beats();

	/* XXX need to TEMPO_MAP_ASSERT that meter note value is >= 4 */

	MeterPoint const & mm (meter_at (beats));

	beats.round_to_subdivision (mm.note_value() / 4, RoundNearest);

	/* Do not allow moving a tempo marker to the same position as
	 * an existing one.
	 */

	Tempos::iterator t, prev_t;
	Meters::iterator m, prev_m;

	/* find tempo & meter in effect at the new target location */

	for (t = _tempos.begin(), prev_t = _tempos.end(); t != _tempos.end() && t->beats() <= beats && *t != tp; ++t) { prev_t = t; }
	for (m = _meters.begin(), prev_m = _meters.end(); m != _meters.end() && m->beats() <= beats; ++m) { prev_m = m; }

	if (prev_t == _tempos.end()) {
		/* moved earlier than first, no movement */
		return false;
	}

	if (prev_m == _meters.end()) {
		/* moved earlier than first, no movement */
		return false;
	}

	/* If the previous tempo is ramped, we need to recompute its omega
	 * constant to cover the (new) duration of the ramp.
	 */

	if (prev_t->actually_ramped()) {
		prev_t->compute_omega_from_distance_and_next_tempo (beats - prev_t->beats(), tp);
	}

	TempoMetric metric (*prev_t, *prev_m);

	const Beats delta ((beats - tp.beats()).abs());

	if (delta < Beats::ticks (metric.meter().ticks_per_grid())) {
		return false;
	}

	sc = metric.superclock_at (beats);
	bbt = metric.bbt_at (beats);

	if (tp.sclock() == sc && tp.beats() == beats && tp.bbt() == bbt) {
		return false;
	}

	const superclock_t old_sc = tp.sclock();
	/* reset position of this tempo */
	const_cast<TempoPoint*> (&tp)->set (sc, beats, bbt);

	/* move to correct position in tempo list */

	{
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
		TEMPO_MAP_ASSERT (current != _tempos.end());
		/* reposition in list */
		_tempos.splice (insert_before, _tempos, current);
	}

	/* move to correct position in points list */

	{
		Points::iterator current = _points.end();
		Points::iterator insert_before = _points.end();

		for (Points::iterator t = _points.begin(); t != _points.end(); ++t) {
			if (*t == tp) {
				current = t;
			}
			if (insert_before == _points.end() && (t->sclock() > sc)) {
				insert_before = t;
			}
		}

		/* existing tempo must have been found */
		TEMPO_MAP_ASSERT (current != _points.end());

		/* reposition in list */
		_points.splice (insert_before, _points, current);
	}

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
TempoMap::set_meter (Meter const & t, BBT_Argument const & bbt)
{
	return set_meter (t, timepos_t (quarters_at (bbt)));
}

void
TempoMap::remove_meter (MeterPoint const & mp, bool with_reset)
{
	if (_meters.size() < 2) {
		return;
	}

	if (!core_remove_meter (mp)) {
		return;
	}

	superclock_t sc = mp.sclock();

	remove_point (mp);

	if (with_reset) {
		reset_starting_at (sc);
	}
}

bool
TempoMap::core_remove_meter (MeterPoint const & mp)
{
	Meters::iterator m;

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

	for (m = _meters.begin(); m != _meters.end() && m->sclock() < mp.sclock(); ++m);

	if (m == _meters.end()) {
		/* not found */
		return false;
	}

	if (m->sclock() != mp.sclock()) {
		/* error ... no meter point at the time of mp */
		return false;
	}

	_meters.erase (m);

	return true;
}

Temporal::BBT_Argument
TempoMap::bbt_at (timepos_t const & pos) const
{
	if (pos.is_beats()) {
		return bbt_at (pos.beats());
	}
	return bbt_at (pos.superclocks());
}

Temporal::BBT_Argument
TempoMap::bbt_at (superclock_t s) const
{
	TempoMetric metric (metric_at (s));

	superclock_t ref (std::min (metric.tempo().sclock(), metric.meter().sclock()));
	return BBT_Argument (ref, metric.bbt_at (timepos_t::from_superclock (s)));
}

Temporal::BBT_Argument
TempoMap::bbt_at (Temporal::Beats const & qn) const
{
	TempoMetric metric (metric_at (qn));
	return BBT_Argument (metric.reftime(), metric.bbt_at (qn));
}

superclock_t
TempoMap::superclock_at (Temporal::Beats const & qn) const
{
	return metric_at (qn).superclock_at (qn);
}

superclock_t
TempoMap::superclock_at (Temporal::BBT_Argument const & bbt) const
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

Temporal::Beats
TempoMap::bbtwalk_to_quarters (BBT_Argument const & pos, BBT_Offset const & distance) const
{
	return quarters_at (bbt_walk (pos, distance)) - quarters_at (pos);
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
	ostr << "\n\nTEMPO MAP @ " << this << ":\n" << std::dec;
	ostr << "... tempos...\n";
	for (Tempos::const_iterator t = _tempos.begin(); t != _tempos.end(); ++t) {
		ostr << &*t << ' ' << *t << endl;
	}

	ostr << "... meters...\n";
	for (Meters::const_iterator m = _meters.begin(); m != _meters.end(); ++m) {
		ostr << &*m << ' ' << *m << endl;
	}

	ostr << "... bartimes...\n";
	for (MusicTimes::const_iterator m = _bartimes.begin(); m != _bartimes.end(); ++m) {
		ostr << &*m << ' ' << *m << endl;
	}
	ostr << "... all points ...\n";
	for (Points::const_iterator p = _points.begin(); p != _points.end(); ++p) {
		ostr << &*p << ' ' << *p;
		if (dynamic_cast<MusicTimePoint const *> (&(*p))) {
			ostr << " BarTime";
		}

		if (dynamic_cast<TempoPoint const *> (&(*p))) {
			ostr << " Tempo";
		}

		if (dynamic_cast<MeterPoint const *> (&(*p))) {
			ostr << " Meter";
		}
		ostr << endl;
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

	TEMPO_MAP_ASSERT (!_tempos.empty());
	TEMPO_MAP_ASSERT (!_meters.empty());
	TEMPO_MAP_ASSERT (!_points.empty());

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

	/* Set return tempo and meter points by value using the starting tempo
	 * and meter passed in.
	 *
	 * Then advance through all points, resetting either tempo and/or meter
	 * until we find a point beyond (or equal to, if @p can_match is
	 * true) the @p arg (end time)
	 */

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

Points::const_iterator
TempoMap::get_grid (TempoMapPoints& ret, superclock_t rstart, superclock_t end, uint32_t bar_mod, uint32_t beat_div) const
{
	if (rstart == end) {
		return _points.end();
	}

	/* note: @p bar_mod is "bar modulo", and describes the N in "give
	   me every Nth bar". If the caller wants every 4th bar, bar_mod ==
	   4. If we want every point defined by the tempo note type (e.g. every
	   quarter not, then bar_mod is zero.
	*/

	TEMPO_MAP_ASSERT (!_tempos.empty());
	TEMPO_MAP_ASSERT (!_meters.empty());
	TEMPO_MAP_ASSERT (!_points.empty());

#ifndef NDEBUG
	if (DEBUG_ENABLED (DEBUG::Grid)) {
		dump (std::cout);
	}
#endif
	DEBUG_TRACE (DEBUG::Grid, string_compose (">>> GRID START %1 .. %2 (barmod = %3)\n", rstart, end, bar_mod));

	/* The fast path: one tempo, one meter, just do (relatively) simple math */

	if (_tempos.size() == 1 && _meters.size() == 1) {

		TempoMetric metric (_tempos.front(), _meters.front());

		/* Figure out the beat preceding rstart */

		superclock_t spdiv;

		if (bar_mod == 1) {
			spdiv = llrintf (metric.superclocks_per_note_type() * (metric.meter().divisions_per_bar() * (4. / metric.meter().note_value())));
		} else {
			spdiv = metric.superclocks_per_note_type() / beat_div;
		}

		superclock_t start = (rstart / spdiv) * spdiv; /* div (bar/beat) preceding rstart */

		/* determine BBT and beats at the position. Note that we know
		 * that the tempo and meter must be at zero
		 */

		BBT_Time bbt (metric.bbt_at (timepos_t::from_superclock (start)));

		/* Now round to bar mod or beat_div to keep the grid aligned
		 * with what has been asked for.
		 */

		BBT_Time on_bar;

		if (rstart != 0) {
			if (bar_mod == 1) {
				on_bar = bbt.round_up_to_bar ();
			} else {
				on_bar = metric.meter().round_up_to_beat_div (bbt, beat_div);
			}

			BBT_Offset delta = Temporal::bbt_delta (on_bar, bbt);

			if (delta != BBT_Offset ()) {
				bbt = on_bar;
				Beats beats_delta = _meters.front().to_quarters (delta);
				DEBUG_TRACE (DEBUG::Grid, string_compose ("simple reset start using bbt %1 via %2 (rounded by %3 beats %4) sc %5\n", bbt, on_bar, delta, beats_delta, start));
			} else {
				DEBUG_TRACE (DEBUG::Grid, string_compose ("bbt %1 was already on-bar or on-beat %2 start is %3\n", bbt, on_bar, start));
			}
		}

		Beats    beats (metric.quarters_at_superclock (start));

		DEBUG_TRACE (DEBUG::Grid, string_compose ("reset start to %1 with beats %2 for %3\n", start, beats, bbt));
		fill_grid_with_final_metric (ret, metric, start, rstart, end, bar_mod, beat_div, beats, bbt);

		return _points.end();
	}

	TempoPoint const * tp = 0;
	MeterPoint const * mp = 0;
	Points::const_iterator p = _points.begin();
	Beats beats;

	/* Find relevant meter for nominal start point */

	p = get_tempo_and_meter (tp, mp, rstart, true, true);

	/* p now points to either the point *after* start, or the end of the
	 * _points list.
	 *
	 * metric is the TempoMetric that is in effect at start
	 */

	TempoMetric metric = TempoMetric (*tp, *mp);

	DEBUG_TRACE (DEBUG::Grid, string_compose ("metric in effect at %1 = %2\n", rstart, metric));

	/* determine the BBT at start. We can discard the reftime of a
	 * BBT_Argument, because it is @var metric that defines it */

	BBT_Argument bba = metric.bbt_at (timepos_t::from_superclock (rstart));
	BBT_Time bbt (bba.bars, bba.beats, bba.ticks);

	/* We know that both the tempo point and meter point that make up @var
	 * metric are beat and bar aligned respectively (note: if they are a
	 * MusicTimePoint, they *define* a beat/bar alignment, even if they are
	 * arbitrarily placed with respect to the earlier elements of the tempo
	 * map.
	 *
	 * So we can just start at the later of the two of them,
	 */

	superclock_t start;

	if (tp->sclock() > mp->sclock()) {
		bbt = tp->bbt();
		start = tp->sclock();
	} else {
		bbt = mp->bbt();
		start = mp->sclock();
	}

	/* at this point:
	 *
	 * - metric is a TempoMetric that describes the situation at the start time
	 * - p is an iterator pointin to either the end of the _points list, or
	 *   the next point in the list after start.
	 */

	fill_grid_by_walking (ret, p, metric, start, rstart, end, bar_mod, beat_div, beats, bbt);

	/* reached the end or no more points to consider, so just
	 * finish by filling the grid to the end, if necessary.
	 */

	if (start < end) {
		fill_grid_with_final_metric (ret, metric, start, rstart, end, bar_mod, beat_div, beats, bbt);
	} else {
		if (p == _points.end()) {
			DEBUG_TRACE (DEBUG::Grid, string_compose ("ended loop with start %1 end %2, p @ END\n", start, end));
		} else {
			DEBUG_TRACE (DEBUG::Grid, string_compose ("ended loop with start %1 end %2, p=> %3\n", start, end, *p));
		}
	}

	DEBUG_TRACE (DEBUG::Grid, "<<< GRID DONE\n");

	return p;
}

void
TempoMap::get_grid (GridIterator& iter, TempoMapPoints& ret, superclock_t rstart, superclock_t end, uint32_t bar_mod, uint32_t beat_div) const
{
	DEBUG_TRACE (DEBUG::Grid, string_compose (">>> GRID-I START %1 .. %2 (barmod = %3) iter valid ? %4 iter for %5\n", rstart, end, bar_mod, iter.valid_for (*this, rstart, bar_mod, beat_div), iter.end));

	if (!iter.valid_for (*this, rstart, bar_mod, beat_div)) {
		Points::const_iterator p = get_grid (ret, rstart, end, bar_mod, beat_div);
		if (!ret.empty()) {
			TempoMapPoint& tmp (ret.back());
			iter.set (*this, &tmp.tempo(), &tmp.meter(), tmp.sclock(), tmp.beats(), tmp.bbt(), p, end, bar_mod, beat_div);
		} else {
			iter.catch_up_to (end);
		}
		return;
	}

	TempoMetric metric (*iter.tempo, *iter.meter);
	superclock_t start = iter.sclock;
	Beats beats = iter.beats;
	BBT_Time bbt = iter.bbt;
	Points::const_iterator p = iter.points_iterator;

	fill_grid_by_walking (ret, p, metric, start, rstart, end, bar_mod, beat_div, beats, bbt);

	if (start < end) {
		fill_grid_with_final_metric (ret, metric, start, rstart, end, bar_mod, beat_div, beats, bbt);
	}

	if (!ret.empty()) {
		TempoMapPoint& tmp (ret.back());
		iter.set (*this, &metric.tempo(), &metric.meter(), tmp.sclock(), tmp.beats(), tmp.bbt(), p, end, bar_mod, beat_div);
	} else {
		iter.catch_up_to (end);
	}

	DEBUG_TRACE (DEBUG::Grid, "<<< GRID-I DONE\n");
}

void
TempoMap::fill_grid_by_walking (TempoMapPoints& ret, Points::const_iterator& p_i, TempoMetric& metric_i, superclock_t& start, superclock_t rstart, superclock_t end,
                                int bar_mod, int beat_div, Beats& beats, BBT_Time& bbt) const
{
	TempoMetric metric (metric_i);
	Points::const_iterator p (p_i);

	while (p != _points.end() && start < end) {

		MusicTimePoint const *mtp = dynamic_cast<MusicTimePoint const *> (&*p);

		/* Generate grid points (either actual meter-defined
		 * beats, or bars based on bar_mod) up until the next point
		 * in the map
		 */

		if (bar_mod != 0) {
			if (start >= rstart) {
				if (bbt.is_bar() && (bar_mod == 1 || ((bbt.bars % bar_mod == 1)))) {
					ret.push_back (TempoMapPoint (*this, metric, start, beats, bbt));
					DEBUG_TRACE (DEBUG::Grid, string_compose ("Ga %1\t       [%2]\n", metric, ret.back()));
				} else {
					DEBUG_TRACE (DEBUG::Grid, string_compose ("-- skip %1 not on bar_mod %2\n", bbt, bar_mod));
				}
			} else {
				DEBUG_TRACE (DEBUG::Grid, string_compose ("skip-a point @ %1, too early for %2\n", start, rstart));
			}

			/* Advance by the number of bars specified by bar_mod */

			bbt.bars += bar_mod;

		} else {

			if (start >= rstart) {
				if (beat_div == 1) {
					ret.push_back (TempoMapPoint (*this, metric, start, beats, bbt));
					DEBUG_TRACE (DEBUG::Grid, string_compose ("Gb %1\t       [%2]\n", metric, ret.back()));
				} else {
					int ticks = (bbt.beats * metric.meter().ticks_per_grid()) + bbt.ticks;
					int mod = Temporal::ticks_per_beat / beat_div;

					if ((ticks % mod) == 0) {
						ret.push_back (TempoMapPoint (*this, metric, start, beats, bbt));
						DEBUG_TRACE (DEBUG::Grid, string_compose ("Gd %1\t       [%2]\n", metric, ret.back()));
					} else {
						DEBUG_TRACE (DEBUG::Grid, string_compose ("-- skip %1 not on beat_div_mod %2\n", ticks, mod));
					}
				}
			} else {
				DEBUG_TRACE (DEBUG::Grid, string_compose ("skip-b point @ %1, too early for %2\n", start, rstart));
			}
			/* Note that in a BBT time, the "beats" count is
			 * meter-dependent. So if we're in 4/4 time, the beats
			 * are quarters. If we're in 7/8 time, the beats are in
			 * 1/8 time, etc.
			 */

			if (beat_div == 1) {
				/* Advance beats by 1 meter-defined "beat */
				bbt = metric.bbt_add (bbt, BBT_Offset (0, 1, 0));
			} else {
				/* Advance beats by a fraction of the * meter-defined "beat"  */
				bbt = metric.bbt_add (bbt, BBT_Offset (0, 0, Temporal::ticks_per_beat / beat_div));
			}
		}

		DEBUG_TRACE (DEBUG::Grid, string_compose ("pre-check overrun of next point with bbt @ %1 audio %2 point %3\n", bbt, start, *p));

		bool reset = false;

		if (!mtp) {
			if (bbt == p->bbt()) {
				// DEBUG_TRACE (DEBUG::Grid, string_compose ("Gc %1\t       [%2]\n", metric, ret.back()));
				DEBUG_TRACE (DEBUG::Grid, string_compose ("we've reached the next point via BBT, BBT %1 audio %2 point %3\n", bbt, start, *p));
				reset = true;
			} else if (bbt > p->bbt()) {
				DEBUG_TRACE (DEBUG::Grid, string_compose ("we've passed the next point via BBT, BBT %1 audio %2 point %3\n", bbt, start, *p));
				reset = true;
			}
		} else {
			/* What does the current metric say about the audio
			   time position of @v bbt ?
			*/
			start = metric.superclock_at (bbt);
			if (start >= p->sclock()) {
				/* Yep, too far. So we need to reset and take
				   the next (music time point) into account.
				*/
				DEBUG_TRACE (DEBUG::Grid, string_compose ("we've reached/passed the next point via sclock, BBT %1 audio %2 point %3, using metric %4\n", bbt, start, *p, metric));
				reset = true;
			} else {
				DEBUG_TRACE (DEBUG::Grid, string_compose ("confirmed that BBT %1 has audio time %2 before next point %3\n", bbt, start, *p));
			}
		}

		DEBUG_TRACE (DEBUG::Grid, string_compose ("check overrun of next point, reset required ? %4 with bbt @ %1 audio %2 point %3\n", bbt, start, *p, (reset ? "YES" : "NO")));
		if (reset) {

			/* bbt is position for the next grid-line.
			 */

			TempoPoint const * tp = &metric.tempo();
			MeterPoint const * mp = &metric.meter();

			if (mtp) {

				/* BBT Markers/MusicTimePoints give the user a
				 * chance to "reset" the BBT ruler. We should
				 * do the same, unconditionally.
				 */
				tp = dynamic_cast<TempoPoint const *> (&*p);
				mp = dynamic_cast<MeterPoint const *> (&*p);

				TEMPO_MAP_ASSERT (tp);
				TEMPO_MAP_ASSERT (mp);

				metric = TempoMetric (*tp, *mp);
				DEBUG_TRACE (DEBUG::Grid, string_compose ("reset metric from music-time point %1, now %2\n", *mtp, metric));

				if (p->bbt().ticks != 0) {

					/* We do not want an arbitrary off-beat
					 * BBT marker to interrupt the grid. So
					 * round up from the marker's BBT time
					 * to the nearest appropriate beat/bar
					 * unit, and then reset from there.
					 */

					BBT_Time on_bar;

					if (bar_mod == 1) {
						on_bar = p->bbt().round_up_to_bar ();
					} else {
						on_bar = mp->round_up_to_beat_div (p->bbt(), beat_div);
					}

					bbt = BBT_Argument (metric.reftime(), on_bar);
					BBT_Offset delta = Temporal::bbt_delta (on_bar, p->bbt());

					if (delta != BBT_Offset ()) {
						Beats beats_delta = mp->to_quarters (delta);
						start = tp->superclock_at (tp->beats() + beats_delta);
						DEBUG_TRACE (DEBUG::Grid, string_compose ("reset start using bbt %1 as %2 via %3 (rounded by %4 beats %5)\n", p->bbt(), bbt, on_bar, delta, beats_delta));
					} else {
						start = p->sclock();
						DEBUG_TRACE (DEBUG::Grid, string_compose ("reset start using bbt %1 as %2 via %3 (rounded by %4)\n", p->bbt(), bbt, on_bar, delta));
					}

					DEBUG_TRACE (DEBUG::Grid, string_compose ("reset start to %1\n", start));

				} else {

					bbt = BBT_Argument (metric.reftime(), p->bbt());
					DEBUG_TRACE (DEBUG::Grid, string_compose ("reset start using bbt %1 as %2\n", p->bbt(), bbt));
					start = p->sclock();
					DEBUG_TRACE (DEBUG::Grid, string_compose ("reset start to %1\n", start));
				}

				/* Advance p to the next point */

				++p;

			} else {

				bool rebuild_metric = false;

				DEBUG_TRACE (DEBUG::Grid, string_compose ("iterating over points to find next, terminal is %1\n", bbt));

				if (p != _points.end()) {
					DEBUG_TRACE (DEBUG::Grid, string_compose ("\tstarting point is %1\n", *p));
				} else  {
					DEBUG_TRACE (DEBUG::Grid, "\treached end already\n");
				}

				/* Find all points at this BBT time (the next
				 * grid), then rebuild the TempoMetric with whatever
				 * we find, so that we will use that going forward.
				 */

				superclock_t sc = p->sclock();

				while (p != _points.end() && p->bbt() <= bbt && p->sclock() == sc) {

					TempoPoint const * tpp;
					MeterPoint const * mpp;

					if ((tpp = dynamic_cast<TempoPoint const *> (&(*p))) != 0) {
						rebuild_metric = true;
						tp = tpp;
					}

					if ((mpp = dynamic_cast<MeterPoint const *> (&(*p))) != 0) {
						rebuild_metric = true;
						mp = mpp;
					}

					++p;

					if (p != _points.end()) {
						DEBUG_TRACE (DEBUG::Grid, string_compose ("next point is %1\n", *p));
					} else {
						DEBUG_TRACE (DEBUG::Grid, "\tthat was that\n");
					}

				}

				/* reset the metric to use the most recent tempo & meter */

				if (rebuild_metric) {
					metric = TempoMetric (*tp, *mp);
					bbt = BBT_Argument (metric.reftime(), bbt);
					DEBUG_TRACE (DEBUG::Grid, string_compose ("second| with start = %1 aka %2 rebuilt metric from points, now %3\n", start, bbt, metric));
				} else {
					DEBUG_TRACE (DEBUG::Grid, string_compose ("not rebuilding metric, continuing with %1\n", metric));
				}
			}

		}

		start = metric.superclock_at (bbt);

		/* Update the quarter-note time value to match the BBT and
		 * audio time positions
		 */

		beats = metric.quarters_at (bbt);

		/* we have a candidate grid point (start,beats,bbt). It might
		 * not be within the range we're generating, but if it is, the iterator
		 * will need to know where the points_iterator and metric
		 * parameters ended up before we return..
		 */

		if (start >= rstart && start < end) {
			p_i = p;
			metric_i = metric;
		}

		DEBUG_TRACE (DEBUG::Grid, string_compose ("moved to %1 qn %2 sc %3) using metric %4 p at end %5\n", bbt, beats, start, metric, p == _points.end()));
	}
}

void
TempoMap::fill_grid_with_final_metric (TempoMapPoints& ret, TempoMetric metric, superclock_t start, superclock_t rstart, superclock_t end, int bar_mod, int beat_div, Beats beats, BBT_Time bbt) const
{
	DEBUG_TRACE (DEBUG::Grid, string_compose ("reached end, no more map points, use %5 to finish between %1 .. %2 initial bbt %3, beats %4\n", start, end, bbt, beats.str(), metric));

	while (start < end) {

		DEBUG_TRACE (DEBUG::Grid, string_compose ("bar mod %1 moved to %2 qn %3 sc %4)\n", bar_mod, bbt, beats, start));

		/* It is possible we already added the current BBT
		 * point, so check to avoid doubling up
		 */

		if (bar_mod != 0) {
			if ((start >= rstart) && bbt.is_bar() && (bar_mod == 1 || ((bbt.bars % bar_mod == 1)))) {
				ret.push_back (TempoMapPoint (*this, metric, start, beats, bbt));
				DEBUG_TRACE (DEBUG::Grid, string_compose ("GendA %1\t       %2\n", metric, ret.back()));
			}

			/* Advance by the number of bars specified by
			   bar_mod, then recompute the beats and
			   superclock position corresponding to that
			   BBT time.
			*/

			bbt.bars += bar_mod;

		} else {
			if (start >= rstart) {
				if (beat_div == 1) {
					ret.push_back (TempoMapPoint (*this, metric, start, beats, bbt));
					DEBUG_TRACE (DEBUG::Grid, string_compose ("Gendb %1\t       [%2]\n", metric, ret.back()));
				} else {
					int ticks = (bbt.beats * metric.meter().ticks_per_grid()) + bbt.ticks;
					int mod = Temporal::ticks_per_beat / beat_div;
					if ((ticks % mod) == 0) {
						ret.push_back (TempoMapPoint (*this, metric, start, beats, bbt));
						DEBUG_TRACE (DEBUG::Grid, string_compose ("Gendd %1\t       [%2]\n", metric, ret.back()));
					}
				}
			}

			/* move on by 1 meter-defined "beat" */

			if (beat_div == 1) {
				bbt = metric.bbt_add (bbt, BBT_Offset (0, 1, 0));
			} else {
				bbt = metric.bbt_add (bbt, BBT_Offset (0, 0, Temporal::ticks_per_beat / beat_div));
			}
		}

		/* compute audio and quarter-note time from the new BBT position */

		start = metric.superclock_at (bbt);
		beats = metric.quarters_at (bbt);
	}
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
		return str << t.note_types_per_minute() << " .. " << t.end_note_types_per_minute() << " 1/" << t.note_type() << " RAMPED notes per minute (" << t.superclocks_per_note_type() << " .. " << t.end_superclocks_per_note_type() << " sc-per-1/" << t.note_type() << ')';
	} else {
		return str << t.note_types_per_minute() << " 1/" << t.note_type() << " notes per minute (" << t.superclocks_per_note_type() << " sc-per-1/" << t.note_type() << ')';
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
	str << "MP @ "
	    << *((Point const *) &p) << ' '
	    << *((Tempo const *) &p) << ' '
	    << *((Meter const *) &p);
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
	str << '@' << std::setw (12) << tmp.sclock() << ' ' << tmp.sclock() / (double) superclock_ticks_per_second()
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
		str << " ramp omega(beats) = " << tmp.tempo().omega();
	}

	return str;
}

BBT_Argument
TempoMap::bbt_walk (BBT_Argument const & bbt, BBT_Offset const & o) const
{
	BBT_Offset offset (o);
	BBT_Time start (bbt);
	Tempos::const_iterator t, prev_t, next_t;
	Meters::const_iterator m, prev_m, next_m;

	TEMPO_MAP_ASSERT (!_tempos.empty());
	TEMPO_MAP_ASSERT (!_meters.empty());

	/* trivial (and common) case: single tempo, single meter */

	if (_tempos.size() == 1 && _meters.size() == 1) {
		return BBT_Argument (_meters.front().bbt_add (bbt, o));
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

	/* may have found tempo and/or meter precisely at the time given */

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

#define TEMPO_CHECK_FOR_NEW_METRIC \
	{ \
		/* need new metric */ \
		bool advance_t = false; \
		bool advance_m = false; \
		if (next_t != _tempos.end() && (start >= next_t->bbt())) { \
			advance_t = true; \
		}\
		if (next_m != _meters.end() && (start >= next_m->bbt())) { \
			advance_m = true; \
		} \
		if (advance_t && advance_m) { \
			metric = TempoMetric (*const_cast<TempoPoint*>(&*next_t), *const_cast<MeterPoint*>(&*next_m)); \
			++next_t; \
			++next_m; \
		} else if (advance_t && !advance_m) { \
			metric = TempoMetric (*const_cast<TempoPoint*>(&*next_t), metric.meter()); \
			++next_t; \
		} else if (advance_m && !advance_t) { \
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

	return BBT_Argument (metric.reftime(), start);
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
TempoMap::quarters_at (Temporal::BBT_Argument const & bbt) const
{
	return metric_at (bbt).quarters_at (bbt);
}

Temporal::Beats
TempoMap::quarters_at_superclock (superclock_t pos) const
{
	return metric_at (pos).quarters_at_superclock (pos);
}

XMLNode&
TempoMap::get_state () const
{
	XMLNode* node = new XMLNode (X_("TempoMap"));

	node->set_property (X_("superclocks-per-second"), superclock_ticks_per_second());

	XMLNode* children;

	children = new XMLNode (X_("Tempos"));
	node->add_child_nocopy (*children);
	for (Tempos::const_iterator t = _tempos.begin(); t != _tempos.end(); ++t) {
		if (!dynamic_cast<MusicTimePoint const *> (&(*t))) {
			children->add_child_nocopy (t->get_state());
		}
	}

	children = new XMLNode (X_("Meters"));
	node->add_child_nocopy (*children);
	for (Meters::const_iterator m = _meters.begin(); m != _meters.end(); ++m) {
		if (!dynamic_cast<MusicTimePoint const *> (& (*m))) {
			children->add_child_nocopy (m->get_state());
		}
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
	if (version <= 6000) {
		return set_state_3x (node);
	}

	/* global map properties */

	/* XXX this should probably be at the global level in the session file
	 * because it is the time unit for anything in the audio time domain,
	 * and affects a lot more than just the tempo map
	 */
	superclock_t sc;
	if (node.get_property (X_("superclocks-per-second"), sc)) {
		set_superclock_ticks_per_second (sc);
	}

	XMLNodeList const & children (node.children());

	/* XXX might be good to have a recovery mechanism in case setting
	 * things from XML fails. Not very likely, however.
	 */

	_tempos.clear ();
	_meters.clear ();
	_bartimes.clear ();
	_points.clear ();

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

	return 0;
}

int
TempoMap::set_music_times_from_state (XMLNode const& mt_node)
{
	XMLNodeList const & children (mt_node.children());

	try {
		for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {
			MusicTimePoint* mp = new MusicTimePoint (*this, **c);
			add_or_replace_bartime (mp);
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
	bool ignore;

	try {
		for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {
			TempoPoint* tp = new TempoPoint (*this, **c);
			core_add_tempo (tp, ignore);
			core_add_point (tp);
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
	bool ignore;

	try {
		for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {
			MeterPoint* mp = new MeterPoint (*this, **c);
			core_add_meter (mp, ignore);
			core_add_point (mp);
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

/** returns the duration (using the domain of @p pos) of the supplied BBT time at a specified sample position in the tempo map.
 * @param pos the frame position in the tempo map.
 * @param bbt the distance in BBT time from pos to calculate.
 * @param dir the rounding direction..
 * @return the timecnt_t that @p bbt represents when starting at @p pos, in
 * the time domain of @p pos
*/
timecnt_t
TempoMap::bbt_duration_at (timepos_t const & pos, BBT_Offset const & dur) const
{
	if (pos.time_domain() == AudioTime) {
		return timecnt_t::from_superclock (superclock_at (bbt_walk (bbt_at (pos), dur)) - pos.superclocks(), pos);
	}
	return timecnt_t (bbtwalk_to_quarters (pos.beats(), dur), pos);

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
	TEMPO_MAP_ASSERT (!_tempos.empty());
	TEMPO_MAP_ASSERT (!_meters.empty());

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
TempoMap::next_tempo (TempoPoint const & t) const
{
	Tempos::const_iterator i = _tempos.iterator_to (t);
	++i;

	if (i != _tempos.end()) {
		return &(*i);
	}

	return 0;
}

TempoPoint const *
TempoMap::previous_tempo (TempoPoint const & point) const
{
	Tempos::const_iterator i = _tempos.iterator_to (point);

	if (i == _tempos.begin()) {
		return 0;
	}

	--i;

	return &(*i);
}

MeterPoint const *
TempoMap::next_meter (MeterPoint const & t) const
{
	Meters::const_iterator i = _meters.iterator_to (t);
	++i;

	if (i != _meters.end()) {
		return &(*i);
	}

	return 0;
}

MeterPoint const *
TempoMap::previous_meter (MeterPoint const & point) const
{
	Meters::const_iterator i = _meters.iterator_to (point);

	if (i == _meters.begin()) {
		return 0;
	}

	--i;

	return &(*i);
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
TempoMap::metric_at (BBT_Argument const & bbt, bool can_match) const
{
	TempoPoint const * tp = 0;
	MeterPoint const * mp = 0;

	/* Since the reference time of a BBT_Argument is the time of the
	 * latest tempo/meter marker before or at BBT, we can use the reference
	 * time to get the metric.
	 */

	(void) get_tempo_and_meter (tp, mp, bbt, can_match, false);

	return TempoMetric (*tp, *mp);
}

bool
TempoMap::set_ramped (TempoPoint & tp, bool yn)
{
	TEMPO_MAP_ASSERT (!_tempos.empty());

	if (tp.ramped() == yn) {
		return false;
	}

	Tempos::iterator nxt = _tempos.begin();
	++nxt;

	for (Tempos::iterator t = _tempos.begin(); nxt != _tempos.end(); ++t, ++nxt) {
		if (tp == *t) {
			break;
		}
	}

	if (nxt == _tempos.end()) {
		return false;
	}

	if (yn) {
		tp.set_end_npm (nxt->end_note_types_per_minute());
	} else {
		tp.set_end_npm (tp.note_types_per_minute());
	}

	reset_starting_at (tp.sclock());

	return true;
}

bool
TempoMap::set_continuing (TempoPoint& tp, bool yn)
{
	if (!yn) {
		tp.set_continuing (false);
		return true; /* change made */
	}

	TempoPoint const * prev = previous_tempo (tp);

	if (!prev) {
		return false;
	}

	tp.set_note_types_per_minute (prev->note_types_per_minute());

	return true;
}

#if 1
void
TempoMap::stretch_tempo (TempoPoint& focus, double tempo_value)
{
	/* Our goal is to alter the outound tempo at @param focus and at the same
	 * time create & modify a ramp between the previous tempo and @param focus
	 * so that @param remains in the same location.
	 *
	 * The user has placed @param focus at the correct point, but wanfocus to
	 * adjust the (outbound) tempo without creating an obvious step change
	 * at @param focus. So we want to ramp from prev to focus
	 */

	TempoPoint* prev = const_cast<TempoPoint*> (previous_tempo (focus));

	TempoPoint old_prev (*prev);
	TempoPoint old_focus (focus);

	focus.set_note_types_per_minute (tempo_value);
	focus.set_end_npm (tempo_value);

	prev->set_end_npm (tempo_value);
	prev->compute_omega_from_next_tempo (focus);

	superclock_t err = prev->superclock_at (focus.beats()) - focus.sclock();
	const superclock_t one_sample = superclock_ticks_per_second() / TEMPORAL_SAMPLE_RATE;
	// const double end_scpqn = focus.superclocks_per_quarter_note();
	double scpqn = focus.superclocks_per_quarter_note ();
	double new_npm;
	int cnt = 0;

	reset_starting_at (prev->sclock());;
	return;

	while (std::abs(err) >= one_sample) {

		if (err > 0) {
			/* estimated > actual: speed end tempo up a little aka
			   reduce scpqn
			*/
			scpqn *= 0.99;
		} else {
			/* estimated < actual: reduce end tempo a little, aka
			   increase scpqn
			*/
			scpqn *= 1.01;
		}

		if (scpqn < 1.0) {
			/* mathematically too small, bail out */
			*prev = old_prev;
			focus = old_focus;
			return;
		}

		/* Convert scpqn to notes-per-minute */

		new_npm = ((superclock_ticks_per_second() * 60.0) / scpqn) * (focus.note_type() / 4.0);

		/* limit range of possible discovered tempo */

		if (new_npm < 4.0 && new_npm > 400) {
			/* too low of a tempo for our taste, bail out */
			*prev = old_prev;
			focus = old_focus;
			return;
		}

		/* set the (initial) tempo, recompute omega and then compute
		 * the (new) error (distance between the predicted position of
		 * the next marker and its actual (fixed) position.
		 */

		focus.set_note_types_per_minute (new_npm);
		focus.set_end_npm (new_npm);
		prev->set_end_npm (new_npm);
		prev->compute_omega_from_next_tempo (focus);
		err = prev->superclock_at (focus.beats()) - focus.sclock();
		++cnt;
	}

	// std::cerr << "that took " << cnt << " iterations to get to < 1 sample\n";
	// std::cerr << "final focus: " << focus << std::endl;
	// std::cerr << "final prev: " << *prev << std::endl;

	reset_starting_at (prev->sclock());
	// dump (std::cerr);
}

#else

/* Adjusts the outgoing tempo at @p ts so that the next Tempo point is at @p
 * end_sample, while keeping the beat time positions of both the same.
 *
 * i.e. literally "stretches" out a tempo section (between two markers) by
 * speeding or slowing the initial outbound tempo and ramping to the end
 * tempo.
 */

void
TempoMap::stretch_tempo (TempoPoint* ts, samplepos_t sample, samplepos_t end_sample, Beats const & start_qnote, Beats const & end_qnote)
{
	/*
	  Ts (future prev_t)   Tnext
	  |                    |
	  |     [drag^]        |
	  |----------|----------
	        e_f  qn_beats(sample)
	*/

	if (!ts) {
		return;
	}

	TempoPoint* next_t = const_cast<TempoPoint*> (next_tempo (*ts));

	/* no stretching of the final tempo, where final includes "terminated
	 * by a BBT marker"
	 */
	if (!next_t || dynamic_cast<MusicTimePoint*> (next_t)) {
		return;
	}

	superclock_t start_sclock = samples_to_superclock (sample, TEMPORAL_SAMPLE_RATE);
	superclock_t end_sclock = samples_to_superclock (end_sample, TEMPORAL_SAMPLE_RATE);

	/* minimum allowed measurement distance in samples */
	const superclock_t min_delta_sclock = samples_to_superclock (2, TEMPORAL_SAMPLE_RATE);
	double new_bpm;

	if (ts->continuing()) {

		/* this tempo point is required to start using the same bpm
		 * that the previous tempo ended with.
		 */

		TempoPoint* prev_to_ts = const_cast<TempoPoint*> (previous_tempo (*ts));
		TEMPO_MAP_ASSERT (prev_to_ts);
		/* the change in samples is the result of changing the slope of at most 2 previous tempo sections.
		 * constant to constant is straightforward, as the tempo prev to ts has constant slope.
		 */
		double contribution = 0.0;
		if (next_t && prev_to_ts->ramped()) {
			const DoubleableBeats delta_tp = ts->beats() - prev_to_ts->beats();
			const DoubleableBeats delta_np = next_t->beats() - prev_to_ts->beats();
			contribution = delta_tp.to_double() / delta_np.to_double();
		}
		samplepos_t const fr_off = end_sclock - start_sclock;
		sampleoffset_t const ts_sample_contribution = fr_off - (contribution * (double) fr_off);

		if (start_sclock > prev_to_ts->sclock() + min_delta_sclock && (start_sclock + ts_sample_contribution) > prev_to_ts->sclock() + min_delta_sclock) {
			DoubleableBeats delta_sp = start_qnote - prev_to_ts->beats();
			DoubleableBeats delta_ep = end_qnote - prev_to_ts->beats();
			new_bpm = ts->note_types_per_minute() * (delta_sp.to_double() / delta_ep.to_double());
		} else {
			new_bpm = ts->note_types_per_minute();
		}

	} else {

		/* ts is free to have it's bpm changed to any value (within limits) */

		if (start_sclock > ts->sclock() + min_delta_sclock && end_sclock > ts->sclock() + min_delta_sclock) {
			new_bpm = ts->note_types_per_minute() * ((start_sclock - ts->sclock()) / (double) (end_sclock - ts->sclock()));
		} else {
			new_bpm = ts->note_types_per_minute();
		}

		new_bpm = std::min (new_bpm, 1000.0);
	}

	/* don't clamp and proceed here.
	   testing has revealed that this can go negative,
	   which is an entirely different thing to just being too low.
	*/

	if (new_bpm < 0.5) {
		return;
	}

	ts->set_note_types_per_minute (new_bpm);

	if (ts->continuing()) {
		TempoPoint* prev = 0;
		if ((prev = const_cast<TempoPoint*> (previous_tempo (*ts))) != 0) {
			prev->set_end_npm (ts->end_note_types_per_minute());
		}
	}

	reset_starting_at (ts->sclock() + 1);
}

#endif

void
TempoMap::stretch_tempo_end (TempoPoint* ts, samplepos_t sample, samplepos_t end_sample)
{
	/*
	  Ts (future prev_t)   Tnext
	  |                    |
	  |     [drag^]        |
	  |----------|----------
	        e_f  qn_beats(sample)
	*/

	if (!ts) {
		return;
	}

	const superclock_t start_sclock = samples_to_superclock (sample, TEMPORAL_SAMPLE_RATE);
	const superclock_t end_sclock = samples_to_superclock (end_sample, TEMPORAL_SAMPLE_RATE);

	TempoPoint * prev_t = const_cast<TempoPoint*> (previous_tempo (*ts));

	if (!prev_t) {
		return;
	}

	/* minimum allowed measurement distance in superclocks */
	const superclock_t min_delta_sclock = samples_to_superclock (2, TEMPORAL_SAMPLE_RATE);
	double new_bpm;

	if (start_sclock > prev_t->sclock() + min_delta_sclock && end_sclock > prev_t->sclock() + min_delta_sclock) {
		new_bpm = prev_t->end_note_types_per_minute() * ((prev_t->sclock() - start_sclock) / (double) (prev_t->sclock() - end_sclock));
	} else {
		new_bpm = prev_t->end_note_types_per_minute();
	}

	new_bpm = std::min (new_bpm, (double) 1000.0);

	if (new_bpm < 0.5) {
		return;
	}

	prev_t->set_end_npm (new_bpm);

	if (ts->continuing()) {
		ts->set_note_types_per_minute (prev_t->note_types_per_minute());
	}

	reset_starting_at (prev_t->sclock());
}

bool
TempoMap::solve_ramped_twist (TempoPoint& earlier, TempoPoint& later)
{
	superclock_t err = earlier.superclock_at (later.beats()) - later.sclock();
	const superclock_t one_sample = superclock_ticks_per_second() / TEMPORAL_SAMPLE_RATE;
	double end_scpqn = earlier.end_superclocks_per_quarter_note();
	double new_end_npm;
	int cnt = 0;

	while (std::abs(err) >= one_sample) {

		if (err > 0) {
			/* estimated > actual: speed end tempo up a little aka
			   reduce scpqn
			*/
			end_scpqn *= 0.99;
		} else {
			/* estimated < actual: reduce end tempo a little, aka
			   increase scpqn
			*/
			end_scpqn *= 1.01;
		}

		if (end_scpqn < 1.0) {
			/* mathematically too small, bail out */
			return false;
		}

		/* Convert scpqn to notes-per-minute */

		new_end_npm = ((superclock_ticks_per_second() * 60.0) / end_scpqn) * (earlier.note_type() / 4.0);

		/* limit range of possible discovered tempo */

		if (new_end_npm < 4.0 && new_end_npm > 400) {
			/* too low of a tempo for our taste, bail out */
			return false;
		}

		/* set the (initial) tempo, recompute omega and then compute
		 * the (new) error (distance between the predicted position of
		 * the later marker and its actual (fixed) position.
		 */

		earlier.set_end_npm (new_end_npm);
		earlier.compute_omega_from_next_tempo (later);
		err = earlier.superclock_at (later.beats()) - later.sclock();

		if (cnt > 20000) {
			// std::cerr << "nn: " << new_end_npm << " err " << err << " @ " << cnt << "solve_ramped_twist FAILED\n";
			return false;
		}

		++cnt;
	}

	// std::cerr << "that took " << cnt << " iterations to get to < 1 sample\n";

	return true;
}

bool
TempoMap::solve_constant_twist (TempoPoint& earlier, TempoPoint& later)
{
	superclock_t err = earlier.superclock_at (later.beats()) - later.sclock();
	const superclock_t one_sample = superclock_ticks_per_second() / TEMPORAL_SAMPLE_RATE;
	double start_npm = earlier.superclocks_per_quarter_note ();
	int cnt = 0;

	while (std::abs(err) >= one_sample) {

		if (err > 0) {
			/* estimated > actual: speed end tempo up a little aka
			   reduce scpqn
			*/
			start_npm *= 0.99;
		} else {
			/* estimated < actual: reduce end tempo a little, aka
			   increase scpqn
			*/
			start_npm *= 1.01;
		}

		/* Convert scpqn to notes-per-minute */

		double new_npm = ((superclock_ticks_per_second() * 60.0) / start_npm) * (earlier.note_type() / 4.0);

		/* limit range of possible discovered tempo */

		if (new_npm < 4.0 && new_npm > 400) {
			/* too low of a tempo for our taste, bail out */
			return false;
		}

		/* set the (initial) tempo, and then compute
		 * the (new) error (distance between the predicted position of
		 * the later marker and its actual (fixed) position.
		 */
		earlier.set_note_types_per_minute (new_npm);
		earlier.set_end_npm (new_npm);
		err = earlier.superclock_at (later.beats()) - later.sclock();

		if (cnt > 20000) {
			// std::cerr << "nn: " << new_npm << " err " << err << " @ " << cnt << "solve_constant_twist FAILED\n";
			return false;
		}

		++cnt;
	}

	// std::cerr << "that took " << cnt << " iterations to get to < 1 sample\n";

	return true;
}

void
TempoMap::constant_twist_tempi (TempoPoint& prev, TempoPoint& focus, TempoPoint& next, double tempo_value)
{
	/* Check if the new tempo value is within an acceptable range */

	if (tempo_value < 4.0 || tempo_value > 400) {
		std::cerr << "can't set tempo to " << tempo_value << " ....fail\n";
		return;
	}

	TempoPoint old_prev (prev);
	TempoPoint old_focus (focus);

	/* Our job here is to reposition @param focus without altering the
	 * positions of @param prev and @param next. We do this by changing
	 * the tempo of prev (as opposed to ramped_twist_tempi, below )
	 */

	/* set a fixed tempo for the previous marker (this results in 'focus' moving a bit with the mouse) */
	prev.set_note_types_per_minute (tempo_value);
	prev.set_end_npm (tempo_value);

	/* reposition focus, using prev to define audio time; leave beat time
	 * and BBT alone
	 */

	focus.set (prev.superclock_at (focus.beats()), focus.beats(), focus.bbt());

	/* Now iteratively adjust focus.superclocks_per_quarter_note() (the
	 * section's starting tempo) so that next.sclock() remains within 1
	 * sample of its current position
	 */

	if (!solve_constant_twist (focus, next)) {
		prev = old_prev;
		focus = old_focus;
		return;
	}

}

void
TempoMap::ramped_twist_tempi (TempoPoint& unused, TempoPoint& focus, TempoPoint& next, double tempo_value)
{
	/* Check if the new tempo value is within an acceptable range */

	if (tempo_value < 4.0 || tempo_value > 400) {
		return;
	}

	/* Our job here is to tweak the ramp of @param focus without
	 * altering the positions of @param focus and @param next.
	 * We are "twisting" the tempo section between those markers
	 * to enact a change but without moving the markers themselves
	 *
	 * Start by saving the current state of focus in case we need
	 * to bail out because change is impossible.
	 */

	TempoPoint old_focus (focus);

	/* set start tempo of prev tempo marker; we will iteratively solve for the required ramp value */
	focus.set_note_types_per_minute (tempo_value);

	if (!solve_ramped_twist (focus, next)) {
		focus = old_focus;
		return;
	}
}

void
TempoMap::init ()
{
	WritableSharedPtr new_map (new TempoMap ());
	_map_mgr.init (new_map);
	fetch ();
}

TempoMap::WritableSharedPtr
TempoMap::write_copy()
{
	return _map_mgr.write_copy();
}

int
TempoMap::update (TempoMap::WritableSharedPtr m)
{
	if (!_map_mgr.update (m)) {
		return -1;
	}

	/* update thread local map pointer in the calling thread */
	update_thread_tempo_map ();

#ifndef NDEBUG
	if (DEBUG_ENABLED (DEBUG::TemporalMap)) {
		m->dump (std::cerr);
	}
#endif

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
TempoMap::midi_clock_beat_at_or_after (samplepos_t const pos, samplepos_t& clk_pos, uint32_t& clk_beat) const
{
	/* Sequences are always assumed to start on a MIDI Beat of 0 (ie, the downbeat).
	 *
	 * There are 24 MIDI clock per quarter note (1 Temporal::Beat)
	 *
	 * from http://midi.teragonaudio.com/tech/midispec/seq.htm
	 */
	superclock_t sc (samples_to_superclock (pos, TEMPORAL_SAMPLE_RATE));
	TempoPoint const & tp (tempo_at (sc));
	Temporal::Beats b = (tp.quarters_at_sample (pos)).round_up_to_beat ();

  again:
	/* We cannot use
	 *    clk_pos = sample_at (b);
	 * because in this case we have to round up to the start
	 * of the next tick, not round to to the current tick.
	 * (compare to 14da117bc88)
	 */
	clk_pos = PBD::muldiv_round (superclock_at (b), TEMPORAL_SAMPLE_RATE, superclock_ticks_per_second ());

	/* Each MIDI Beat spans 6 MIDI Clocks.
	 * In other words, each MIDI Beat is a 16th note (since there are 24 MIDI
	 * Clocks in a quarter note, therefore 4 MIDI Beats also fit in a quarter).
	 * So, a master can sync playback to a resolution of any particular 16th note.
	 */
	clk_beat = b.get_beats () * 4 ; // 4 = 24 / 6;

	/* It can happen that the computed beat is actually slightly before
	 * pos. For example, if @p pos is 441002, this is less than 1 tick
	 * beyond beat 20 (at 120bpm, 44100Hz, 4/4). The call to
	 * quarters_at_sample(pos) will return 20:0, which does not (and
	 * cannot) be rounded up). But when we convert that back to a sample
	 * time, we get 441000, which is before the time (@p pos) that we are
	 * meant to be computing for. So .. advance to the next beat, and do it
	 * again.
	 */

	if (clk_pos < pos) {
		b += Beats (1, 0);
		goto again;
	}
}

/******** OLD STATE LOADING CODE SECTION *************/

#if 0
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
#endif

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
			return -1;
		}
	} else {
		/* older session, make note type be quarter by default */
		lts.note_type = 4.0;
	}

	/* older versions used "clamped" as the property name here */

	if (!node.get_property ("clamped", lts.continuing)) {
		lts.continuing = false;
	}

	if (node.get_property ("end-beats-per-minute", lts.end_note_types_per_minute)) {
		if (lts.end_note_types_per_minute < 0.0) {
			info << _("TempoSection XML node has an illegal \"end-beats-per-minute\" value") << endmsg;
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
	bool initial_tempo_at_zero = true;
	bool initial_meter_at_zero = true;

	for (niter = nlist.begin(), index = 0; niter != nlist.end(); ++niter, ++index) {
		XMLNode* child = *niter;

		if ((initial_tempo_index < 0) && (child->name() == Tempo::xml_node_name)) {

			LegacyTempoState lts;

			if (parse_tempo_state_3x (*child, lts)) {
				error << _("Tempo map: could not set new state, restoring old one.") << endmsg;
				break;
			}

			if (lts.sample != 0) {
				initial_tempo_at_zero = false;
			}

			Tempo t (lts.note_types_per_minute,
			         lts.end_note_types_per_minute,
			         lts.note_type);
			TempoPoint* tp = new TempoPoint (*this, t, samples_to_superclock (0, TEMPORAL_SAMPLE_RATE), Beats(), BBT_Time());

			tp->set_continuing (lts.continuing);

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

			if (lms.sample != 0) {
				initial_meter_at_zero = false;
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
			if (index == initial_tempo_index) {
				if (!initial_tempo_at_zero) {
					/* already added */
					continue;
				}
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
				if (!initial_meter_at_zero) {
					/* Add a BBT point to fix the meter location */
					set_bartime (lms.bbt, timepos_t (lms.sample));
				} else {
					continue;
				}
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
					error << string_compose (_("Multiple meter definitions found at %1"), prev_m->beat()) << endmsg;
					return -1;
				}
			} else if ((prev_t = dynamic_cast<TempoSection*>(*prev)) != 0 && (ts = dynamic_cast<TempoSection*>(*i)) != 0) {
				if (prev_t->pulse() == ts->pulse()) {
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

void
TempoMap::map_assert (bool expr, char const * exprstr, char const * file, int line)
{
	if (!expr) {
		TempoMap::SharedPtr map = TempoMap::use();
		std::cerr << "TEMPO MAP LOGIC FAILURE: [" << exprstr << "] at " << file << ':' << line << std::endl;
		map->dump (std::cerr);
		abort ();
	}
}

double
TempoMap::max_notes_per_minute() const
{
	double npm = 0;
	for (auto const & t : _tempos) {
		if (t.note_types_per_minute() > npm) {
			npm = t.note_types_per_minute();
		}
		if (t.end_note_types_per_minute() > npm) {
			npm = t.end_note_types_per_minute();
		}
	}

	return npm;
}

double
TempoMap::min_notes_per_minute() const
{
	double npm = std::numeric_limits<double>::max();

	for (auto const & t : _tempos) {
		if (t.note_types_per_minute() < npm) {
			npm = t.note_types_per_minute();
		}
		if (t.end_note_types_per_minute() < npm) {
			npm = t.end_note_types_per_minute();
		}
	}

	return npm;
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
			t->set_end_npm (t->note_types_per_minute());

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
					prev_t->set_end_npm (t->note_types_per_minute());
				}
			}

			prev_t = t;
		}
	}

	if (prev_t) {
		prev_t->set_end_npm (prev_t->note_types_per_minute());
	}
}

#endif

TempoCommand::TempoCommand (XMLNode const & node)
	: _before (0)
	, _after (0)
{
	if (!node.get_property (X_("name"), _name)) {
		throw failed_constructor();
	}

	XMLNodeList const & children (node.children());
	for (XMLNodeList::const_iterator n = children.begin(); n != children.end(); ++n) {
		if ((*n)->name() == X_("before")) {
			if ((*n)->children().empty()) {
				throw failed_constructor();
			}
			_before = new XMLNode (*(*n)->children().front());
		} else if ((*n)->name() == X_("after")) {
			if ((*n)->children().empty()) {
				throw failed_constructor();
			}
			_after = new XMLNode (*(*n)->children().front());
		}
	}

	if (!_before || !_after) {
		throw failed_constructor();
	}
}

TempoCommand::TempoCommand (std::string const & str, XMLNode const * before, XMLNode const * after)
	: _name (str)
	, _before (before)
	, _after (after)
{

}

TempoCommand::~TempoCommand ()
{
	delete _before;
	delete _after;
}

XMLNode&
TempoCommand::get_state() const
{
	XMLNode* node = new XMLNode (X_("TempoCommand"));
	node->set_property (X_("name"), _name);

	if (_before) {
		XMLNode* b = new XMLNode (X_("before"));
		b->add_child_copy (*_before);
		node->add_child_nocopy (*b);
	}

	if (_after) {
		XMLNode* a = new XMLNode (X_("after"));
		a->add_child_copy (*_after);
		node->add_child_nocopy (*a);
	}

	return *node;
}

void
TempoCommand::undo ()
{
	if (!_before) {
		return;
	}

	TempoMap::WritableSharedPtr map (TempoMap::write_copy());
	map->set_state (*_before, Stateful::current_state_version);
	TempoMap::update (map);
}

void
TempoCommand::operator() ()
{
	if (!_after) {
		return;
	}

	TempoMap::WritableSharedPtr map (TempoMap::write_copy());
	map->set_state (*_after, Stateful::current_state_version);
	TempoMap::update (map);
}

TempoMapCutBuffer::TempoMapCutBuffer (timecnt_t const & dur)
	: _start_tempo (nullptr)
	, _end_tempo (nullptr)
	, _start_meter (nullptr)
	, _end_meter (nullptr)
	, _duration (dur)
{
}

TempoMapCutBuffer::~TempoMapCutBuffer ()
{
	delete _start_tempo;
	delete _end_tempo;
	delete _start_meter;
	delete _end_meter;
}

void
TempoMapCutBuffer::add_start_tempo (Tempo const & t)
{
	delete _start_tempo;
	_start_tempo = new Tempo (t);
}

void
TempoMapCutBuffer::add_end_tempo (Tempo const & t)
{
	delete _end_tempo;
	_end_tempo = new Tempo (t);
}

void
TempoMapCutBuffer::add_start_meter (Meter const & t)
{
	delete _start_meter;
	_start_meter = new Meter (t);
}

void
TempoMapCutBuffer::add_end_meter (Meter const & t)
{
	delete _end_meter;
	_end_meter = new Meter (t);
}

void
TempoMapCutBuffer::dump (std::ostream& ostr)
{
	ostr << "TempoMapCutBuffer @ " << this << std::endl;

	if (_start_tempo) {
		ostr << "Start Tempo: " << *_start_tempo << std::endl;
	}
	if (_end_tempo) {
		ostr << "End Tempo: " << *_end_tempo << std::endl;
	}
	if (_start_meter) {
		ostr << "Start Meter: " << *_start_meter << std::endl;
	}
	if (_end_meter) {
		ostr << "End Meter: " << *_end_meter << std::endl;
	}

	ostr << "Tempos:\n";

	for (auto const & t : _tempos) {
		ostr << '\t' << &t << ' ' << t << std::endl;
	}

	ostr << "Meters:\n";

	for (auto const & m : _meters) {
		ostr << '\t' << &m << ' ' << m << std::endl;
	}
}

void
TempoMapCutBuffer::add (TempoPoint const & tp)
{
	TempoPoint* ntp = new TempoPoint (tp);

	/* We must reset the audio and beat time position, but we can't do
	 * anything useful with the BBT time designation.
	 */

	ntp->set (ntp->sclock() - _duration.position().superclocks(),
	          ntp->beats() - _duration.position().beats(),
	          ntp->bbt());

	_tempos.push_back (*ntp);
	_points.push_back (*ntp);
}

void
TempoMapCutBuffer::add (MeterPoint const & mp)
{
	MeterPoint* ntp = new MeterPoint (mp);

	/* We must reset the audio and beat time position, but we can't do
	 * anything useful with the BBT time designation.
	 */

	ntp->set (ntp->sclock() - _duration.position().superclocks(),
	          ntp->beats() - _duration.position().beats(),
	          ntp->bbt());

	_meters.push_back (*ntp);
	_points.push_back (*ntp);
}

void
TempoMapCutBuffer::add (MusicTimePoint const & mtp)
{
	MusicTimePoint* ntp = new MusicTimePoint (mtp);

	/* We must reset the audio and beat time position, but we can't do
	 * anything useful with the BBT time designation.
	 */

	ntp->set (ntp->sclock() - _duration.position().superclocks(),
	          ntp->beats() - _duration.position().beats(),
	          ntp->bbt());

	_bartimes.push_back (*ntp);
	_tempos.push_back (*ntp);
	_meters.push_back (*ntp);
	_points.push_back (*ntp);
}

void
TempoMapCutBuffer::clear ()
{
	_tempos.clear ();
	_meters.clear ();
	_bartimes.clear ();
	_points.clear ();
}
