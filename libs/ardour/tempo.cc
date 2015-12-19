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
	: MetricSection (BBT_Time()), Tempo (TempoMap::default_tempo())
{
	XMLProperty const * prop;
	BBT_Time start;
	LocaleGuard lg;

	if ((prop = node.property ("start")) == 0) {
		error << _("TempoSection XML node has no \"start\" property") << endmsg;
		throw failed_constructor();
	}

	if (sscanf (prop->value().c_str(), "%" PRIu32 "|%" PRIu32 "|%" PRIu32,
		    &start.bars,
		    &start.beats,
		    &start.ticks) < 3) {
		error << _("TempoSection XML node has an illegal \"start\" value") << endmsg;
		throw failed_constructor();
	}

	set_start (start);

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
		_type = TempoSectionType::Ramp;
	} else {
		if (strstr(prop->value().c_str(),"Constant")) {
			_type = TempoSectionType::Constant;
		} else {
			_type = TempoSectionType::Ramp;
		}
	}
}

XMLNode&
TempoSection::get_state() const
{
	XMLNode *root = new XMLNode (xml_state_node_name);
	char buf[256];
	LocaleGuard lg;

	snprintf (buf, sizeof (buf), "%" PRIu32 "|%" PRIu32 "|%" PRIu32,
		  start().bars,
		  start().beats,
		  start().ticks);
	root->add_property ("start", buf);
	snprintf (buf, sizeof (buf), "%f", _beats_per_minute);
	root->add_property ("beats-per-minute", buf);
	snprintf (buf, sizeof (buf), "%f", _note_type);
	root->add_property ("note-type", buf);
	// snprintf (buf, sizeof (buf), "%f", _bar_offset);
	// root->add_property ("bar-offset", buf);
	snprintf (buf, sizeof (buf), "%s", movable()?"yes":"no");
	root->add_property ("movable", buf);

	snprintf (buf, sizeof (buf), "%s", _type == Constant?"Constant":"Ramp");
	root->add_property ("tempo-type", buf);

	return *root;
}

void

TempoSection::update_bar_offset_from_bbt (const Meter& m)
{
	_bar_offset = ((start().beats - 1) * BBT_Time::ticks_per_beat + start().ticks) /
		(m.divisions_per_bar() * BBT_Time::ticks_per_beat);

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("Tempo set bar offset to %1 from %2 w/%3\n", _bar_offset, start(), m.divisions_per_bar()));
}

void
TempoSection::set_type (TempoSectionType type)
{
	_type = type;
}

double
TempoSection::tempo_at_frame (framepos_t frame, double end_bpm, framepos_t end_frame, framecnt_t frame_rate) const
{

	if (_type == Constant) {
		return beats_per_minute();
	}
 
	return tick_tempo_at_time (frame_to_minute (frame, frame_rate), end_bpm *  BBT_Time::ticks_per_beat, frame_to_minute (end_frame, frame_rate)) / BBT_Time::ticks_per_beat;
}

framepos_t
TempoSection::frame_at_tempo (double tempo, double end_bpm, framepos_t end_frame, framecnt_t frame_rate) const
{
	if (_type == Constant) {
		return 0;
	}

	return minute_to_frame (time_at_tick_tempo (tempo *  BBT_Time::ticks_per_beat,  end_bpm *  BBT_Time::ticks_per_beat, frame_to_minute (end_frame, frame_rate)), frame_rate);
}

double
TempoSection::tick_at_frame (framepos_t frame, double end_bpm, framepos_t end_frame, framecnt_t frame_rate) const
{
	if (_type == Constant) {
		return frame / frames_per_beat (frame_rate);
	}

	return tick_at_time (frame_to_minute (frame, frame_rate), end_bpm *  BBT_Time::ticks_per_beat, frame_to_minute (end_frame, frame_rate));
}

framepos_t
TempoSection::frame_at_tick (double tick, double end_bpm, framepos_t end_frame, framecnt_t frame_rate) const
{
	if (_type == Constant) {
		return (framepos_t) floor (tick * frames_per_beat(frame_rate));
	}

	return minute_to_frame (time_at_tick (tick, end_bpm *  BBT_Time::ticks_per_beat, frame_to_minute (end_frame, frame_rate)), frame_rate);
}

double TempoSection::beat_at_frame (framepos_t frame, double end_bpm, framepos_t end_frame, framecnt_t frame_rate) const
{
	return tick_at_frame (frame, end_bpm, end_frame, frame_rate) / BBT_Time::ticks_per_beat;
}

framepos_t TempoSection::frame_at_beat (double beat, double end_bpm, framepos_t end_frame, framecnt_t frame_rate) const
{
	return frame_at_tick (beat * BBT_Time::ticks_per_beat, end_bpm, end_frame, frame_rate);
}

framecnt_t
TempoSection::minute_to_frame (double time, framecnt_t frame_rate) const
{
	return time * 60.0 * frame_rate;
}

double
TempoSection::frame_to_minute (framecnt_t frame, framecnt_t frame_rate) const
{
	return (frame / (double) frame_rate) / 60.0;
}

/* constant for exp */
double
TempoSection::a_func (double begin_tpm, double end_tpm, double end_time) const
{
	return log (end_tpm / ticks_per_minute()) /  c_func (end_tpm, end_time);
}
double
TempoSection::c_func (double end_tpm, double end_time) const
{
	return log (end_tpm / ticks_per_minute()) /  end_time;
}

/* tempo in tpm at time in minutes */
double
TempoSection::tick_tempo_at_time (double time, double end_tpm, double end_time) const
{
	return exp (c_func (end_tpm, end_time) * time) * ticks_per_minute();
}

/* time in minutes at tempo in tpm */
double
TempoSection::time_at_tick_tempo (double tick_tempo, double end_tpm, double end_time) const
{
	return log (tick_tempo / ticks_per_minute()) / c_func (end_tpm, end_time);
}

/* tempo in bpm at time in minutes */
double
TempoSection::tempo_at_time (double time, double end_bpm, double end_time) const
{
	return tick_tempo_at_time (time, end_bpm *  BBT_Time::ticks_per_beat, end_time) / BBT_Time::ticks_per_beat;
}

/* time in minutes at tempo in bpm */
double
TempoSection::time_at_tempo (double tempo, double end_bpm, double end_time) const
{
	return time_at_tick_tempo (tempo * BBT_Time::ticks_per_beat, end_bpm * BBT_Time::ticks_per_beat, end_time);
}

/* tick at time in minutes */
double
TempoSection::tick_at_time (double time, double end_tpm, double end_time) const
{
	return ((exp (c_func (end_tpm, end_time) * time)) - 1) * ticks_per_minute() / c_func (end_tpm, end_time);
}

/* time in minutes at tick */
double
TempoSection::time_at_tick (double tick, double end_tpm, double end_time) const
{
	return log (((c_func (end_tpm, end_time) * tick) / ticks_per_minute()) + 1) / c_func (end_tpm, end_time);
}

/* beat at time in minutes */
double
TempoSection::beat_at_time (double time, double end_tpm, double end_time) const
{
	return tick_at_time (time, end_tpm, end_time) / BBT_Time::ticks_per_beat;
}

/* time in munutes at beat */
double
TempoSection::time_at_beat (double beat, double end_tpm, double end_time) const
{
	return time_at_tick (beat * BBT_Time::ticks_per_beat, end_tpm, end_time);
}

void
TempoSection::update_bbt_time_from_bar_offset (const Meter& meter)
{
	BBT_Time new_start;

	if (_bar_offset < 0.0) {
		/* not set yet */
		return;
	}

	new_start.bars = start().bars;

	double ticks = BBT_Time::ticks_per_beat * meter.divisions_per_bar() * _bar_offset;
	new_start.beats = (uint32_t) floor (ticks/BBT_Time::ticks_per_beat);
	new_start.ticks = 0; /* (uint32_t) fmod (ticks, BBT_Time::ticks_per_beat); */

	/* remember the 1-based counting properties of beats */
	new_start.beats += 1;
	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("from bar offset %1 and dpb %2, ticks = %3->%4 beats = %5\n",
						       _bar_offset, meter.divisions_per_bar(), ticks, new_start.ticks, new_start.beats));

	set_start (new_start);
}

/***********************************************************************/

const string MeterSection::xml_state_node_name = "Meter";

MeterSection::MeterSection (const XMLNode& node)
	: MetricSection (BBT_Time()), Meter (TempoMap::default_meter())
{
	XMLProperty const * prop;
	BBT_Time start;
	LocaleGuard lg;

	if ((prop = node.property ("start")) == 0) {
		error << _("MeterSection XML node has no \"start\" property") << endmsg;
		throw failed_constructor();
	}

	if (sscanf (prop->value().c_str(), "%" PRIu32 "|%" PRIu32 "|%" PRIu32,
		    &start.bars,
		    &start.beats,
		    &start.ticks) < 3) {
		error << _("MeterSection XML node has an illegal \"start\" value") << endmsg;
		throw failed_constructor();
	}

	set_start (start);

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
		  start().bars,
		  start().beats,
		  start().ticks);
	root->add_property ("start", buf);
	snprintf (buf, sizeof (buf), "%f", _note_type);
	root->add_property ("note-type", buf);
	snprintf (buf, sizeof (buf), "%f", _divisions_per_bar);
	root->add_property ("divisions-per-bar", buf);
	snprintf (buf, sizeof (buf), "%s", movable()?"yes":"no");
	root->add_property ("movable", buf);

	return *root;
}

/***********************************************************************/

struct MetricSectionSorter {
    bool operator() (const MetricSection* a, const MetricSection* b) {
	    return a->start() < b->start();
    }
};

TempoMap::TempoMap (framecnt_t fr)
{
	_frame_rate = fr;
	BBT_Time start;

	start.bars = 1;
	start.beats = 1;
	start.ticks = 0;

	TempoSection *t = new TempoSection (start, _default_tempo.beats_per_minute(), _default_tempo.note_type(), TempoSection::TempoSectionType::Ramp);
	MeterSection *m = new MeterSection (start, _default_meter.divisions_per_bar(), _default_meter.note_divisor());

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

	assert (section->start().ticks == 0);

	/* we only allow new meters to be inserted on beat 1 of an existing
	 * measure.
	 */

	if (dynamic_cast<MeterSection*>(section)) {

		/* we need to (potentially) update the BBT times of tempo
		   sections based on this new meter.
		*/

		if ((section->start().beats != 1) || (section->start().ticks != 0)) {

			BBT_Time corrected = section->start();
			corrected.beats = 1;
			corrected.ticks = 0;

			warning << string_compose (_("Meter changes can only be positioned on the first beat of a bar. Moving from %1 to %2"),
						   section->start(), corrected) << endmsg;

			section->set_start (corrected);
		}
	}



	/* Look for any existing MetricSection that is of the same type and
	   in the same bar as the new one, and remove it before adding
	   the new one. Note that this means that if we find a matching,
	   existing section, we can break out of the loop since we're
	   guaranteed that there is only one such match.
	*/

	for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {

		bool const iter_is_tempo = dynamic_cast<TempoSection*> (*i) != 0;
		bool const insert_is_tempo = dynamic_cast<TempoSection*> (section) != 0;

		if (iter_is_tempo && insert_is_tempo) {

			/* Tempo sections */

			if ((*i)->start().bars == section->start().bars &&
			    (*i)->start().beats == section->start().beats) {

				if (!(*i)->movable()) {

					/* can't (re)move this section, so overwrite
					 * its data content (but not its properties as
					 * a section).
					 */

					*(dynamic_cast<Tempo*>(*i)) = *(dynamic_cast<Tempo*>(section));
					need_add = false;
				} else {
					metrics.erase (i);
				}
				break;
			}

		} else if (!iter_is_tempo && !insert_is_tempo) {

			/* Meter Sections */

			if ((*i)->start().bars == section->start().bars) {

				if (!(*i)->movable()) {

					/* can't (re)move this section, so overwrite
					 * its data content (but not its properties as
					 * a section
					 */

					*(dynamic_cast<Meter*>(*i)) = *(dynamic_cast<Meter*>(section));
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

		Metrics::iterator i;

		for (i = metrics.begin(); i != metrics.end(); ++i) {
			if ((*i)->start() > section->start()) {
				break;
			}
		}

		metrics.insert (i, section);
	}
}

void
TempoMap::replace_tempo (const TempoSection& ts, const Tempo& tempo, const BBT_Time& where, TempoSection::TempoSectionType type)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		TempoSection& first (first_tempo());
		TempoSection::TempoSectionType tt = first.type();

		if (ts.start() != first.start()) {
			remove_tempo_locked (ts);
			add_tempo_locked (tempo, where, true, tt);
		} else {
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
TempoMap::add_tempo (const Tempo& tempo, BBT_Time where, ARDOUR::TempoSection::TempoSectionType type)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		add_tempo_locked (tempo, where, true, type);
	}


	PropertyChanged (PropertyChange ());
}

void
TempoMap::add_tempo_locked (const Tempo& tempo, BBT_Time where, bool recompute, ARDOUR::TempoSection::TempoSectionType type)
{
	/* new tempos always start on a beat */
	where.ticks = 0;
	TempoSection* ts = new TempoSection (where, tempo.beats_per_minute(), tempo.note_type(), type);

	/* find the meter to use to set the bar offset of this
	 * tempo section.
	 */

	const Meter* meter = &first_meter();

	/* as we start, we are *guaranteed* to have m.meter and m.tempo pointing
	   at something, because we insert the default tempo and meter during
	   TempoMap construction.

	   now see if we can find better candidates.
	*/

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {

		const MeterSection* m;

		if (where < (*i)->start()) {
			break;
		}

		if ((m = dynamic_cast<const MeterSection*>(*i)) != 0) {
			meter = m;
		}
	}

	ts->update_bar_offset_from_bbt (*meter);

	/* and insert it */

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

		if (ms.start() != first.start()) {
			remove_meter_locked (ms);
			add_meter_locked (meter, where, true);
		} else {
			/* cannot move the first meter section */
			*static_cast<Meter*>(&first) = meter;
			recompute_map (true);
		}
	}

	PropertyChanged (PropertyChange ());
}

void
TempoMap::add_meter (const Meter& meter, BBT_Time where)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		add_meter_locked (meter, where, true);
	}


#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::TempoMap)) {
		dump (std::cerr);
	}
#endif

	PropertyChanged (PropertyChange ());
}

void
TempoMap::add_meter_locked (const Meter& meter, BBT_Time where, bool recompute)
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

	do_insert (new MeterSection (where, meter.divisions_per_bar(), meter.note_divisor()));

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

	MeterSection* meter = 0;
	TempoSection* tempo = 0;
	double current_frame;
	BBT_Time current;
	Metrics::iterator next_metric;

	if (end < 0) {

		/* we will actually stop once we hit
		   the last metric.
		*/
		end = max_framepos;

	} else {
		/*
		if (!_map.empty ()) {
			/* never allow the map to be shortened /
			end = max (end, _map.back().frame);
		}
		*/
	}

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("recomputing tempo map, zero to %1\n", end));

	for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {
		MeterSection* ms;

		if ((ms = dynamic_cast<MeterSection *> (*i)) != 0) {
			meter = ms;
			break;
		}
	}

	assert(meter);

	for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* ts;

		if ((ts = dynamic_cast<TempoSection *> (*i)) != 0) {
			tempo = ts;
			break;
		}
	}

	assert(tempo);

	/* assumes that the first meter & tempo are at frame zero */
	current_frame = 0;
	meter->set_frame (0);
	tempo->set_frame (0);

	/* assumes that the first meter & tempo are at 1|1|0 */
	current.bars = 1;
	current.beats = 1;
	current.ticks = 0;
	if (reassign_tempo_bbt) {

		MeterSection* rmeter = meter;

		DEBUG_TRACE (DEBUG::TempoMath, "\tUpdating tempo marks BBT time from bar offset\n");

		for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {

			TempoSection* ts;
			MeterSection* ms;

			if ((ts = dynamic_cast<TempoSection*>(*i)) != 0) {

				/* reassign the BBT time of this tempo section
				 * based on its bar offset position.
				 */

				ts->update_bbt_time_from_bar_offset (*rmeter);

			} else if ((ms = dynamic_cast<MeterSection*>(*i)) != 0) {
				rmeter = ms;
			} else {
				fatal << _("programming error: unhandled MetricSection type") << endmsg;
				abort(); /*NOTREACHED*/
			}
		}
	}

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("start with meter = %1 tempo = %2\n", *((Meter*)meter), *((Tempo*)tempo)));

	next_metric = metrics.begin();
	++next_metric; // skip meter (or tempo)
	++next_metric; // skip tempo (or meter)

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("Add first bar at 1|1 @ %2\n", current.bars, current_frame));

	if (end == 0) {
		/* silly call from Session::process() during startup
		 */
		return;
	}

	_extend_map (tempo, meter, next_metric, current, current_frame, end);
}

void
TempoMap::_extend_map (TempoSection* tempo, MeterSection* meter,
		       Metrics::iterator next_metric,
		       BBT_Time current, framepos_t current_frame, framepos_t end)
{
	/* CALLER MUST HOLD WRITE LOCK */

	uint32_t first_tick_in_new_meter = 0;
	Metrics::const_iterator i;
	TempoSection* prev_ts = tempo;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		MeterSection* m = 0;

		if ((m = dynamic_cast<MeterSection*> (*i)) != 0) {

			if (m->start() >= prev_ts->start()) {
				first_tick_in_new_meter = ((((m->start().bars - 1) * meter->divisions_per_bar()) + (m->start().beats - 1)) * BBT_Time::ticks_per_beat) + m->start().ticks; // expressed in ticks from the previous meter
				for (i = metrics.begin(); i != metrics.end(); ++i) {
					TempoSection* t;

					if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {

						if (t->start() >= m->start() && t->start() > prev_ts->start()) {
							//cerr << "new ts start bars = " << t->start().bars << " beats = " << t->start().beats << " ticks = " << t->start().ticks << endl;
							//cerr << "prev ts start bars = " << prev_ts->start().bars << " beats = " << prev_ts->start().beats << " ticks = " << prev_ts->start().ticks << endl;

							/*tempo section (t) lies in the previous meter */
							double ticks_at_ts = ((((t->start().bars - 1 ) * meter->divisions_per_bar()) + (t->start().beats - 1) )  * BBT_Time::ticks_per_beat) + t->start().ticks;


							double ticks_at_prev_ts = ((((prev_ts->start().bars - 1) * meter->divisions_per_bar()) + (prev_ts->start().beats - 1))  * BBT_Time::ticks_per_beat) + prev_ts->start().ticks;

							double ticks_relative_to_prev_ts = ticks_at_ts - ticks_at_prev_ts;
							/* assume (falsely) that the target tempo is constant */
							double length_estimate = (ticks_relative_to_prev_ts /  BBT_Time::ticks_per_beat) * meter->frames_per_grid (*t, _frame_rate);
							double system_precision_at_target_tempo =  (_frame_rate / t->ticks_per_minute());
							cerr << " system_precision_at_target_tempo = " << system_precision_at_target_tempo << endl;
							double tick_error = system_precision_at_target_tempo + 1.0; // sorry for the wtf

							while (fabs (tick_error) >= system_precision_at_target_tempo) {

								double actual_ticks = prev_ts->tick_at_frame (length_estimate, t->beats_per_minute(), (framepos_t) length_estimate, _frame_rate);
								tick_error = ticks_relative_to_prev_ts - actual_ticks;
								length_estimate += (tick_error / BBT_Time::ticks_per_beat) * meter->frames_per_grid (*t, _frame_rate);
								cerr << "actual ticks = " << actual_ticks << endl;

								cerr << "tick error  = " << tick_error << endl;
							}
							t->set_frame (length_estimate + prev_ts->frame());

							if (m->start() < t->start() && m->start() == prev_ts->start()) {
								m->set_frame (prev_ts->frame());
							} else if (m->start() < t->start() && m->start() > prev_ts->start()) {
								m->set_frame (prev_ts->frame_at_tick ((first_tick_in_new_meter - ticks_at_prev_ts), t->beats_per_minute(), (framepos_t) length_estimate, _frame_rate));
							}
						}
						prev_ts = t;
					}
				}
			}
			meter = m;
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

		BBT_Time section_start ((*i)->start());

		if (section_start.bars > bbt.bars || (section_start.bars == bbt.bars && section_start.beats > bbt.beats)) {
			break;
		}

		m.set_metric (*i);
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
	bbt = beats_to_bbt (beat_at_frame (frame));
}

int32_t
TempoMap::bars_in_meter_section (MeterSection* ms) const
{
	/* YOU MUST HAVE THE READ LOCK */
	Metrics::const_iterator i;

	MeterSection* next_ms = 0;
	const MeterSection* prev_ms = &first_meter();

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		MeterSection* m;
		if ((m = dynamic_cast<MeterSection*> (*i)) != 0) {
			if (ms->frame() < m->frame()) {
				next_ms = m;
				break;
			}
			prev_ms = m;
		}
	}
	if (next_ms) {
		double ticks_at_next = tick_at_frame (next_ms->frame());
		double ticks_at_prev = tick_at_frame (prev_ms->frame());
		double ticks_in_meter = ticks_at_next - ticks_at_prev;

		return (int32_t) floor ((ticks_in_meter / BBT_Time::ticks_per_beat) / prev_ms->note_divisor());
	}
	return -1;
}

Timecode::BBT_Time
TempoMap::beats_to_bbt (double beats)
{
	/* CALLER HOLDS READ LOCK */
	BBT_Time ret;
	MeterSection* prev_ms = &first_meter();

	framecnt_t frame = frame_at_beat (beats);
	uint32_t cnt = 0;

	if (n_meters() < 2) {
		uint32_t bars = (uint32_t) floor (beats / prev_ms->note_divisor());
		double remaining_beats = beats - (bars *  prev_ms->note_divisor());
		double remaining_ticks = (remaining_beats - floor (remaining_beats)) * BBT_Time::ticks_per_beat;

		ret.ticks = (uint32_t) floor (remaining_ticks + 0.5);
		ret.beats = (uint32_t) floor (remaining_beats);
		ret.bars = bars;

		/* 0 0 0 to 1 1 0 - based mapping*/
		++ret.bars;
		++ret.beats;

		if (ret.ticks >= BBT_Time::ticks_per_beat) {
			++ret.beats;
			ret.ticks -= BBT_Time::ticks_per_beat;
		}

		if (ret.beats > prev_ms->note_divisor()) {
			++ret.bars;
			ret.beats = 1;
		}

		return ret;
	}

	uint32_t first_beat_in_meter = 0;
	uint32_t accumulated_bars = 0;
	Metrics::const_iterator i;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		MeterSection* m = 0;

		if ((m = dynamic_cast<MeterSection*> (*i)) != 0) {
			first_beat_in_meter = beat_at_frame (m->frame());

			if (beats < first_beat_in_meter) {
				/* this is the meter after the one our beat is on*/
				break;
			}
			int32_t const bars_in_ms = bars_in_meter_section (m);

			if (bars_in_ms > 0) {
				accumulated_bars += bars_in_ms;
			}

			prev_ms = m;
			++cnt;
		}
	}
	//cerr << "beats to bbr with beats = " << beats << " first_beat_in_meter =  " << first_beat_in_meter << " accumulated_bars = " << accumulated_bars <<  endl;

	if (beats > first_beat_in_meter) {
		/* prev_ms is the relevant one here */

		/* now get the ticks at frame */
		double ticks_at_frame = tick_at_frame (frame);

		/* find the number of ticks at the beginning of the meter section (bar 1)*/
		double ticks_at_ms = tick_at_frame (prev_ms->frame());

		double beats_used_by_ms = (ticks_at_frame - ticks_at_ms) / BBT_Time::ticks_per_beat;

		uint32_t bars = (uint32_t) floor (beats_used_by_ms / prev_ms->note_divisor());
		double remaining_beats = beats_used_by_ms - (bars *  prev_ms->note_divisor());
		double remaining_ticks = (remaining_beats - floor (remaining_beats)) * BBT_Time::ticks_per_beat;

		ret.bars = bars + accumulated_bars;
		ret.beats = (uint32_t) floor (remaining_beats);
		ret.ticks = (uint32_t) floor (remaining_ticks + 0.5);

		/* now ensure we srtart at 1 1 0 */
		++ret.bars;
		++ret.beats;
		//cerr << "part 1 ret bars = " << ret.bars << " ret beats = " << ret.beats << " ret ticks = " << ret.ticks << endl;
		if (ret.ticks >= BBT_Time::ticks_per_beat) {
			++ret.beats;
			ret.ticks -= BBT_Time::ticks_per_beat;
		}

		if (ret.beats > prev_ms->note_divisor()) {
			++ret.bars;
			ret.beats = 1;
		}

		return ret;
	}

	/* find the number of ticks at the beginning of the meter section (bar 1)*/
	double ticks_at_ms = tick_at_frame (prev_ms->frame());

	/* now get the ticks at frame */
	double ticks_at_frame = tick_at_frame (frame);

	double ticks_within_ms = ticks_at_frame - ticks_at_ms;

	ret.bars = (uint32_t) floor (((ticks_within_ms / BBT_Time::ticks_per_beat) / prev_ms->note_divisor())) + accumulated_bars;
	uint32_t remaining_ticks = ticks_within_ms - (ret.bars * prev_ms->note_divisor() * BBT_Time::ticks_per_beat);
	ret.beats = (uint32_t) floor (remaining_ticks);
	remaining_ticks -= ret.beats * BBT_Time::ticks_per_beat;

	/* only round ticks */
	ret.ticks = (uint32_t) floor (remaining_ticks + 0.5);

	/* now ensure we srtart at 1 1 0 */
	++ret.bars;
	++ret.beats;
	if (ret.ticks >= BBT_Time::ticks_per_beat) {
		++ret.beats;
		ret.ticks -= BBT_Time::ticks_per_beat;
	}

	if (ret.beats > prev_ms->note_divisor()) {
		++ret.bars;
		ret.beats = 1;
	}

	return ret;
}

double
TempoMap::tick_at_frame (framecnt_t frame) const
{
	Metrics::const_iterator i;
	const TempoSection* prev_ts = &first_tempo();
	double accumulated_ticks = 0.0;
	uint32_t cnt = 0;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;

		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {

			if (frame < t->frame()) {
				/*the previous ts is the one containing the frame */

				framepos_t time = frame - prev_ts->frame();
				framepos_t last_frame = t->frame() - prev_ts->frame();
				double last_beats_per_minute = t->beats_per_minute();

				return prev_ts->tick_at_frame (time, last_beats_per_minute, last_frame, _frame_rate) + accumulated_ticks;
			}

			if (cnt > 0 && t->frame() > prev_ts->frame()) {
				framepos_t time = t->frame() - prev_ts->frame();
				framepos_t last_frame = t->frame() - prev_ts->frame();
				double last_beats_per_minute = t->beats_per_minute();
				accumulated_ticks += prev_ts->tick_at_frame (time, last_beats_per_minute, last_frame, _frame_rate);
			}

			prev_ts = t;
			++cnt;
		}
	}

	/* treated s linear for this ts */
	framecnt_t frames_in_section = frame - prev_ts->frame();
	double ticks_in_section = (frames_in_section / prev_ts->frames_per_beat (_frame_rate)) * Timecode::BBT_Time::ticks_per_beat;

	return ticks_in_section + accumulated_ticks;

}

framecnt_t
TempoMap::frame_at_tick (double tick) const
{
	double accumulated_ticks = 0.0;
	const TempoSection* prev_ts =  &first_tempo();
	uint32_t cnt = 0;

	Metrics::const_iterator i;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;
		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {

			if (cnt > 0 && t->frame() > prev_ts->frame()) {
				framepos_t time = t->frame() - prev_ts->frame();
				framepos_t last_time = t->frame() - prev_ts->frame();
				double last_beats_per_minute = t->beats_per_minute();
				accumulated_ticks += prev_ts->tick_at_frame (time, last_beats_per_minute, last_time, _frame_rate);
			}

			if (tick < accumulated_ticks) {
				/* prev_ts is the one affecting us. */

				double ticks_in_section = tick - tick_at_frame (prev_ts->frame());
				framepos_t section_start = prev_ts->frame();
				framepos_t last_time = t->frame() - prev_ts->frame();
				double last_beats_per_minute = t->beats_per_minute();
				return prev_ts->frame_at_tick (ticks_in_section, last_beats_per_minute, last_time, _frame_rate) + section_start;
			}

			prev_ts = t;
			++cnt;
		}
	}
	double ticks_in_section = tick - tick_at_frame (prev_ts->frame());
	double dtime = (ticks_in_section / BBT_Time::ticks_per_beat) * prev_ts->frames_per_beat(_frame_rate);
	framecnt_t ret = ((framecnt_t) floor (dtime)) + prev_ts->frame();

	return ret;
}

double
TempoMap::beat_at_frame (framecnt_t frame) const
{
	return tick_at_frame (frame) / BBT_Time::ticks_per_beat;
}

framecnt_t
TempoMap::frame_at_beat (double beat) const
{
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

	Metrics::const_iterator i;
	uint32_t accumulated_bars = 0;

	MeterSection* prev_ms = &first_meter();

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		MeterSection* m;
		if ((m = dynamic_cast<MeterSection*> (*i)) != 0) {
			int32_t const bims = bars_in_meter_section (m);

			if (bims < 0 || bbt.bars <= (accumulated_bars + bims)) {
				break;
			}
			if (bims > 0 ) {
				accumulated_bars += bims;
			}
			prev_ms = m;
		}
	}

	uint32_t remaining_bars = bbt.bars - accumulated_bars - 1; // back to zero - based bars
	double const ticks_within_prev_taken_by_remaining_bars = remaining_bars * prev_ms->note_divisor() * BBT_Time::ticks_per_beat;
	double const ticks_after_space_used_by_bars = ((bbt.beats - 1) * BBT_Time::ticks_per_beat) + bbt.ticks; // back to zero - based beats
	double const ticks_target = ticks_within_prev_taken_by_remaining_bars + ticks_after_space_used_by_bars;

	TempoSection* prev_ts = &first_tempo();
	double accumulated_ticks = 0.0;
	uint32_t cnt = 0;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;
		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
			if (t->frame() < prev_ms->frame()) {
				continue;
			}

			if (cnt > 0 && t->frame() > prev_ts->frame()) {
				/*find the number of ticke in this section */
				framepos_t const time = t->frame() - prev_ts->frame();
				framepos_t const last_time = t->frame() - prev_ts->frame();
				double const last_beats_per_minute = t->beats_per_minute();
				accumulated_ticks += prev_ts->tick_at_frame (time, last_beats_per_minute, last_time, _frame_rate);
			}

			if (ticks_target < accumulated_ticks) {
				double const ticks_in_section = ticks_target - tick_at_frame (prev_ts->frame());
				framepos_t const section_start_time = prev_ts->frame();
				framepos_t const last_time = t->frame() - prev_ts->frame();
				double const last_beats_per_minute = t->beats_per_minute();
				framepos_t const ret = prev_ts->frame_at_tick (ticks_in_section, last_beats_per_minute, last_time, _frame_rate) + section_start_time;
				return ret;
			}

			prev_ts = t;
			++cnt;
		}
	}

	/*treat this ts as constant tempo */
	double const ticks_in_this_ts = ticks_target - tick_at_frame (prev_ts->frame());
	double const dtime = (ticks_in_this_ts / BBT_Time::ticks_per_beat) * prev_ts->frames_per_beat(_frame_rate);
	framecnt_t const ret = ((framecnt_t) floor (dtime)) + prev_ts->frame();
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
		framepos_t const last_time = second->frame() - first->frame();
		double const last_beats_per_minute = second->beats_per_minute();

		framepos_t const time = pos - first->frame();
		double const tick_at_time = first->tick_at_frame (time, last_beats_per_minute, last_time, _frame_rate);
		double const bbt_ticks = bbt.ticks + (bbt.beats * BBT_Time::ticks_per_beat);

		double const time_at_bbt = first->frame_at_tick (tick_at_time + bbt_ticks, last_beats_per_minute, last_time, _frame_rate);

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

	BBT_Time bbt (beats_to_bbt (beat_at_framepos));

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
		BBT_Time const bbt = beats_to_bbt ((double) cnt);

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
				framepos_t const last_time = t->frame() - prev_ts->frame();
				double const last_beats_per_minute = t->beats_per_minute();
				double const ret = prev_ts->tempo_at_frame (time, last_beats_per_minute, last_time, _frame_rate);
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
							ts->update_bar_offset_from_bbt (*last_meter);
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

		/* check for multiple tempo/meters at the same location, which
		   ardour2 somehow allowed.
		*/

		Metrics::iterator prev = metrics.end();
		for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {
			if (prev != metrics.end()) {
				if (dynamic_cast<MeterSection*>(*prev) && dynamic_cast<MeterSection*>(*i)) {
					if ((*prev)->start() == (*i)->start()) {
						cerr << string_compose (_("Multiple meter definitions found at %1"), (*prev)->start()) << endmsg;
						error << string_compose (_("Multiple meter definitions found at %1"), (*prev)->start()) << endmsg;
						return -1;
					}
				} else if (dynamic_cast<TempoSection*>(*prev) && dynamic_cast<TempoSection*>(*i)) {
					if ((*prev)->start() == (*i)->start()) {
						cerr << string_compose (_("Multiple tempo definitions found at %1"), (*prev)->start()) << endmsg;
						error << string_compose (_("Multiple tempo definitions found at %1"), (*prev)->start()) << endmsg;
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
			o << "Tempo @ " << *i << " (Bar-offset: " << t->bar_offset() << ") " << t->beats_per_minute() << " BPM (pulse = 1/" << t->note_type() << ") at " << t->start() << " frame= " << t->frame() << " (movable? "
			  << t->movable() << ')' << endl;
		} else if ((m = dynamic_cast<const MeterSection*>(*i)) != 0) {
			o << "Meter @ " << *i << ' ' << m->divisions_per_bar() << '/' << m->note_divisor() << " at " << m->start() << " frame= " << m->frame()
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
			TempoMetric metric (*meter, *tempo);

			if (prev) {
				metric.set_start (prev->start());
				metric.set_frame (prev->frame());
			} else {
				// metric will be at frames=0 bbt=1|1|0 by default
				// which is correct for our purpose
			}

			bbt_time ((*i)->frame(), bbt);

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

			// cerr << bbt << endl;

			(*i)->set_start (bbt);

			if ((t = dynamic_cast<TempoSection*>(*i)) != 0) {
				tempo = t;
				// cerr << "NEW TEMPO, frame = " << (*i)->frame() << " start = " << (*i)->start() <<endl;
			} else if ((m = dynamic_cast<MeterSection*>(*i)) != 0) {
				meter = m;
				// cerr << "NEW METER, frame = " << (*i)->frame() << " start = " << (*i)->start() <<endl;
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
	Glib::Threads::RWLock::ReaderLock lm (lock);
	Metrics::const_iterator i;
	const MeterSection* meter;
	const MeterSection* m;
	const TempoSection* tempo;
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

	/* We now have:

	   meter -> the Meter for "pos"
	   tempo -> the Tempo for "pos"
	   i     -> for first new metric after "pos", possibly metrics.end()
	*/

	/* now comes the complicated part. we have to add one beat a time,
	   checking for a new metric on every beat.
	*/

	frames_per_beat = tempo->frames_per_beat (_frame_rate);

	uint64_t bars = 0;

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

				pos += llrint (frames_per_beat * (bars * meter->divisions_per_bar()));
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

	pos += llrint (frames_per_beat * (bars * meter->divisions_per_bar()));

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

				pos += llrint (beats * frames_per_beat);
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

	pos += llrint (beats * frames_per_beat);

	if (op.ticks) {
		if (op.ticks >= BBT_Time::ticks_per_beat) {
			pos += llrint (frames_per_beat + /* extra beat */
				       (frames_per_beat * ((op.ticks % (uint32_t) BBT_Time::ticks_per_beat) /
							   (double) BBT_Time::ticks_per_beat)));
		} else {
			pos += llrint (frames_per_beat * (op.ticks / (double) BBT_Time::ticks_per_beat));
		}
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

	o << "MetricSection @ " << section.frame() << " aka " << section.start() << ' ';

	const TempoSection* ts;
	const MeterSection* ms;

	if ((ts = dynamic_cast<const TempoSection*> (&section)) != 0) {
		o << *((const Tempo*) ts);
	} else if ((ms = dynamic_cast<const MeterSection*> (&section)) != 0) {
		o << *((const Meter*) ms);
	}

	return o;
}
