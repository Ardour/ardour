/*
    Copyright (C) 2000-2002 Paul Davis

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
#include <stdexcept>
#include <cmath>

#include <unistd.h>

#include <glibmm/threads.h>

#include "pbd/enumwriter.h"
#include "pbd/xml++.h"
#include "evoral/Beats.hpp"

#include "ardour/debug.h"
#include "ardour/lmath.h"
#include "ardour/tempo.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

using Timecode::BBT_Time;

/* _default tempo is 4/4 qtr=120 */

Meter    TempoMap::_default_meter (4.0, 4.0);
Tempo    TempoMap::_default_tempo (120.0);

/***********************************************************************/

double
Meter::frames_per_grid (const Tempo& tempo, framecnt_t sr) const
{
	/* This is tempo- and meter-sensitive. The number it returns
	   is based on the interval between any two lines in the
	   grid that is constructed from tempo and meter sections.

	   The return value IS NOT interpretable in terms of "beats".
	*/

	return (60.0 * sr) / (tempo.beats_per_minute() * (_note_type/tempo.note_type()));
}

double
Meter::frames_per_bar (const Tempo& tempo, framecnt_t sr) const
{
	return frames_per_grid (tempo, sr) * _divisions_per_bar;
}


/***********************************************************************/

const string TempoSection::xml_state_node_name = "Tempo";

TempoSection::TempoSection (const XMLNode& node)
	: MetricSection (0.0), Tempo (TempoMap::default_tempo())
{
	const XMLProperty *prop;
	LocaleGuard lg;
	BBT_Time bbt;
	double beat;
	uint32_t frame;

	if ((prop = node.property ("start")) != 0) {
		if (sscanf (prop->value().c_str(), "%" PRIu32 "|%" PRIu32 "|%" PRIu32,
			    &bbt.bars,
			    &bbt.beats,
			    &bbt.ticks) == 3) {
			/* legacy session - start used to be in bbt*/
			_legacy_bbt = bbt;
			beat = -1.0;
			set_beat (beat);
		}
	} else {
		warning << _("TempoSection XML node has no \"start\" property") << endmsg;
	}


	if ((prop = node.property ("beat")) != 0) {
		if (sscanf (prop->value().c_str(), "%lf", &beat) != 1 || beat < 0.0) {
			error << _("TempoSection XML node has an illegal \"beat\" value") << endmsg;
		} else {
			set_beat (beat);
		}
	}
	if ((prop = node.property ("frame")) != 0) {
		if (sscanf (prop->value().c_str(), "%" PRIu32, &frame) != 1) {
			error << _("TempoSection XML node has an illegal \"frame\" value") << endmsg;
		} else {
			set_frame (frame);
		}
	}

	if ((prop = node.property ("beats-per-minute")) == 0) {
		error << _("TempoSection XML node has no \"beats-per-minute\" property") << endmsg;
		throw failed_constructor();
	}

	if (sscanf (prop->value().c_str(), "%lf", &_beats_per_minute) != 1 || _beats_per_minute < 0.0) {
		error << _("TempoSection XML node has an illegal \"beats_per_minute\" value") << endmsg;
		throw failed_constructor();
	}

	if ((prop = node.property ("note-type")) == 0) {
		/* older session, make note type be quarter by default */
		_note_type = 4.0;
	} else {
		if (sscanf (prop->value().c_str(), "%lf", &_note_type) != 1 || _note_type < 1.0) {
			error << _("TempoSection XML node has an illegal \"note-type\" value") << endmsg;
			throw failed_constructor();
		}
	}

	if ((prop = node.property ("movable")) == 0) {
		error << _("TempoSection XML node has no \"movable\" property") << endmsg;
		throw failed_constructor();
	}

	set_movable (string_is_affirmative (prop->value()));

	if ((prop = node.property ("bar-offset")) == 0) {
		_bar_offset = -1.0;
	} else {
		if (sscanf (prop->value().c_str(), "%lf", &_bar_offset) != 1 || _bar_offset < 0.0) {
			error << _("TempoSection XML node has an illegal \"bar-offset\" value") << endmsg;
			throw failed_constructor();
		}
	}

	if ((prop = node.property ("tempo-type")) == 0) {
		_type = Constant;
	} else {
		_type = Type (string_2_enum (prop->value(), _type));
	}

	if ((prop = node.property ("lock-style")) == 0) {
		set_position_lock_style (MusicTime);
	} else {
		set_position_lock_style (PositionLockStyle (string_2_enum (prop->value(), position_lock_style())));
	}
}

XMLNode&
TempoSection::get_state() const
{
	XMLNode *root = new XMLNode (xml_state_node_name);
	char buf[256];
	LocaleGuard lg;

	snprintf (buf, sizeof (buf), "%f", beat());
	root->add_property ("beat", buf);
	snprintf (buf, sizeof (buf), "%li", frame());
	root->add_property ("frame", buf);
	snprintf (buf, sizeof (buf), "%f", _beats_per_minute);
	root->add_property ("beats-per-minute", buf);
	snprintf (buf, sizeof (buf), "%f", _note_type);
	root->add_property ("note-type", buf);
	// snprintf (buf, sizeof (buf), "%f", _bar_offset);
	// root->add_property ("bar-offset", buf);
	snprintf (buf, sizeof (buf), "%s", movable()?"yes":"no");
	root->add_property ("movable", buf);
	root->add_property ("tempo-type", enum_2_string (_type));
	root->add_property ("lock-style", enum_2_string (position_lock_style()));

	return *root;
}

void

TempoSection::update_bar_offset_from_bbt (const Meter& m)
{
	_bar_offset = (beat() * BBT_Time::ticks_per_beat) /
		(m.divisions_per_bar() * BBT_Time::ticks_per_beat);

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("Tempo set bar offset to %1 from %2 w/%3\n", _bar_offset, beat(), m.divisions_per_bar()));
}

void
TempoSection::set_type (Type type)
{
	_type = type;
}

/** returns the tempo at the zero-based (relative to tempo section) frame.
*/
double
TempoSection::tempo_at_frame (framepos_t frame, framecnt_t frame_rate) const
{

	if (_type == Constant) {
		return beats_per_minute();
	}

	return tick_tempo_at_time (frame_to_minute (frame, frame_rate)) / BBT_Time::ticks_per_beat;
}

/** returns the zero-based frame (relative to tempo section)
   where the tempo occurs.
*/
framepos_t
TempoSection::frame_at_tempo (double bpm, framecnt_t frame_rate) const
{
	if (_type == Constant) {
		return 0;
	}

	return minute_to_frame (time_at_tick_tempo (bpm *  BBT_Time::ticks_per_beat), frame_rate);
}

/** returns the zero-based tick (relative to tempo section)
   where the zero-based frame (relative to tempo section)
   lies.
*/
double
TempoSection::tick_at_frame (framepos_t frame, framecnt_t frame_rate) const
{
	if (_type == Constant) {
		return (frame / frames_per_beat (frame_rate)) * BBT_Time::ticks_per_beat;
	}

	return tick_at_time (frame_to_minute (frame, frame_rate));
}

/** returns the zero-based frame (relative to tempo section)
   where the zero-based tick (relative to tempo section)
   falls.
*/
framepos_t
TempoSection::frame_at_tick (double tick, framecnt_t frame_rate) const
{
	if (_type == Constant) {
		return (framepos_t) floor ((tick  / BBT_Time::ticks_per_beat) * frames_per_beat (frame_rate));
	}

	return minute_to_frame (time_at_tick (tick), frame_rate);
}

/** returns the zero-based beat (relative to tempo section)
   where the zero-based frame (relative to tempo section)
   lies.
*/
double
TempoSection::beat_at_frame (framepos_t frame, framecnt_t frame_rate) const
{
	return tick_at_frame (frame, frame_rate) / BBT_Time::ticks_per_beat;
}

/** returns the zero-based frame (relative to tempo section start frame)
   where the zero-based beat (relative to tempo section start)
   falls.
*/

framepos_t
TempoSection::frame_at_beat (double beat, framecnt_t frame_rate) const
{
	return frame_at_tick (beat * BBT_Time::ticks_per_beat, frame_rate);
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

with function constant
c = log(Ta/T0)/a
so
a = log(Ta/T0)/c

The integral over t of our Tempo function (the beat function, which is the duration in beats at some time t) is:
b(t) = T0(e^(ct) - 1) / c

To find the time t at beat duration b, we use the inverse function of the beat function (the time function) which can be shown to be:
t(b) = log((cb / T0) + 1) / c

The time t at which Tempo T occurs is a as above:
t(T) = log(T / T0) / c

We define c for this tempo ramp by placing a new tempo section at some time t after this one.
Our problem is that we usually don't know t.
We almost always know the duration in beats between this and the new section, so we need to find c in terms of the beat function.
Where a = t (i.e. when a is equal to the time of the next tempo section), the beat function reveals:
t = b log (Ta / T0) / (T0 (e^(log (Ta / T0)) - 1))

By substituting our expanded t as a in the c function above, our problem is reduced to:
c = T0 (e^(log (Ta / T0)) - 1) / b

We can now store c for future time calculations.
If the following tempo section (the one that defines c in conjunction with this one)
is changed or moved, c is no longer valid.

Most of this stuff is taken from this paper:

WHERE’S THE BEAT?
TOOLS FOR DYNAMIC TEMPO CALCULATIONS
Jan C. Schacher
Martin Neukom
Zurich University of Arts
Institute for Computer Music and Sound Technology

https://www.zhdk.ch/fileadmin/data_subsites/data_icst/Downloads/Timegrid/ICST_Tempopolyphony_ICMC07.pdf

*/

/* set this ramp's function constant using the end tempo and duration in beats of some later tempo section*/
void
TempoSection::set_c_func_from_tempo_and_beat (double end_bpm, double end_beat, framecnt_t frame_rate)
{
	double const log_tempo_ratio = log ((end_bpm * BBT_Time::ticks_per_beat) / ticks_per_minute());
	_c_func = ticks_per_minute() *  (exp (log_tempo_ratio) - 1) / (end_beat * BBT_Time::ticks_per_beat);
}

/* compute the function constant from some later tempo section, given tempo (beats/min.) and distance (in frames) from this tempo section */
double
TempoSection::compute_c_func (double end_bpm, framepos_t end_frame, framecnt_t frame_rate) const
{
	return c_func (end_bpm * BBT_Time::ticks_per_beat, frame_to_minute (end_frame, frame_rate));
}

framecnt_t
TempoSection::minute_to_frame (double time, framecnt_t frame_rate) const
{
	return (framecnt_t) floor ((time * 60.0 * frame_rate) + 0.5);
}

double
TempoSection::frame_to_minute (framecnt_t frame, framecnt_t frame_rate) const
{
	return (frame / (double) frame_rate) / 60.0;
}

/* position function */
double
TempoSection::a_func (double end_tpm, double c_func) const
{
	return log (end_tpm / ticks_per_minute()) /  c_func;
}

/*function constant*/
double
TempoSection::c_func (double end_tpm, double end_time) const
{
	return log (end_tpm / ticks_per_minute()) /  end_time;
}

/* tempo in tpm at time in minutes */
double
TempoSection::tick_tempo_at_time (double time) const
{
	return exp (_c_func * time) * ticks_per_minute();
}

/* time in minutes at tempo in tpm */
double
TempoSection::time_at_tick_tempo (double tick_tempo) const
{
	return log (tick_tempo / ticks_per_minute()) / _c_func;
}

/* tick at time in minutes */
double
TempoSection::tick_at_time (double time) const
{
	return ((exp (_c_func * time)) - 1) * ticks_per_minute() / _c_func;
}

/* time in minutes at tick */
double
TempoSection::time_at_tick (double tick) const
{
	return log (((_c_func * tick) / ticks_per_minute()) + 1) / _c_func;
}

/* beat at time in minutes */
double
TempoSection::beat_at_time (double time) const
{
	return tick_at_time (time) / BBT_Time::ticks_per_beat;
}

/* time in munutes at beat */
double
TempoSection::time_at_beat (double beat) const
{
	return time_at_tick (beat * BBT_Time::ticks_per_beat);
}

void
TempoSection::update_bbt_time_from_bar_offset (const Meter& meter)
{
	double new_beat;

	if (_bar_offset < 0.0) {
		/* not set yet */
		return;
	}

	new_beat = beat();

	double ticks = BBT_Time::ticks_per_beat * meter.divisions_per_bar() * _bar_offset;
	new_beat = ticks / BBT_Time::ticks_per_beat;

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("from bar offset %1 and dpb %2, ticks = %3->%4 beats = %5\n",
						       _bar_offset, meter.divisions_per_bar(), ticks, new_beat, new_beat));

	set_beat (new_beat);
}

/***********************************************************************/

const string MeterSection::xml_state_node_name = "Meter";

MeterSection::MeterSection (const XMLNode& node)
	: MetricSection (0.0), Meter (TempoMap::default_meter())
{
	XMLProperty const * prop;
	BBT_Time start;
	LocaleGuard lg;
	const XMLProperty *prop;
	BBT_Time bbt;
	double beat = 0.0;
	framepos_t frame = 0;
	pair<double, BBT_Time> start;

	if ((prop = node.property ("start")) != 0) {
		if (sscanf (prop->value().c_str(), "%" PRIu32 "|%" PRIu32 "|%" PRIu32,
		    &bbt.bars,
		    &bbt.beats,
		    &bbt.ticks) < 3) {
			error << _("MeterSection XML node has an illegal \"start\" value") << endmsg;
		} else {
			/* legacy session - start used to be in bbt*/
			beat = -1.0;
		}
	} else {
		error << _("MeterSection XML node has no \"start\" property") << endmsg;
	}

	if ((prop = node.property ("beat")) != 0) {
		if (sscanf (prop->value().c_str(), "%lf", &beat) != 1 || beat < 0.0) {
			error << _("MeterSection XML node has an illegal \"beat\" value") << endmsg;
		}
	}

	start.first = beat;

	if ((prop = node.property ("bbt")) == 0) {
		error << _("MeterSection XML node has no \"bbt\" property") << endmsg;
	} else if (sscanf (prop->value().c_str(), "%" PRIu32 "|%" PRIu32 "|%" PRIu32,
		    &bbt.bars,
		    &bbt.beats,
		    &bbt.ticks) < 3) {
		error << _("MeterSection XML node has an illegal \"bbt\" value") << endmsg;
		throw failed_constructor();
	}

	start.second = bbt;

	set_beat (start);

	if ((prop = node.property ("frame")) != 0) {
		if (sscanf (prop->value().c_str(), "%li", &frame) != 1) {
			error << _("MeterSection XML node has an illegal \"frame\" value") << endmsg;
		} else {
			set_frame (frame);
		}
	}

	/* beats-per-bar is old; divisions-per-bar is new */

	if ((prop = node.property ("divisions-per-bar")) == 0) {
		if ((prop = node.property ("beats-per-bar")) == 0) {
			error << _("MeterSection XML node has no \"beats-per-bar\" or \"divisions-per-bar\" property") << endmsg;
			throw failed_constructor();
		}
	}

	if (sscanf (prop->value().c_str(), "%lf", &_divisions_per_bar) != 1 || _divisions_per_bar < 0.0) {
		error << _("MeterSection XML node has an illegal \"beats-per-bar\" or \"divisions-per-bar\" value") << endmsg;
		throw failed_constructor();
	}

	if ((prop = node.property ("note-type")) == 0) {
		error << _("MeterSection XML node has no \"note-type\" property") << endmsg;
		throw failed_constructor();
	}

	if (sscanf (prop->value().c_str(), "%lf", &_note_type) != 1 || _note_type < 0.0) {
		error << _("MeterSection XML node has an illegal \"note-type\" value") << endmsg;
		throw failed_constructor();
	}

	if ((prop = node.property ("lock-style")) == 0) {
		warning << _("MeterSection XML node has no \"lock-style\" property") << endmsg;
		//throw failed_constructor();
		set_position_lock_style (PositionLockStyle::MusicTime);
	} else {
		set_position_lock_style (PositionLockStyle (string_2_enum (prop->value(), position_lock_style())));
	}

	if ((prop = node.property ("movable")) == 0) {
		error << _("MeterSection XML node has no \"movable\" property") << endmsg;
		throw failed_constructor();
	}

	set_movable (string_is_affirmative (prop->value()));
}

XMLNode&
MeterSection::get_state() const
{
	XMLNode *root = new XMLNode (xml_state_node_name);
	char buf[256];
	LocaleGuard lg;

	snprintf (buf, sizeof (buf), "%" PRIu32 "|%" PRIu32 "|%" PRIu32,
		  bbt().bars,
		  bbt().beats,
		  bbt().ticks);
	root->add_property ("bbt", buf);
	snprintf (buf, sizeof (buf), "%lf", beat());
	root->add_property ("beat", buf);
	snprintf (buf, sizeof (buf), "%f", _note_type);
	root->add_property ("frame", buf);
	snprintf (buf, sizeof (buf), "%li", frame());
	root->add_property ("note-type", buf);
	root->add_property ("lock-style", enum_2_string (position_lock_style()));
	snprintf (buf, sizeof (buf), "%f", _divisions_per_bar);
	root->add_property ("divisions-per-bar", buf);
	snprintf (buf, sizeof (buf), "%s", movable()?"yes":"no");
	root->add_property ("movable", buf);

	return *root;
}

/***********************************************************************/

struct MetricSectionSorter {
    bool operator() (const MetricSection* a, const MetricSection* b) {
	    return a->beat() < b->beat();
    }
};

struct MetricSectionFrameSorter {
    bool operator() (const MetricSection* a, const MetricSection* b) {
	    return a->frame() < b->frame();
    }
};

TempoMap::TempoMap (framecnt_t fr)
{
	_frame_rate = fr;
	BBT_Time start;

	start.bars = 1;
	start.beats = 1;
	start.ticks = 0;

	TempoSection *t = new TempoSection (0.0, _default_tempo.beats_per_minute(), _default_tempo.note_type(), TempoSection::Constant);
	MeterSection *m = new MeterSection (0.0, start, _default_meter.divisions_per_bar(), _default_meter.note_divisor());

	t->set_movable (false);
	m->set_movable (false);

	/* note: frame time is correct (zero) for both of these */

	metrics.push_back (t);
	metrics.push_back (m);

}

TempoMap::~TempoMap ()
{
}

void
TempoMap::remove_tempo (const TempoSection& tempo, bool complete_operation)
{
	bool removed = false;

	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		if ((removed = remove_tempo_locked (tempo))) {
			if (complete_operation) {
				recompute_map (true);
			}
		}
	}

	if (removed && complete_operation) {
		PropertyChanged (PropertyChange ());
	}
}

bool
TempoMap::remove_tempo_locked (const TempoSection& tempo)
{
	Metrics::iterator i;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		if (dynamic_cast<TempoSection*> (*i) != 0) {
			if (tempo.frame() == (*i)->frame()) {
				if ((*i)->movable()) {
					metrics.erase (i);
					return true;
				}
			}
		}
	}

	return false;
}

void
TempoMap::remove_meter (const MeterSection& tempo, bool complete_operation)
{
	bool removed = false;

	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		if ((removed = remove_meter_locked (tempo))) {
			if (complete_operation) {
				recompute_map (true);
			}
		}
	}

	if (removed && complete_operation) {
		PropertyChanged (PropertyChange ());
	}
}

bool
TempoMap::remove_meter_locked (const MeterSection& tempo)
{
	Metrics::iterator i;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		if (dynamic_cast<MeterSection*> (*i) != 0) {
			if (tempo.frame() == (*i)->frame()) {
				if ((*i)->movable()) {
					metrics.erase (i);
					return true;
				}
			}
		}
	}

	return false;
}

void
TempoMap::do_insert (MetricSection* section)
{
	bool need_add = true;

	/* we only allow new meters to be inserted on beat 1 of an existing
	 * measure.
	 */
	MeterSection* m = 0;
	if ((m = dynamic_cast<MeterSection*>(section)) != 0) {
		assert (m->bbt().ticks == 0);

		/* we need to (potentially) update the BBT times of tempo
		   sections based on this new meter.
		*/

		if ((m->bbt().beats != 1) || (m->bbt().ticks != 0)) {

			pair<double, BBT_Time> corrected = make_pair (m->beat(), m->bbt());
			corrected.second.beats = 1;
			corrected.second.ticks = 0;
			corrected.first = bbt_to_beats_locked (corrected.second);
			warning << string_compose (_("Meter changes can only be positioned on the first beat of a bar. Moving from %1 to %2"),
						   m->bbt(), corrected.second) << endmsg;
			m->set_beat (corrected);
		}
	}



	/* Look for any existing MetricSection that is of the same type and
	   in the same bar as the new one, and remove it before adding
	   the new one. Note that this means that if we find a matching,
	   existing section, we can break out of the loop since we're
	   guaranteed that there is only one such match.
	*/

	for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {

		TempoSection* const tempo = dynamic_cast<TempoSection*> (*i);
		TempoSection* const insert_tempo = dynamic_cast<TempoSection*> (section);

		if (tempo && insert_tempo) {

			/* Tempo sections */
			PositionLockStyle const tpl = tempo->position_lock_style();
			PositionLockStyle const ipl = insert_tempo->position_lock_style();
			if (tpl == ipl && ((ipl == MusicTime && tempo->beat() == insert_tempo->beat()) || (ipl == AudioTime && tempo->frame() == insert_tempo->frame()))) {

				if (!tempo->movable()) {

					/* can't (re)move this section, so overwrite
					 * its data content (but not its properties as
					 * a section).
					 */

					*(dynamic_cast<Tempo*>(*i)) = *(dynamic_cast<Tempo*>(insert_tempo));
					need_add = false;
				} else {
					metrics.erase (i);
				}
				break;
			}

		} else if (!tempo && !insert_tempo) {

			/* Meter Sections */
			MeterSection* const meter = dynamic_cast<MeterSection*> (*i);
			MeterSection* const insert_meter = dynamic_cast<MeterSection*> (section);
			PositionLockStyle const mpl = meter->position_lock_style();
			PositionLockStyle const ipl = insert_meter->position_lock_style();

			if (mpl == ipl && ((ipl == MusicTime && meter->beat() == insert_meter->beat()) || (ipl == AudioTime && meter->frame() == insert_meter->frame()))) {

				if (!meter->movable()) {

					/* can't (re)move this section, so overwrite
					 * its data content (but not its properties as
					 * a section
					 */

					*(dynamic_cast<Meter*>(*i)) = *(dynamic_cast<Meter*>(insert_meter));
					need_add = false;
				} else {
					metrics.erase (i);

				}

				break;
			}
		} else {
			/* non-matching types, so we don't care */
		}
	}

	/* Add the given MetricSection, if we didn't just reset an existing
	 * one above
	 */

	if (need_add) {
		MeterSection* const insert_meter = dynamic_cast<MeterSection*> (section);
		TempoSection* const insert_tempo = dynamic_cast<TempoSection*> (section);

		Metrics::iterator i;
		if (insert_meter) {
			for (i = metrics.begin(); i != metrics.end(); ++i) {
				MeterSection* const meter = dynamic_cast<MeterSection*> (*i);

				if (meter) {
					PositionLockStyle const ipl = insert_meter->position_lock_style();
					if (ipl == MusicTime && meter->beat() > insert_meter->beat()) {
						break;
					}
					if (ipl == AudioTime && meter->frame() > insert_meter->frame()) {
						break;
					}
				}
			}
		} else if (insert_tempo) {
			for (i = metrics.begin(); i != metrics.end(); ++i) {
				TempoSection* const tempo = dynamic_cast<TempoSection*> (*i);

				if (tempo) {
					PositionLockStyle const ipl = insert_tempo->position_lock_style();
					if ((ipl == MusicTime && tempo->beat() > insert_tempo->beat()) || (ipl == AudioTime && tempo->frame() > insert_tempo->frame())) {
						break;
					}
				}
			}
		}

		metrics.insert (i, section);
	}
}

void
TempoMap::replace_tempo (const TempoSection& ts, const Tempo& tempo, const double& where, TempoSection::Type type)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		TempoSection& first (first_tempo());
		if (ts.beat() != first.beat()) {
			remove_tempo_locked (ts);
			add_tempo_locked (tempo, where, true, type);
		} else {
			first.set_type (type);
			{
				/* cannot move the first tempo section */
				*static_cast<Tempo*>(&first) = tempo;
				recompute_map (false);
			}
		}
	}

	PropertyChanged (PropertyChange ());
}

void
TempoMap::replace_tempo (const TempoSection& ts, const Tempo& tempo, const framepos_t& frame, TempoSection::Type type)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		TempoSection& first (first_tempo());
		if (ts.beat() != first.beat()) {
			remove_tempo_locked (ts);
			add_tempo_locked (tempo, frame, true, type);
		} else {
			first.set_type (type);
			{
				/* cannot move the first tempo section */
				*static_cast<Tempo*>(&first) = tempo;
				recompute_map (false);
			}
		}
	}

	PropertyChanged (PropertyChange ());
}

void
TempoMap::gui_set_tempo_frame (TempoSection& ts, framepos_t frame, double  beat_where)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);

		if (ts.position_lock_style() == MusicTime) {
			std::cerr << "Music " << " beat where : " << beat_where << " frame : " << frame <<std::endl;
			/* MusicTime */
			ts.set_beat (beat_where);
			Metrics::const_iterator i;

			TempoSection* prev_ts = 0;
			MetricSectionSorter cmp;
			metrics.sort (cmp);
			for (i = metrics.begin(); i != metrics.end(); ++i) {
				TempoSection* t;
				if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {

					if (t->beat() >= beat_where) {
						break;
					}

					prev_ts = t;
				}
			}

			prev_ts->set_c_func_from_tempo_and_beat (ts.beats_per_minute(), ts.beat() - prev_ts->beat(), _frame_rate);
			ts.set_frame (prev_ts->frame_at_beat (ts.beat() - prev_ts->beat(), _frame_rate));
		} else {
			std::cerr << "Audio " << " beat where : " << beat_where << " frame : " << frame <<std::endl;

			/*AudioTime*/
			ts.set_frame (frame);
			MetricSectionFrameSorter fcmp;
			metrics.sort (fcmp);

			Metrics::const_iterator i;
			TempoSection* prev_ts = 0;
			TempoSection* next_ts = 0;

			for (i = metrics.begin(); i != metrics.end(); ++i) {
				TempoSection* t;
				if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {

					if (t->frame() >= frame) {
						break;
					}

					prev_ts = t;
				}
			}

			for (i = metrics.begin(); i != metrics.end(); ++i) {
				TempoSection* t;
				if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {

					if (t->frame() > frame) {
						next_ts = t;
						break;
					}
				}
			}

			if (prev_ts) {
				/* set the start beat - we need to reset the function constant before beat calculations make sense*/
				prev_ts->set_c_func (prev_ts->compute_c_func (ts.beats_per_minute(), frame - prev_ts->frame(), _frame_rate));

				double beats_to_ts = prev_ts->beat_at_frame (frame - prev_ts->frame(), _frame_rate);
				double beats = beats_to_ts + prev_ts->beat();

				if (next_ts) {
					if (next_ts->beat() < beats) {
						/* with frame-based editing, it is possible to get in a
						   situation where if the tempo was placed at the mouse pointer frame,
						   the following music-based tempo would jump to an earlier frame,
						   changing the beat beat of the moved tempo.
						   in this case, we have to do some beat-based comparison TODO
						*/
					} else if (prev_ts->beat() > beats) {
						ts.set_beat (prev_ts->beat());
					} else {
						ts.set_beat (beats);
					}
				} else {
					ts.set_beat (beats);
					ts.set_c_func (0.0);

				}
				MetricSectionSorter cmp;
				metrics.sort (cmp);
			}
		}

		recompute_map (false);
	}

	MetricPositionChanged (); // Emit Signal
}

void
TempoMap::add_tempo (const Tempo& tempo, double where, ARDOUR::TempoSection::Type type)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		add_tempo_locked (tempo, where, true, type);
	}


	PropertyChanged (PropertyChange ());
}

void
TempoMap::add_tempo (const Tempo& tempo, framepos_t frame, ARDOUR::TempoSection::Type type)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		add_tempo_locked (tempo, frame, true, type);
	}


	PropertyChanged (PropertyChange ());
}

void
TempoMap::add_tempo_locked (const Tempo& tempo, double where, bool recompute, ARDOUR::TempoSection::Type type)
{

	TempoSection* ts = new TempoSection (where, tempo.beats_per_minute(), tempo.note_type(), type);

	do_insert (ts);

	if (recompute) {
		recompute_map (false);
	}
}

void
TempoMap::add_tempo_locked (const Tempo& tempo, framepos_t frame, bool recompute, ARDOUR::TempoSection::Type type)
{
	TempoSection* ts = new TempoSection (frame, tempo.beats_per_minute(), tempo.note_type(), type);
	std::cerr << "add tempo locked frame = " << ts->frame() << " pos lock : " << ts->position_lock_style() << std::endl;

	do_insert (ts);

	if (recompute) {
		recompute_map (false);
	}
}

void
TempoMap::replace_meter (const MeterSection& ms, const Meter& meter, const BBT_Time& where)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		MeterSection& first (first_meter());
		if (ms.beat() != first.beat()) {
			remove_meter_locked (ms);
			add_meter_locked (meter, bbt_to_beats_locked (where), where, true);
		} else {
			/* cannot move the first meter section */
			*static_cast<Meter*>(&first) = meter;
			recompute_map (true);
		}
	}

	PropertyChanged (PropertyChange ());
}

void
TempoMap::replace_meter (const MeterSection& ms, const Meter& meter, const framepos_t& frame)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		MeterSection& first (first_meter());
		if (ms.beat() != first.beat()) {
			remove_meter_locked (ms);
			add_meter_locked (meter, frame, true);
		} else {
			/* cannot move the first meter section */
			*static_cast<Meter*>(&first) = meter;
			recompute_map (true);
		}
	}

	PropertyChanged (PropertyChange ());
}


void
TempoMap::add_meter (const Meter& meter, double beat, BBT_Time where)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		add_meter_locked (meter, beat, where, true);
	}


#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::TempoMap)) {
		dump (std::cerr);
	}
#endif

	PropertyChanged (PropertyChange ());
}

void
TempoMap::add_meter (const Meter& meter, framepos_t frame)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		add_meter_locked (meter, frame, true);
	}


#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::TempoMap)) {
		dump (std::cerr);
	}
#endif

	PropertyChanged (PropertyChange ());
}

void
TempoMap::add_meter_locked (const Meter& meter, double beat, BBT_Time where, bool recompute)
{
	/* a new meter always starts a new bar on the first beat. so
	   round the start time appropriately. remember that
	   `where' is based on the existing tempo map, not
	   the result after we insert the new meter.

	*/

	if (where.beats != 1) {
		where.beats = 1;
		where.bars++;
	}

	/* new meters *always* start on a beat. */
	where.ticks = 0;

	do_insert (new MeterSection (beat, where, meter.divisions_per_bar(), meter.note_divisor()));

	if (recompute) {
		recompute_map (true);
	}

}

void
TempoMap::add_meter_locked (const Meter& meter, framepos_t frame, bool recompute)
{

	/* MusicTime meters *always* start on 1|1|0. */
	MeterSection* ms = new MeterSection (frame, meter.divisions_per_bar(), meter.note_divisor());
	BBT_Time bbt;
	pair<double, BBT_Time> pr;
	bbt.bars = 1;
	bbt.beats = 1;
	bbt.ticks = 0;
	/* just a dummy - the actual beat should be applied in recompute_map() as thins is AudioTime */
	pr.first = 0.0;
	pr.second = bbt;
	ms->set_beat (pr);
	do_insert (ms);

	if (recompute) {
		recompute_map (true);
	}

}

void
TempoMap::change_initial_tempo (double beats_per_minute, double note_type)
{
	Tempo newtempo (beats_per_minute, note_type);
	TempoSection* t;

	for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
			{
				Glib::Threads::RWLock::WriterLock lm (lock);
				*((Tempo*) t) = newtempo;
				recompute_map (false);
			}
			PropertyChanged (PropertyChange ());
			break;
		}
	}
}

void
TempoMap::change_existing_tempo_at (framepos_t where, double beats_per_minute, double note_type)
{
	Tempo newtempo (beats_per_minute, note_type);

	TempoSection* prev;
	TempoSection* first;
	Metrics::iterator i;

	/* find the TempoSection immediately preceding "where"
	 */

	for (first = 0, i = metrics.begin(), prev = 0; i != metrics.end(); ++i) {

		if ((*i)->frame() > where) {
			break;
		}

		TempoSection* t;

		if ((t = dynamic_cast<TempoSection*>(*i)) != 0) {
			if (!first) {
				first = t;
			}
			prev = t;
		}
	}

	if (!prev) {
		if (!first) {
			error << string_compose (_("no tempo sections defined in tempo map - cannot change tempo @ %1"), where) << endmsg;
			return;
		}

		prev = first;
	}

	/* reset */

	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		/* cannot move the first tempo section */
		*((Tempo*)prev) = newtempo;
		recompute_map (false);
	}

	PropertyChanged (PropertyChange ());
}

const MeterSection&
TempoMap::first_meter () const
{
	const MeterSection *m = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if ((m = dynamic_cast<const MeterSection *> (*i)) != 0) {
			return *m;
		}
	}

	fatal << _("programming error: no tempo section in tempo map!") << endmsg;
	abort(); /*NOTREACHED*/
	return *m;
}

MeterSection&
TempoMap::first_meter ()
{
	MeterSection *m = 0;

	/* CALLER MUST HOLD LOCK */

	for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if ((m = dynamic_cast<MeterSection *> (*i)) != 0) {
			return *m;
		}
	}

	fatal << _("programming error: no tempo section in tempo map!") << endmsg;
	abort(); /*NOTREACHED*/
	return *m;
}

const TempoSection&
TempoMap::first_tempo () const
{
	const TempoSection *t = 0;

	/* CALLER MUST HOLD LOCK */

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if ((t = dynamic_cast<const TempoSection *> (*i)) != 0) {
			return *t;
		}
	}

	fatal << _("programming error: no tempo section in tempo map!") << endmsg;
	abort(); /*NOTREACHED*/
	return *t;
}

TempoSection&
TempoMap::first_tempo ()
{
	TempoSection *t = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if ((t = dynamic_cast<TempoSection *> (*i)) != 0) {
			return *t;
		}
	}

	fatal << _("programming error: no tempo section in tempo map!") << endmsg;
	abort(); /*NOTREACHED*/
	return *t;
}

void
TempoMap::recompute_map (bool reassign_tempo_bbt, framepos_t end)
{
	/* CALLER MUST HOLD WRITE LOCK */

	if (end < 0) {

		/* we will actually stop once we hit
		   the last metric.
		*/
		end = max_framepos;

	}

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("recomputing tempo map, zero to %1\n", end));

	if (end == 0) {
		/* silly call from Session::process() during startup
		 */
		return;
	}

	Metrics::const_iterator i;
	TempoSection* prev_ts = 0;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;

		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
			if (prev_ts) {
				if (t->position_lock_style() == AudioTime) {
					if (prev_ts->type() == TempoSection::Ramp) {
						prev_ts->set_c_func (prev_ts->compute_c_func (t->beats_per_minute(), t->frame() - prev_ts->frame(), _frame_rate));
						t->set_beat (prev_ts->beat_at_frame (t->frame() - prev_ts->frame(), _frame_rate) + prev_ts->beat());
					} else {
						prev_ts->set_c_func (0.0);
						t->set_beat (prev_ts->beat_at_frame (t->frame() - prev_ts->frame(), _frame_rate) + prev_ts->beat());
					}
				} else if (t->position_lock_style() == MusicTime) {
					if (prev_ts->type() == TempoSection::Ramp) {
						prev_ts->set_c_func_from_tempo_and_beat (t->beats_per_minute(), t->beat() - prev_ts->beat(), _frame_rate);
						t->set_frame (prev_ts->frame_at_beat (t->beat() - prev_ts->beat(), _frame_rate) + prev_ts->frame());
					} else {
						double const ticks_relative_to_prev = (t->beat() - prev_ts->beat()) * BBT_Time::ticks_per_beat;
						framecnt_t const duration = (framecnt_t) floor (ticks_relative_to_prev * prev_ts->frames_per_beat (_frame_rate)
												* BBT_Time::ticks_per_beat);
						prev_ts->set_c_func (0.0);
						t->set_frame (duration + prev_ts->frame());
					}
				}
			}
			prev_ts = t;
		}
	}

	MeterSection* meter = 0;

	for (Metrics::const_iterator mi = metrics.begin(); mi != metrics.end(); ++mi) {
		/* We now have the tempo map set. use it to set meter positions.*/
		if ((meter = dynamic_cast<MeterSection*> (*mi)) != 0) {
			if (meter->position_lock_style() == AudioTime) {
				/* a frame based meter has to have a 1|1|0 bbt */
				pair<double, BBT_Time> pr;
				BBT_Time where;

				where.bars = 1;
				where.beats = 1;
				where.ticks = 0;

				pr.first = tick_at_frame (meter->frame()) / BBT_Time::ticks_per_beat;
				pr.second = where;
				meter->set_beat (pr);
			} else if (meter->position_lock_style() == MusicTime) {
				meter->set_frame (frame_at_tick (meter->beat() * BBT_Time::ticks_per_beat));
			}
		}
	}
}


TempoMetric
TempoMap::metric_at (framepos_t frame, Metrics::const_iterator* last) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	TempoMetric m (first_meter(), first_tempo());

	/* at this point, we are *guaranteed* to have m.meter and m.tempo pointing
	   at something, because we insert the default tempo and meter during
	   TempoMap construction.

	   now see if we can find better candidates.
	*/

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {

		if ((*i)->frame() > frame) {
			break;
		}

		m.set_metric(*i);

		if (last) {
			*last = i;
		}
	}

	return m;
}
/* XX meters only */
TempoMetric
TempoMap::metric_at (BBT_Time bbt) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	TempoMetric m (first_meter(), first_tempo());

	/* at this point, we are *guaranteed* to have m.meter and m.tempo pointing
	   at something, because we insert the default tempo and meter during
	   TempoMap construction.

	   now see if we can find better candidates.
	*/

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		MeterSection* mw;
		if ((mw = dynamic_cast<MeterSection*> (*i)) != 0) {
			BBT_Time section_start (mw->bbt());

			if (section_start.bars > bbt.bars || (section_start.bars == bbt.bars && section_start.beats > bbt.beats)) {
				break;
			}

			m.set_metric (*i);
		}
	}

	return m;
}

void
TempoMap::bbt_time (framepos_t frame, BBT_Time& bbt)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	if (frame < 0) {
		bbt.bars = 1;
		bbt.beats = 1;
		bbt.ticks = 0;
		warning << string_compose (_("tempo map asked for BBT time at frame %1\n"), frame) << endmsg;
		return;
	}
	bbt = beats_to_bbt_locked (beat_at_frame (frame));
}

double
TempoMap::bbt_to_beats (Timecode::BBT_Time bbt)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	return bbt_to_beats_locked (bbt);
}

double
TempoMap::bbt_to_beats_locked (Timecode::BBT_Time bbt)
{
	/* CALLER HOLDS READ LOCK */

	double accumulated_beats = 0.0;
	double accumulated_bars = 0.0;
	MeterSection* prev_ms = 0;

	Metrics::const_iterator i;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		MeterSection* m;
		if ((m = dynamic_cast<MeterSection*> (*i)) != 0) {
			double bars_to_m = 0.0;
			if (prev_ms) {
				bars_to_m = (m->beat() - prev_ms->beat()) / prev_ms->divisions_per_bar();
			}
			if ((bars_to_m + accumulated_bars) > (bbt.bars - 1)) {
				break;
			}
			if (prev_ms) {
				accumulated_beats += m->beat() - prev_ms->beat();
				accumulated_bars += bars_to_m;
			}
			prev_ms = m;
		}
	}

	double const remaining_bars = (bbt.bars - 1) - accumulated_bars;
	double const remaining_bars_in_beats = remaining_bars * prev_ms->divisions_per_bar();
	double const ret = remaining_bars_in_beats + accumulated_beats + (bbt.beats - 1) + (bbt.ticks / BBT_Time::ticks_per_beat);
	return ret;
}

Timecode::BBT_Time
TempoMap::beats_to_bbt (double beats)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	return beats_to_bbt_locked (beats);
}

Timecode::BBT_Time
TempoMap::beats_to_bbt_locked (double beats)
{
	/* CALLER HOLDS READ LOCK */

	MeterSection* prev_ms = 0;
	uint32_t accumulated_bars = 0;

	Metrics::const_iterator i;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		MeterSection* m = 0;

		if ((m = dynamic_cast<MeterSection*> (*i)) != 0) {

			if (beats < m->beat()) {
				/* this is the meter after the one our beat is on*/
				break;
			}

			if (prev_ms) {
				/* we need a whole number of bars. */
				accumulated_bars += ((m->beat() - prev_ms->beat()) + 1) / prev_ms->divisions_per_bar();
			}

			prev_ms = m;
		}
	}

	double const beats_in_ms = beats - prev_ms->beat();
	uint32_t const bars_in_ms = (uint32_t) floor (beats_in_ms / prev_ms->divisions_per_bar());
	uint32_t const total_bars = bars_in_ms + accumulated_bars;
	double const remaining_beats = beats_in_ms - (bars_in_ms * prev_ms->divisions_per_bar());
	double const remaining_ticks = (remaining_beats - floor (remaining_beats)) * BBT_Time::ticks_per_beat;

	BBT_Time ret;

	ret.ticks = (uint32_t) floor (remaining_ticks + 0.5);
	ret.beats = (uint32_t) floor (remaining_beats);
	ret.bars = total_bars;

	/* 0 0 0 to 1 1 0 - based mapping*/
	++ret.bars;
	++ret.beats;

	if (ret.ticks >= BBT_Time::ticks_per_beat) {
		++ret.beats;
		ret.ticks -= BBT_Time::ticks_per_beat;
	}

	if (ret.beats > prev_ms->divisions_per_bar()) {
		++ret.bars;
		ret.beats = 1;
	}

	return ret;
}

double
TempoMap::tick_at_frame (framecnt_t frame) const
{
	/* HOLD (at least) THE READER LOCK */

	Metrics::const_iterator i;
	TempoSection* prev_ts = 0;
	double accumulated_ticks = 0.0;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;

		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {

			if ((prev_ts) && frame < t->frame()) {
				/*the previous ts is the one containing the frame */

				framepos_t const time = frame - prev_ts->frame();

				return prev_ts->tick_at_frame (time, _frame_rate) + accumulated_ticks;
			}

			if (prev_ts && t->frame() > prev_ts->frame()) {
				accumulated_ticks = t->beat() * BBT_Time::ticks_per_beat;
			}

			prev_ts = t;
		}
	}

	/* treated as constant for this ts */
	framecnt_t const frames_in_section = frame - prev_ts->frame();
	double const ticks_in_section = (frames_in_section / prev_ts->frames_per_beat (_frame_rate)) * Timecode::BBT_Time::ticks_per_beat;

	return ticks_in_section + accumulated_ticks;

}

framecnt_t
TempoMap::frame_at_tick (double tick) const
{
	/* HOLD THE READER LOCK */

	double accumulated_ticks = 0.0;
	double accumulated_ticks_to_prev = 0.0;
	const TempoSection* prev_ts = 0;

	Metrics::const_iterator i;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;
		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {

			if (prev_ts && t->frame() > prev_ts->frame()) {
				accumulated_ticks = t->beat() * BBT_Time::ticks_per_beat;
			}

			if (prev_ts && tick < accumulated_ticks) {
				/* prev_ts is the one affecting us. */

				double const ticks_in_section = tick - accumulated_ticks_to_prev;

				return prev_ts->frame_at_tick (ticks_in_section, _frame_rate) + prev_ts->frame();
			}
			accumulated_ticks_to_prev = accumulated_ticks;
			prev_ts = t;
		}
	}
	/* must be treated as constant, irrespective of _type */
	double const ticks_in_section = tick - accumulated_ticks_to_prev;
	double const dtime = (ticks_in_section / BBT_Time::ticks_per_beat) * prev_ts->frames_per_beat (_frame_rate);

	framecnt_t const ret = ((framecnt_t) floor (dtime)) + prev_ts->frame();

	return ret;
}

double
TempoMap::beat_at_frame (framecnt_t frame) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return tick_at_frame (frame) / BBT_Time::ticks_per_beat;
}

framecnt_t
TempoMap::frame_at_beat (double beat) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return frame_at_tick (beat * BBT_Time::ticks_per_beat);
}

framepos_t
TempoMap::frame_time (const BBT_Time& bbt)
{
	if (bbt.bars < 1) {
		warning << string_compose (_("tempo map asked for frame time at bar < 1  (%1)\n"), bbt) << endmsg;
		return 0;
	}

	if (bbt.beats < 1) {
		throw std::logic_error ("beats are counted from one");
	}
	Glib::Threads::RWLock::ReaderLock lm (lock);

	framepos_t const ret = frame_at_beat (bbt_to_beats_locked (bbt));

	return ret;
}


framecnt_t
TempoMap::bbt_duration_at (framepos_t pos, const BBT_Time& bbt, int dir)
{

	Glib::Threads::RWLock::ReaderLock lm (lock);

	Metrics::const_iterator i;
	TempoSection* first = 0;
	TempoSection* second = 0;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;

		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {

			if ((*i)->frame() > pos) {
				second = t;
				break;
			}

			first = t;
		}
	}
	if (first && second) {
		framepos_t const time = pos - first->frame();
		double const tick_at_time = first->tick_at_frame (time, _frame_rate);
		double const bbt_ticks = bbt.ticks + (bbt.beats * BBT_Time::ticks_per_beat);
		double const time_at_bbt = first->frame_at_tick (tick_at_time + bbt_ticks, _frame_rate);

		return time_at_bbt - time;
	}

	double const ticks = bbt.ticks + (bbt.beats * BBT_Time::ticks_per_beat);
	return (framecnt_t) floor ((ticks / BBT_Time::ticks_per_beat) * first->frames_per_beat(_frame_rate));
}

framepos_t
TempoMap::round_to_bar (framepos_t fr, RoundMode dir)
{
	return round_to_type (fr, dir, Bar);
}

framepos_t
TempoMap::round_to_beat (framepos_t fr, RoundMode dir)
{
	return round_to_type (fr, dir, Beat);
}

framepos_t
TempoMap::round_to_beat_subdivision (framepos_t fr, int sub_num, RoundMode dir)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	uint32_t ticks = (uint32_t) floor (tick_at_frame (fr) + 0.5);
	uint32_t beats = (uint32_t) floor (ticks / BBT_Time::ticks_per_beat);
	uint32_t ticks_one_subdivisions_worth = (uint32_t)BBT_Time::ticks_per_beat / sub_num;

	ticks -= beats * BBT_Time::ticks_per_beat;

	if (dir > 0) {
		/* round to next (or same iff dir == RoundUpMaybe) */

		uint32_t mod = ticks % ticks_one_subdivisions_worth;

		if (mod == 0 && dir == RoundUpMaybe) {
			/* right on the subdivision, which is fine, so do nothing */

		} else if (mod == 0) {
			/* right on the subdivision, so the difference is just the subdivision ticks */
			ticks += ticks_one_subdivisions_worth;

		} else {
			/* not on subdivision, compute distance to next subdivision */

			ticks += ticks_one_subdivisions_worth - mod;
		}

		if (ticks >= BBT_Time::ticks_per_beat) {
			ticks -= BBT_Time::ticks_per_beat;
		}
	} else if (dir < 0) {

		/* round to previous (or same iff dir == RoundDownMaybe) */

		uint32_t difference = ticks % ticks_one_subdivisions_worth;

		if (difference == 0 && dir == RoundDownAlways) {
			/* right on the subdivision, but force-rounding down,
			   so the difference is just the subdivision ticks */
			difference = ticks_one_subdivisions_worth;
		}

		if (ticks < difference) {
			ticks = BBT_Time::ticks_per_beat - ticks;
		} else {
			ticks -= difference;
		}

	} else {
		/* round to nearest */
		double rem;

		/* compute the distance to the previous and next subdivision */

		if ((rem = fmod ((double) ticks, (double) ticks_one_subdivisions_worth)) > ticks_one_subdivisions_worth/2.0) {

			/* closer to the next subdivision, so shift forward */

			ticks = lrint (ticks + (ticks_one_subdivisions_worth - rem));

			DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved forward to %1\n", ticks));

			if (ticks > BBT_Time::ticks_per_beat) {
				++beats;
				ticks -= BBT_Time::ticks_per_beat;
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("fold beat to %1\n", beats));
			}

		} else if (rem > 0) {

			/* closer to previous subdivision, so shift backward */

			if (rem > ticks) {
				if (beats == 0) {
					/* can't go backwards past zero, so ... */
					return 0;
				}
				/* step back to previous beat */
				--beats;
				ticks = lrint (BBT_Time::ticks_per_beat - rem);
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("step back beat to %1\n", beats));
			} else {
				ticks = lrint (ticks - rem);
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved backward to %1\n", ticks));
			}
		} else {
			/* on the subdivision, do nothing */
		}
	}
	return frame_at_tick ((beats * BBT_Time::ticks_per_beat) + ticks);
}

framepos_t
TempoMap::round_to_type (framepos_t frame, RoundMode dir, BBTPointType type)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	double const beat_at_framepos = beat_at_frame (frame);

	BBT_Time bbt (beats_to_bbt_locked (beat_at_framepos));

	switch (type) {
	case Bar:
		if (dir < 0) {
			/* find bar previous to 'frame' */
			bbt.beats = 1;
			bbt.ticks = 0;
			return frame_time (bbt);

		} else if (dir > 0) {
			/* find bar following 'frame' */
			++bbt.bars;
			bbt.beats = 1;
			bbt.ticks = 0;
			return frame_time (bbt);
		} else {
			/* true rounding: find nearest bar */
			framepos_t raw_ft = frame_time (bbt);
			bbt.beats = 1;
			bbt.ticks = 0;
			framepos_t prev_ft = frame_time (bbt);
			++bbt.bars;
			framepos_t next_ft = frame_time (bbt);

			if ((raw_ft - prev_ft) > (next_ft - prev_ft) / 2) { 
				return next_ft;
			} else {
				return prev_ft;
			}
		}

		break;

	case Beat:
		if (dir < 0) {
			return frame_at_beat (floor (beat_at_framepos));
		} else if (dir > 0) {
			return frame_at_beat (ceil (beat_at_framepos));
		} else {
			return frame_at_beat (floor (beat_at_framepos + 0.5));
		}
		break;
	}

	return 0;
}

void
TempoMap::get_grid (vector<TempoMap::BBTPoint>& points,
		    framepos_t lower, framepos_t upper)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	uint32_t const upper_beat = (uint32_t) floor (beat_at_frame (upper));
	uint32_t cnt = (uint32_t) ceil (beat_at_frame (lower));

	while (cnt <= upper_beat) {
		framecnt_t const pos = frame_at_beat (cnt);
		MeterSection const meter = meter_section_at (pos);
		Tempo const tempo = tempo_at (pos);
		BBT_Time const bbt = beats_to_bbt_locked ((double) cnt);

		points.push_back (BBTPoint (meter, tempo, pos, bbt.bars, bbt.beats));
		++cnt;
	}
}

const TempoSection&
TempoMap::tempo_section_at (framepos_t frame) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	Metrics::const_iterator i;
	TempoSection* prev = 0;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;

		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {

			if ((*i)->frame() > frame) {
				break;
			}

			prev = t;
		}
	}

	if (prev == 0) {
		fatal << endmsg;
		abort(); /*NOTREACHED*/
	}

	return *prev;
}

/* don't use this to calculate length (the tempo is only correct for this frame).
   do that stuff based on the beat_at_frame and frame_at_beat api
*/
double
TempoMap::frames_per_beat_at (framepos_t frame, framecnt_t sr) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	const TempoSection* ts_at = &tempo_section_at (frame);
	const TempoSection* ts_after = 0;
	Metrics::const_iterator i;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;

		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {

			if ((*i)->frame() > frame) {
				ts_after = t;
				break;
			}
		}
	}

	if (ts_after) {
		return  (60.0 * _frame_rate) / (ts_at->tempo_at_frame (frame - ts_at->frame(), _frame_rate));
	}
	/* must be treated as constant tempo */
	return ts_at->frames_per_beat (_frame_rate);
}

const Tempo
TempoMap::tempo_at (framepos_t frame) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	TempoMetric m (metric_at (frame));
	TempoSection* prev_ts = 0;

	Metrics::const_iterator i;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;
		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
			if ((prev_ts) && t->frame() > frame) {
				/* this is the one past frame */
				framepos_t const time = frame - prev_ts->frame();
				double const ret = prev_ts->tempo_at_frame (time, _frame_rate);
				Tempo const ret_tempo (ret, m.tempo().note_type ());
				return ret_tempo;
			}
			prev_ts = t;
		}
	}

	return m.tempo();

}

const MeterSection&
TempoMap::meter_section_at (framepos_t frame) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	Metrics::const_iterator i;
	MeterSection* prev = 0;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		MeterSection* t;

		if ((t = dynamic_cast<MeterSection*> (*i)) != 0) {

			if ((*i)->frame() > frame) {
				break;
			}

			prev = t;
		}
	}

	if (prev == 0) {
		fatal << endmsg;
		abort(); /*NOTREACHED*/
	}

	return *prev;
}

const Meter&
TempoMap::meter_at (framepos_t frame) const
{
	TempoMetric m (metric_at (frame));
	return m.meter();
}

XMLNode&
TempoMap::get_state ()
{
	Metrics::const_iterator i;
	XMLNode *root = new XMLNode ("TempoMap");

	{
		Glib::Threads::RWLock::ReaderLock lm (lock);
		for (i = metrics.begin(); i != metrics.end(); ++i) {
			root->add_child_nocopy ((*i)->get_state());
		}
	}

	return *root;
}

int
TempoMap::set_state (const XMLNode& node, int /*version*/)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);

		XMLNodeList nlist;
		XMLNodeConstIterator niter;
		Metrics old_metrics (metrics);
		MeterSection* last_meter = 0;
		metrics.clear();

		nlist = node.children();

		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			XMLNode* child = *niter;

			if (child->name() == TempoSection::xml_state_node_name) {

				try {
					TempoSection* ts = new TempoSection (*child);
					metrics.push_back (ts);

					if (ts->bar_offset() < 0.0) {
						if (last_meter) {
							//ts->update_bar_offset_from_bbt (*last_meter);
						}
					}
				}

				catch (failed_constructor& err){
					error << _("Tempo map: could not set new state, restoring old one.") << endmsg;
					metrics = old_metrics;
					break;
				}

			} else if (child->name() == MeterSection::xml_state_node_name) {

				try {
					MeterSection* ms = new MeterSection (*child);
					metrics.push_back (ms);
					last_meter = ms;
				}

				catch (failed_constructor& err) {
					error << _("Tempo map: could not set new state, restoring old one.") << endmsg;
					metrics = old_metrics;
					break;
				}
			}
		}

		if (niter == nlist.end()) {
			MetricSectionSorter cmp;
			metrics.sort (cmp);
		}
		/* check for legacy sessions where bbt was the base musical unit for tempo */
		for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {
			MeterSection* prev_ms;
			TempoSection* prev_ts;
			if ((prev_ms = dynamic_cast<MeterSection*>(*i)) != 0) {
				if (prev_ms->beat() < 0.0) {
					/*XX we cannot possibly make this work??. */
					pair<double, BBT_Time> start = make_pair (((prev_ms->bbt().bars - 1) * 4.0) + (prev_ms->bbt().beats - 1) + (prev_ms->bbt().ticks / BBT_Time::ticks_per_beat), prev_ms->bbt());
					prev_ms->set_beat (start);
				}
			} else if ((prev_ts = dynamic_cast<TempoSection*>(*i)) != 0) {
				if (prev_ts->beat() < 0.0) {
					double const start = ((prev_ts->legacy_bbt().bars - 1) * 4.0) + (prev_ts->legacy_bbt().beats - 1) + (prev_ts->legacy_bbt().ticks / BBT_Time::ticks_per_beat);
					prev_ts->set_beat (start);

				}
			}
		}
		/* check for multiple tempo/meters at the same location, which
		   ardour2 somehow allowed.
		*/

		Metrics::iterator prev = metrics.end();
		for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {
			if (prev != metrics.end()) {
				MeterSection* ms;
				MeterSection* prev_ms;
				TempoSection* ts;
				TempoSection* prev_ts;
				if ((prev_ms = dynamic_cast<MeterSection*>(*prev)) != 0 && (ms = dynamic_cast<MeterSection*>(*i)) != 0) {
					if (prev_ms->beat() == ms->beat()) {
						cerr << string_compose (_("Multiple meter definitions found at %1"), prev_ms->beat()) << endmsg;
						error << string_compose (_("Multiple meter definitions found at %1"), prev_ms->beat()) << endmsg;
						return -1;
					}
				} else if ((prev_ts = dynamic_cast<TempoSection*>(*prev)) != 0 && (ts = dynamic_cast<TempoSection*>(*i)) != 0) {
					if (prev_ts->beat() == ts->beat()) {
						cerr << string_compose (_("Multiple tempo definitions found at %1"), prev_ts->beat()) << endmsg;
						error << string_compose (_("Multiple tempo definitions found at %1"), prev_ts->beat()) << endmsg;
						return -1;
					}
				}
			}
			prev = i;
		}

		recompute_map (true, -1);
	}

	PropertyChanged (PropertyChange ());

	return 0;
}

void
TempoMap::dump (std::ostream& o) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock, Glib::Threads::TRY_LOCK);
	const MeterSection* m;
	const TempoSection* t;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {

		if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
			o << "Tempo @ " << *i << " (Bar-offset: " << t->bar_offset() << ") " << t->beats_per_minute() << " BPM (pulse = 1/" << t->note_type() << ") at " << t->beat() << " frame= " << t->frame() << " (movable? "
			  << t->movable() << ')' << endl;
		} else if ((m = dynamic_cast<const MeterSection*>(*i)) != 0) {
			o << "Meter @ " << *i << ' ' << m->divisions_per_bar() << '/' << m->note_divisor() << " at " << m->bbt() << " frame= " << m->frame()
			  << " (movable? " << m->movable() << ')' << endl;
		}
	}
}

int
TempoMap::n_tempos() const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	int cnt = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if (dynamic_cast<const TempoSection*>(*i) != 0) {
			cnt++;
		}
	}

	return cnt;
}

int
TempoMap::n_meters() const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	int cnt = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if (dynamic_cast<const MeterSection*>(*i) != 0) {
			cnt++;
		}
	}

	return cnt;
}

void
TempoMap::insert_time (framepos_t where, framecnt_t amount)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {
			if ((*i)->frame() >= where && (*i)->movable ()) {
				(*i)->set_frame ((*i)->frame() + amount);
			}
		}

		/* now reset the BBT time of all metrics, based on their new
		 * audio time. This is the only place where we do this reverse
		 * timestamp.
		 */

		Metrics::iterator i;
		const MeterSection* meter;
		const TempoSection* tempo;
		MeterSection *m;
		TempoSection *t;

		meter = &first_meter ();
		tempo = &first_tempo ();

		BBT_Time start;
		BBT_Time end;

		// cerr << "\n###################### TIMESTAMP via AUDIO ##############\n" << endl;

		bool first = true;
		MetricSection* prev = 0;

		for (i = metrics.begin(); i != metrics.end(); ++i) {

			BBT_Time bbt;
			//TempoMetric metric (*meter, *tempo);
			MeterSection* ms = const_cast<MeterSection*>(meter);
			TempoSection* ts = const_cast<TempoSection*>(tempo);
			if (prev) {
				if (ts){
					if ((t = dynamic_cast<TempoSection*>(prev)) != 0) {
						ts->set_beat (t->beat());
					}
					if ((m = dynamic_cast<MeterSection*>(prev)) != 0) {
						ts->set_beat (m->beat());
					}
					ts->set_frame (prev->frame());

				}
				if (ms) {
					if ((m = dynamic_cast<MeterSection*>(prev)) != 0) {
						pair<double, BBT_Time> start = make_pair (m->beat(), m->bbt());
						ms->set_beat (start);
					}
					if ((t = dynamic_cast<TempoSection*>(prev)) != 0) {
						pair<double, BBT_Time> start = make_pair (t->beat(), beats_to_bbt_locked (t->beat()));
						ms->set_beat (start);
					}
					ms->set_frame (prev->frame());
				}

			} else {
				// metric will be at frames=0 bbt=1|1|0 by default
				// which is correct for our purpose
			}

			// cerr << bbt << endl;

			if ((t = dynamic_cast<TempoSection*>(*i)) != 0) {
				t->set_beat (beat_at_frame (m->frame()));
				tempo = t;
				// cerr << "NEW TEMPO, frame = " << (*i)->frame() << " beat = " << (*i)->beat() <<endl;
			} else if ((m = dynamic_cast<MeterSection*>(*i)) != 0) {
				bbt_time (m->frame(), bbt);

				// cerr << "timestamp @ " << (*i)->frame() << " with " << bbt.bars << "|" << bbt.beats << "|" << bbt.ticks << " => ";

				if (first) {
					first = false;
				} else {

					if (bbt.ticks > BBT_Time::ticks_per_beat/2) {
						/* round up to next beat */
						bbt.beats += 1;
					}

					bbt.ticks = 0;

					if (bbt.beats != 1) {
						/* round up to next bar */
						bbt.bars += 1;
						bbt.beats = 1;
					}
				}
				pair<double, BBT_Time> start = make_pair (beat_at_frame (m->frame()), bbt);
				m->set_beat (start);
				meter = m;
				// cerr << "NEW METER, frame = " << (*i)->frame() << " beat = " << (*i)->beat() <<endl;
			} else {
				fatal << _("programming error: unhandled MetricSection type") << endmsg;
				abort(); /*NOTREACHED*/
			}

			prev = (*i);
		}

		recompute_map (true);
	}


	PropertyChanged (PropertyChange ());
}
bool
TempoMap::remove_time (framepos_t where, framecnt_t amount)
{
	bool moved = false;

	std::list<MetricSection*> metric_kill_list;

	TempoSection* last_tempo = NULL;
	MeterSection* last_meter = NULL;
	bool tempo_after = false; // is there a tempo marker at the first sample after the removed range?
	bool meter_after = false; // is there a meter marker likewise?
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {
			if ((*i)->frame() >= where && (*i)->frame() < where+amount) {
				metric_kill_list.push_back(*i);
				TempoSection *lt = dynamic_cast<TempoSection*> (*i);
				if (lt)
					last_tempo = lt;
				MeterSection *lm = dynamic_cast<MeterSection*> (*i);
				if (lm)
					last_meter = lm;
			}
			else if ((*i)->frame() >= where) {
				// TODO: make sure that moved tempo/meter markers are rounded to beat/bar boundaries
				(*i)->set_frame ((*i)->frame() - amount);
				if ((*i)->frame() == where) {
					// marker was immediately after end of range
					tempo_after = dynamic_cast<TempoSection*> (*i);
					meter_after = dynamic_cast<MeterSection*> (*i);
				}
				moved = true;
			}
		}

		//find the last TEMPO and METER metric (if any) and move it to the cut point so future stuff is correct
		if (last_tempo && !tempo_after) {
			metric_kill_list.remove(last_tempo);
			last_tempo->set_frame(where);
			moved = true;
		}
		if (last_meter && !meter_after) {
			metric_kill_list.remove(last_meter);
			last_meter->set_frame(where);
			moved = true;
		}

		//remove all the remaining metrics
		for (std::list<MetricSection*>::iterator i = metric_kill_list.begin(); i != metric_kill_list.end(); ++i) {
			metrics.remove(*i);
			moved = true;
		}

		if (moved) {
			recompute_map (true);
		}
	}
	PropertyChanged (PropertyChange ());
	return moved;
}

/** Add some (fractional) beats to a session frame position, and return the result in frames.
 *  pos can be -ve, if required.
 */
framepos_t
TempoMap::framepos_plus_beats (framepos_t pos, Evoral::Beats beats) const
{
	return frame_at_beat (beat_at_frame (pos) + beats.to_double());
}

/** Subtract some (fractional) beats from a frame position, and return the result in frames */
framepos_t
TempoMap::framepos_minus_beats (framepos_t pos, Evoral::Beats beats) const
{
	return frame_at_beat (beat_at_frame (pos) - beats.to_double());
}

/** Add the BBT interval op to pos and return the result */
framepos_t
TempoMap::framepos_plus_bbt (framepos_t pos, BBT_Time op) const
{
	cerr << "framepos_plus_bbt - untested" << endl;
	Glib::Threads::RWLock::ReaderLock lm (lock);

	Metrics::const_iterator i;
	const MeterSection* meter;
	const MeterSection* m;
	const TempoSection* tempo;
	const TempoSection* next_tempo = 0;
	const TempoSection* t;
	double frames_per_beat;
	framepos_t effective_pos = max (pos, (framepos_t) 0);

	meter = &first_meter ();
	tempo = &first_tempo ();

	assert (meter);
	assert (tempo);

	/* find the starting metrics for tempo & meter */

	for (i = metrics.begin(); i != metrics.end(); ++i) {

		if ((*i)->frame() > effective_pos) {
			break;
		}

		if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
			tempo = t;
		} else if ((m = dynamic_cast<const MeterSection*>(*i)) != 0) {
			meter = m;
		}
	}

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		if ((*i)->frame() > effective_pos) {
			if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
				next_tempo = t;
			}
			break;
		}
	}

	/* We now have:

	   meter -> the Meter for "pos"
	   tempo -> the Tempo for "pos"
	   next_tempo -> the Tempo after "pos" or 0
	   i     -> for first new metric after "pos", possibly metrics.end()
	*/

	/* now comes the complicated part. we have to add one beat a time,
	   checking for a new metric on every beat.
	*/

	uint64_t bars = 0;
	/* fpb is used for constant tempo */
	frames_per_beat = tempo->frames_per_beat (_frame_rate);

	while (op.bars) {

		bars++;
		op.bars--;

		/* check if we need to use a new metric section: has adding frames moved us
		   to or after the start of the next metric section? in which case, use it.
		*/

		if (i != metrics.end()) {
			if ((*i)->frame() <= pos) {

				/* about to change tempo or meter, so add the
				 * number of frames for the bars we've just
				 * traversed before we change the
				 * frames_per_beat value.
				 */

				if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
					next_tempo = t;
				}

				if (next_tempo) {
					pos += tempo->frame_at_beat (bars * meter->divisions_per_bar(), _frame_rate);
				} else {
					pos += llrint (frames_per_beat * (bars * meter->divisions_per_bar()));
				}

				bars = 0;

				if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
					tempo = t;
				} else if ((m = dynamic_cast<const MeterSection*>(*i)) != 0) {
					meter = m;
				}
				++i;
				frames_per_beat = tempo->frames_per_beat (_frame_rate);
			}
		}

	}

	if (next_tempo) {
		pos += tempo->frame_at_beat (bars * meter->divisions_per_bar(), _frame_rate);
	} else {
		pos += llrint (frames_per_beat * (bars * meter->divisions_per_bar()));
	}

	uint64_t beats = 0;

	while (op.beats) {

		/* given the current meter, have we gone past the end of the bar ? */

		beats++;
		op.beats--;

		/* check if we need to use a new metric section: has adding frames moved us
		   to or after the start of the next metric section? in which case, use it.
		*/

		if (i != metrics.end()) {
			if ((*i)->frame() <= pos) {

				/* about to change tempo or meter, so add the
				 * number of frames for the beats we've just
				 * traversed before we change the
				 * frames_per_beat value.
				 */

				if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
					next_tempo = t;
				}

				if (next_tempo) {
					pos += tempo->frame_at_beat (beats, _frame_rate);
				} else {
					pos += llrint (beats * frames_per_beat);
				}

				beats = 0;

				if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
					tempo = t;
				} else if ((m = dynamic_cast<const MeterSection*>(*i)) != 0) {
					meter = m;
				}
				++i;
				frames_per_beat = tempo->frames_per_beat (_frame_rate);
			}
		}
	}

	if (next_tempo) {
		pos += tempo->frame_at_beat (beats, _frame_rate);
	} else {
		pos += llrint (beats * frames_per_beat);
	}

	if (op.ticks) {
		pos += tempo->frame_at_tick (op.ticks, _frame_rate);
	}

	return pos;

}

/** Count the number of beats that are equivalent to distance when going forward,
    starting at pos.
*/
Evoral::Beats
TempoMap::framewalk_to_beats (framepos_t pos, framecnt_t distance) const
{
	return Evoral::Beats(beat_at_frame (pos + distance) - beat_at_frame (pos));
}

struct bbtcmp {
    bool operator() (const BBT_Time& a, const BBT_Time& b) {
	    return a < b;
    }
};

std::ostream&
operator<< (std::ostream& o, const Meter& m) {
	return o << m.divisions_per_bar() << '/' << m.note_divisor();
}

std::ostream&
operator<< (std::ostream& o, const Tempo& t) {
	return o << t.beats_per_minute() << " 1/" << t.note_type() << "'s per minute";
}

std::ostream&
operator<< (std::ostream& o, const MetricSection& section) {

	o << "MetricSection @ " << section.frame() << ' ';

	const TempoSection* ts;
	const MeterSection* ms;

	if ((ts = dynamic_cast<const TempoSection*> (&section)) != 0) {
		o << *((const Tempo*) ts);
	} else if ((ms = dynamic_cast<const MeterSection*> (&section)) != 0) {
		//o << *((const Meter*) ms);
	}

	return o;
}
