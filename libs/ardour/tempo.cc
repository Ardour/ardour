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

#include <unistd.h>

#include <cmath>

#include <glibmm/thread.h>
#include "pbd/xml++.h"
#include "evoral/types.hpp"
#include "ardour/debug.h"
#include "ardour/tempo.h"
#include "ardour/utils.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

using Timecode::BBT_Time;

/* _default tempo is 4/4 qtr=120 */

Meter    TempoMap::_default_meter (4.0, 4.0);
Tempo    TempoMap::_default_tempo (120.0);

double 
Tempo::frames_per_beat (framecnt_t sr) const
{
	return  (60.0 * sr) / _beats_per_minute;
}

/***********************************************************************/

double 
Meter::frames_per_division (const Tempo& tempo, framecnt_t sr) const
{
	return (60.0 * sr) / (tempo.beats_per_minute() * (_note_type/tempo.note_type()));
}

double
Meter::frames_per_bar (const Tempo& tempo, framecnt_t sr) const
{
	return frames_per_division (tempo, sr) * _divisions_per_bar;
}

/***********************************************************************/

const string TempoSection::xml_state_node_name = "Tempo";

TempoSection::TempoSection (const XMLNode& node)
	: MetricSection (BBT_Time()), Tempo (TempoMap::default_tempo())
{
	const XMLProperty *prop;
	BBT_Time start;
	LocaleGuard lg (X_("POSIX"));

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
}

XMLNode&
TempoSection::get_state() const
{
	XMLNode *root = new XMLNode (xml_state_node_name);
	char buf[256];
	LocaleGuard lg (X_("POSIX"));

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

	return *root;
}

void

TempoSection::update_bar_offset_from_bbt (const Meter& m)
{
	_bar_offset = ((start().beats - 1) * BBT_Time::ticks_per_bar_division + start().ticks) / 
		(m.divisions_per_bar() * BBT_Time::ticks_per_bar_division);

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("Tempo set bar offset to %1 from %2 w/%3\n", _bar_offset, start(), m.divisions_per_bar()));
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
	
	double ticks = BBT_Time::ticks_per_bar_division * meter.divisions_per_bar() * _bar_offset;
	new_start.beats = (uint32_t) floor(ticks/BBT_Time::ticks_per_bar_division);
	new_start.ticks = (uint32_t) fmod (ticks, BBT_Time::ticks_per_bar_division);

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
	const XMLProperty *prop;
	BBT_Time start;
	LocaleGuard lg (X_("POSIX"));

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
	LocaleGuard lg (X_("POSIX"));

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
	metrics = new Metrics;
	_map = new BBTPointList;
	_frame_rate = fr;
	last_bbt_valid = false;
	BBT_Time start;

	start.bars = 1;
	start.beats = 1;
	start.ticks = 0;

	TempoSection *t = new TempoSection (start, _default_tempo.beats_per_minute(), _default_tempo.note_type());
	MeterSection *m = new MeterSection (start, _default_meter.divisions_per_bar(), _default_meter.note_divisor());

	t->set_movable (false);
	m->set_movable (false);

	/* note: frame time is correct (zero) for both of these */

	metrics->push_back (t);
	metrics->push_back (m);
}

TempoMap::~TempoMap ()
{
	delete metrics;
	delete _map;
}

void
TempoMap::remove_tempo (const TempoSection& tempo, bool complete_operation)
{
	bool removed = false;

	{
		Glib::RWLock::WriterLock lm (metrics_lock);
		Metrics::iterator i;

		for (i = metrics->begin(); i != metrics->end(); ++i) {
			if (dynamic_cast<TempoSection*> (*i) != 0) {
				if (tempo.frame() == (*i)->frame()) {
					if ((*i)->movable()) {
						metrics->erase (i);
						removed = true;
						break;
					}
				}
			}
		}
	}

	if (removed && complete_operation) {
		recompute_map (false);
		PropertyChanged (PropertyChange ());
	}
}

void
TempoMap::remove_meter (const MeterSection& tempo, bool complete_operation)
{
	bool removed = false;

	{
		Glib::RWLock::WriterLock lm (metrics_lock);
		Metrics::iterator i;

		for (i = metrics->begin(); i != metrics->end(); ++i) {
			if (dynamic_cast<MeterSection*> (*i) != 0) {
				if (tempo.frame() == (*i)->frame()) {
					if ((*i)->movable()) {
						metrics->erase (i);
						removed = true;
						break;
					}
				}
			}
		}
	}

	if (removed && complete_operation) {
		recompute_map (true);
		PropertyChanged (PropertyChange ());
	}
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

	Metrics::iterator i;

	/* Look for any existing MetricSection that is of the same type and
	   at the same time as the new one, and remove it before adding
	   the new one.
	*/

	Metrics::iterator to_remove = metrics->end ();

	for (i = metrics->begin(); i != metrics->end(); ++i) {

		int const c = (*i)->compare (*section);

		if (c < 0) {
			/* this section is before the one to be added; go back round */
			continue;
		} else if (c > 0) {
			/* this section is after the one to be added; there can't be any at the same time */
			break;
		}

		/* hacky comparison of type */
		bool const iter_is_tempo = dynamic_cast<TempoSection*> (*i) != 0;
		bool const insert_is_tempo = dynamic_cast<TempoSection*> (section) != 0;

		if (iter_is_tempo == insert_is_tempo) {

			if (!(*i)->movable()) {

				/* can't (re)move this section, so overwrite it
				 */

				if (!iter_is_tempo) {
					*(dynamic_cast<MeterSection*>(*i)) = *(dynamic_cast<MeterSection*>(section));
				} else {
					*(dynamic_cast<TempoSection*>(*i)) = *(dynamic_cast<TempoSection*>(section));
				}
				need_add = false;
				break;
			}

			to_remove = i;
			break;
		}
	}

	if (to_remove != metrics->end()) {
		/* remove the MetricSection at the same time as the one we are about to add */
		metrics->erase (to_remove);
	}

	/* Add the given MetricSection */

	if (need_add) {
		for (i = metrics->begin(); i != metrics->end(); ++i) {
			
			if ((*i)->compare (*section) < 0) {
				continue;
			}
			
			metrics->insert (i, section);
			break;
		}

		if (i == metrics->end()) {
			metrics->insert (metrics->end(), section);
		}
	}
}

void
TempoMap::replace_tempo (const TempoSection& ts, const Tempo& tempo, const BBT_Time& where)
{
	const TempoSection& first (first_tempo());

	if (ts != first) {
		remove_tempo (ts, false);
		add_tempo (tempo, where);
	} else {
		{
			Glib::RWLock::WriterLock lm (metrics_lock);
			/* cannot move the first tempo section */
			*((Tempo*)&first) = tempo;
		}

		recompute_map (false);
	}

	PropertyChanged (PropertyChange ());
}

void
TempoMap::add_tempo (const Tempo& tempo, BBT_Time where)
{
	{
		Glib::RWLock::WriterLock lm (metrics_lock);

		/* new tempos always start on a beat */
		where.ticks = 0;

		TempoSection* ts = new TempoSection (where, tempo.beats_per_minute(), tempo.note_type());
		
		/* find the meter to use to set the bar offset of this
		 * tempo section.
		 */

		const Meter* meter = &first_meter();
		
		/* as we start, we are *guaranteed* to have m.meter and m.tempo pointing
		   at something, because we insert the default tempo and meter during
		   TempoMap construction.
		   
		   now see if we can find better candidates.
		*/
		
		for (Metrics::const_iterator i = metrics->begin(); i != metrics->end(); ++i) {
			
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
	}

	recompute_map (false);

	PropertyChanged (PropertyChange ());
}

void
TempoMap::replace_meter (const MeterSection& ms, const Meter& meter, const BBT_Time& where)
{
	const MeterSection& first (first_meter());

	if (ms != first) {
		remove_meter (ms, false);
		add_meter (meter, where);
	} else {
		{
			Glib::RWLock::WriterLock lm (metrics_lock);
			/* cannot move the first meter section */
			*((Meter*)&first) = meter;
		}
		recompute_map (true);
	}

	PropertyChanged (PropertyChange ());
}

void
TempoMap::add_meter (const Meter& meter, BBT_Time where)
{
	{
		Glib::RWLock::WriterLock lm (metrics_lock);

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
	}

	recompute_map (true);
	
#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::TempoMap)) {
		dump (std::cerr);
	}
#endif

	PropertyChanged (PropertyChange ());
}

void
TempoMap::change_initial_tempo (double beats_per_minute, double note_type)
{
	Tempo newtempo (beats_per_minute, note_type);
	TempoSection* t;

	for (Metrics::iterator i = metrics->begin(); i != metrics->end(); ++i) {
		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
			{
				Glib::RWLock::WriterLock lm (metrics_lock);
				*((Tempo*) t) = newtempo;
			}
			recompute_map (false);
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

	for (first = 0, i = metrics->begin(), prev = 0; i != metrics->end(); ++i) {

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
		Glib::RWLock::WriterLock lm (metrics_lock);
		/* cannot move the first tempo section */
		*((Tempo*)prev) = newtempo;
	}

	recompute_map (false);
	PropertyChanged (PropertyChange ());
}

const MeterSection&
TempoMap::first_meter () const
{
	const MeterSection *m = 0;

	for (Metrics::const_iterator i = metrics->begin(); i != metrics->end(); ++i) {
		if ((m = dynamic_cast<const MeterSection *> (*i)) != 0) {
			return *m;
		}
	}

	fatal << _("programming error: no tempo section in tempo map!") << endmsg;
	/*NOTREACHED*/
	return *m;
}

const TempoSection&
TempoMap::first_tempo () const
{
	const TempoSection *t = 0;

	for (Metrics::const_iterator i = metrics->begin(); i != metrics->end(); ++i) {
		if ((t = dynamic_cast<const TempoSection *> (*i)) != 0) {
			return *t;
		}
	}

	fatal << _("programming error: no tempo section in tempo map!") << endmsg;
	/*NOTREACHED*/
	return *t;
}

void
TempoMap::timestamp_metrics_from_audio_time ()
{
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

	for (i = metrics->begin(); i != metrics->end(); ++i) {

		BBT_Time bbt;
		TempoMetric metric (*meter, *tempo);

		if (prev) {
			metric.set_start (prev->start());
			metric.set_frame (prev->frame());
		} else {
			// metric will be at frames=0 bbt=1|1|0 by default
			// which is correct for our purpose
		}

		BBTPointList::const_iterator bi = bbt_before_or_at ((*i)->frame());
		bbt_time_unlocked ((*i)->frame(), bbt, bi);
		
		// cerr << "timestamp @ " << (*i)->frame() << " with " << bbt.bars << "|" << bbt.beats << "|" << bbt.ticks << " => ";

		if (first) {
			first = false;
		} else {

			if (bbt.ticks > BBT_Time::ticks_per_bar_division/2) {
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
			/*NOTREACHED*/
		}

		prev = (*i);
	}

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::TempoMap)) {
		dump (cerr);
	}
#endif

}

void
TempoMap::require_map_to (framepos_t pos)
{
	bool revise_map;

	{
		Glib::RWLock::ReaderLock lm (map_lock);
		revise_map = (_map->empty() || _map->back().frame < pos);
	}

	if (revise_map) {
		recompute_map (false, pos);
	}
}

void
TempoMap::require_map_to (const BBT_Time& bbt)
{
	bool revise_map;

	{
		Glib::RWLock::ReaderLock lm (map_lock);
		revise_map = (_map->empty() || _map->back().bbt() < bbt);
	}

	if (revise_map) {
		recompute_map (false, 99);
	}
}

void
TempoMap::recompute_map (bool reassign_tempo_bbt, framepos_t end)
{
	MeterSection* meter;
	TempoSection* tempo;
	TempoSection* ts;
	MeterSection* ms;
	double divisions_per_bar;
	double beat_frames;
	double current_frame;
	BBT_Time current;
	Metrics::iterator next_metric;
	BBTPointList* new_map = new BBTPointList;

	if (end < 0) {

		Glib::RWLock::ReaderLock lm (map_lock);

		if (_map->empty()) {
			/* compute 1 mins worth */
			end = _frame_rate * 60;
		} else {
			end = _map->back().frame;
		}
	}

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("recomputing tempo map, zero to %1\n", end));

	Glib::RWLock::ReaderLock lm (metrics_lock);
	
	for (Metrics::iterator i = metrics->begin(); i != metrics->end(); ++i) {
		if ((ms = dynamic_cast<MeterSection *> (*i)) != 0) {
			meter = ms;
			break;
		}
	}

	for (Metrics::iterator i = metrics->begin(); i != metrics->end(); ++i) {
		if ((ts = dynamic_cast<TempoSection *> (*i)) != 0) {
			tempo = ts;
			break;
		}
	}

	/* assumes that the first meter & tempo are at frame zero */
	current_frame = 0;
	meter->set_frame (0);
	tempo->set_frame (0);

	/* assumes that the first meter & tempo are at 1|1|0 */
	current.bars = 1;
	current.beats = 1;
	current.ticks = 0;

	divisions_per_bar = meter->divisions_per_bar ();
	beat_frames = meter->frames_per_division (*tempo,_frame_rate);
	
	if (reassign_tempo_bbt) {

		MeterSection* rmeter = meter;

		DEBUG_TRACE (DEBUG::TempoMath, "\tUpdating tempo marks BBT time from bar offset\n");

		for (Metrics::iterator i = metrics->begin(); i != metrics->end(); ++i) {
			
			if ((ts = dynamic_cast<TempoSection*>(*i)) != 0) {

				/* reassign the BBT time of this tempo section
				 * based on its bar offset position.
				 */

				ts->update_bbt_time_from_bar_offset (*rmeter);

			} else if ((ms = dynamic_cast<MeterSection*>(*i)) != 0) {
				rmeter = ms;
			} else {
				fatal << _("programming error: unhandled MetricSection type") << endmsg;
				/*NOTREACHED*/
			}
		}
	}

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("start with meter = %1 tempo = %2 dpb %3 fpb %4\n", 
						       *((Meter*)meter), *((Tempo*)tempo), divisions_per_bar, beat_frames));

	next_metric = metrics->begin();
	++next_metric; // skip meter (or tempo)
	++next_metric; // skip tempo (or meter)

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("Add first bar at 1|1 @ %2\n", current.bars, current_frame));
	new_map->push_back (BBTPoint (*meter, *tempo,(framepos_t) llrint(current_frame), 1, 1));

	while (current_frame < end) {
		
		current.beats++;
		current_frame += beat_frames;

		if (current.beats > meter->divisions_per_bar()) {
			current.bars++;
			current.beats = 1;
		}

		if (next_metric != metrics->end()) {

			/* no operator >= so invert operator < */

			DEBUG_TRACE (DEBUG::TempoMath, string_compose ("now at %1 next metric @ %2\n", current, (*next_metric)->start()));

			if (!(current < (*next_metric)->start())) {

			  set_metrics:
				if (((ts = dynamic_cast<TempoSection*> (*next_metric)) != 0)) {

					tempo = ts;

					/* new tempo section: if its on a beat,
					 * we don't have to do anything other
					 * than recompute various distances,
					 * done further below as we transition
					 * the next metric section.
					 *
					 * if its not on the beat, we have to
					 * compute the duration of the beat it
					 * is within, which will be different
					 * from the preceding following ones
					 * since it takes part of its duration
					 * from the preceding tempo and part 
					 * from this new tempo.
					 */

					if (tempo->start().ticks != 0) {
						
						double next_beat_frames = meter->frames_per_division (*tempo,_frame_rate);					
						
						DEBUG_TRACE (DEBUG::TempoMath, string_compose ("bumped into non-beat-aligned tempo metric at %1 = %2, adjust next beat using %3\n",
											       tempo->start(), current_frame, tempo->bar_offset()));
						
						/* back up to previous beat */
						current_frame -= beat_frames;
						/* set tempo section location based on offset from last beat */
						tempo->set_frame (current_frame + (ts->bar_offset() * beat_frames));
						/* advance to the location of the new (adjusted) beat */
						current_frame += (ts->bar_offset() * beat_frames) + ((1.0 - ts->bar_offset()) * next_beat_frames);
						/* next metric doesn't have to
						 * match this precisely to
						 * merit a reloop ...
						 */
						DEBUG_TRACE (DEBUG::TempoMath, string_compose ("Adjusted last beat to %1\n", current_frame));
						
					} else {
						
						DEBUG_TRACE (DEBUG::TempoMath, string_compose ("bumped into beat-aligned tempo metric at %1 = %2\n",
											       tempo->start(), current_frame));
						tempo->set_frame (current_frame);
					}

				} else if ((ms = dynamic_cast<MeterSection*>(*next_metric)) != 0) {
					
					meter = ms;

					/* new meter section: always defines the
					 * start of a bar.
					 */
					
					DEBUG_TRACE (DEBUG::TempoMath, string_compose ("bumped into meter section at %1 vs %2 (%3)\n",
										       meter->start(), current, current_frame));
					
					assert (current.beats == 1);

					meter->set_frame (current_frame);
				}
				
				divisions_per_bar = meter->divisions_per_bar ();
				beat_frames = meter->frames_per_division (*tempo, _frame_rate);
				
				DEBUG_TRACE (DEBUG::TempoMath, string_compose ("New metric with beat frames = %1 dpb %2 meter %3 tempo %4\n", 
									       beat_frames, divisions_per_bar, *((Meter*)meter), *((Tempo*)tempo)));
			
				++next_metric;

				if (next_metric != metrics->end() && ((*next_metric)->start() == current)) {
					/* same position so go back and set this one up before advancing
					*/
					goto set_metrics;
				}
			}
		}

		if (current.beats == 1) {
			DEBUG_TRACE (DEBUG::TempoMath, string_compose ("Add Bar at %1|1 @ %2\n", current.bars, current_frame));
			new_map->push_back (BBTPoint (*meter, *tempo,(framepos_t) llrint(current_frame), current.bars, 1));
		} else {
			DEBUG_TRACE (DEBUG::TempoMath, string_compose ("Add Beat at %1|%2 @ %3\n", current.bars, current.beats, current_frame));
			new_map->push_back (BBTPoint (*meter, *tempo, (framepos_t) llrint(current_frame), current.bars, current.beats));
		}
	}

	{
		Glib::RWLock::WriterLock lm (map_lock);
		swap (_map, new_map);
		delete new_map;
	}
}

TempoMetric
TempoMap::metric_at (framepos_t frame) const
{
	Glib::RWLock::ReaderLock lm (metrics_lock);
	TempoMetric m (first_meter(), first_tempo());
	const Meter* meter;
	const Tempo* tempo;

	/* at this point, we are *guaranteed* to have m.meter and m.tempo pointing
	   at something, because we insert the default tempo and meter during
	   TempoMap construction.

	   now see if we can find better candidates.
	*/

	for (Metrics::const_iterator i = metrics->begin(); i != metrics->end(); ++i) {

		// cerr << "Looking at a metric section " << **i << endl;

		if ((*i)->frame() > frame) {
			break;
		}

		if ((tempo = dynamic_cast<const TempoSection*>(*i)) != 0) {
			m.set_tempo (*tempo);
		} else if ((meter = dynamic_cast<const MeterSection*>(*i)) != 0) {
			m.set_meter (*meter);
		}

		m.set_frame ((*i)->frame ());
		m.set_start ((*i)->start ());
	}
	
	// cerr << "for framepos " << frame << " returning " << m.meter() << " @ " << m.tempo() << " location " << m.frame() << " = " << m.start() << endl;
	return m;
}

TempoMetric
TempoMap::metric_at (BBT_Time bbt) const
{
	Glib::RWLock::ReaderLock lm (metrics_lock);
	TempoMetric m (first_meter(), first_tempo());
	const Meter* meter;
	const Tempo* tempo;

	/* at this point, we are *guaranteed* to have m.meter and m.tempo pointing
	   at something, because we insert the default tempo and meter during
	   TempoMap construction.

	   now see if we can find better candidates.
	*/

	for (Metrics::const_iterator i = metrics->begin(); i != metrics->end(); ++i) {

		BBT_Time section_start ((*i)->start());

		if (section_start.bars > bbt.bars || (section_start.bars == bbt.bars && section_start.beats > bbt.beats)) {
			break;
		}

		if ((tempo = dynamic_cast<const TempoSection*>(*i)) != 0) {
			m.set_tempo (*tempo);
		} else if ((meter = dynamic_cast<const MeterSection*>(*i)) != 0) {
			m.set_meter (*meter);
		}

		m.set_frame ((*i)->frame ());
		m.set_start (section_start);
	}

	return m;
}

void
TempoMap::bbt_time (framepos_t frame, BBT_Time& bbt)
{
	{
		Glib::RWLock::ReaderLock lm (map_lock);
		BBTPointList::const_iterator i = bbt_before_or_at (frame);
		bbt_time_unlocked (frame, bbt, i);
	}
}

void
TempoMap::bbt_time_unlocked (framepos_t frame, BBT_Time& bbt, const BBTPointList::const_iterator& i)
{
	bbt.bars = (*i).bar;
	bbt.beats = (*i).beat;

	if ((*i).frame == frame) {
		bbt.ticks = 0;
	} else {
		bbt.ticks = llrint (((frame - (*i).frame) / (*i).meter->frames_per_division(*((*i).tempo), _frame_rate)) *
				    BBT_Time::ticks_per_bar_division);
	}
}

framepos_t
TempoMap::frame_time (const BBT_Time& bbt)
{
	Glib::RWLock::ReaderLock lm (map_lock);

	BBTPointList::const_iterator s = bbt_point_for (BBT_Time (1, 1, 0));
	BBTPointList::const_iterator e = bbt_point_for (BBT_Time (bbt.bars, bbt.beats, 0));

	if (bbt.ticks != 0) {
		return ((*e).frame - (*s).frame) + 
			llrint ((*e).meter->frames_per_division (*(*e).tempo, _frame_rate) * (bbt.ticks/BBT_Time::ticks_per_bar_division));
	} else {
		return ((*e).frame - (*s).frame);
	}
}

framecnt_t
TempoMap::bbt_duration_at (framepos_t pos, const BBT_Time& bbt, int dir)
{
	Glib::RWLock::ReaderLock lm (map_lock);
	framecnt_t frames = 0;
	BBT_Time when;

	bbt_time (pos, when);
	frames = bbt_duration_at_unlocked (when, bbt,dir);

	return frames;
}

framecnt_t
TempoMap::bbt_duration_at_unlocked (const BBT_Time& when, const BBT_Time& bbt, int dir) 
{
	if (bbt.bars == 0 && bbt.beats == 0 && bbt.ticks == 0) {
		return 0;
	}

	/* round back to the previous precise beat */
	BBTPointList::const_iterator wi = bbt_point_for (BBT_Time (when.bars, when.beats, 0));
	BBTPointList::const_iterator start (wi);
	double tick_frames = 0;

	assert (wi != _map->end());

	/* compute how much rounding we did because of non-zero ticks */

	if (when.ticks != 0) {
		tick_frames = (*wi).meter->frames_per_division (*(*wi).tempo, _frame_rate) * (when.ticks/BBT_Time::ticks_per_bar_division);
	}
	
	uint32_t bars = 0;
	uint32_t beats = 0;

	while (wi != _map->end() && bars < bbt.bars) {
		++wi;
		if ((*wi).is_bar()) {
			++bars;
		}
	}
	assert (wi != _map->end());

	while (wi != _map->end() && beats < bbt.beats) {
		++wi;
		++beats;
	}
	assert (wi != _map->end());

	/* add any additional frames related to ticks in the added value */

	if (bbt.ticks != 0) {
		tick_frames += (*wi).meter->frames_per_division (*(*wi).tempo, _frame_rate) * (bbt.ticks/BBT_Time::ticks_per_bar_division);
	}

	return ((*wi).frame - (*start).frame) + llrint (tick_frames);
}

framepos_t
TempoMap::round_to_bar (framepos_t fr, int dir)
{
	return round_to_type (fr, dir, Bar);
}

framepos_t
TempoMap::round_to_beat (framepos_t fr, int dir)
{
	return round_to_type (fr, dir, Beat);
}

framepos_t
TempoMap::round_to_beat_subdivision (framepos_t fr, int sub_num, int dir)
{
	Glib::RWLock::ReaderLock lm (map_lock);
	BBTPointList::const_iterator i = bbt_before_or_at (fr);
	BBT_Time the_beat;
	uint32_t ticks_one_subdivisions_worth;
	uint32_t difference;

	bbt_time_unlocked (fr, the_beat, i);

	DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("round %1 to nearest 1/%2 beat, before-or-at = %3 @ %4|%5 precise = %6\n",
						     fr, sub_num, (*i).frame, (*i).bar, (*i).beat, the_beat));

	ticks_one_subdivisions_worth = (uint32_t)BBT_Time::ticks_per_bar_division / sub_num;

	if (dir > 0) {

		/* round to next (even if we're on a subdivision */

		uint32_t mod = the_beat.ticks % ticks_one_subdivisions_worth;

		if (mod == 0) {
			/* right on the subdivision, so the difference is just the subdivision ticks */
			the_beat.ticks += ticks_one_subdivisions_worth;

		} else {
			/* not on subdivision, compute distance to next subdivision */

			the_beat.ticks += ticks_one_subdivisions_worth - mod;
		}

		if (the_beat.ticks > BBT_Time::ticks_per_bar_division) {
			assert (i != _map->end());
			++i;
			assert (i != _map->end());
			the_beat.ticks -= BBT_Time::ticks_per_bar_division;
		} 


	} else if (dir < 0) {

		/* round to previous (even if we're on a subdivision) */

		uint32_t mod = the_beat.ticks % ticks_one_subdivisions_worth;

		if (mod == 0) {
			/* right on the subdivision, so the difference is just the subdivision ticks */
			difference = ticks_one_subdivisions_worth;
		} else {
			/* not on subdivision, compute distance to previous subdivision, which
			   is just the modulus.
			*/

			difference = mod;
		}

		if (the_beat.ticks < difference) {
			if (i == _map->begin()) {
				/* can't go backwards from wherever pos is, so just return it */
				return fr;
			}
			--i;
			the_beat.ticks = BBT_Time::ticks_per_bar_division - the_beat.ticks;
		} else {
			the_beat.ticks -= difference;
		}

	} else {
		/* round to nearest */

		double rem;

		/* compute the distance to the previous and next subdivision */
		
		if ((rem = fmod ((double) the_beat.ticks, (double) ticks_one_subdivisions_worth)) > ticks_one_subdivisions_worth/2.0) {
			
			/* closer to the next subdivision, so shift forward */

			the_beat.ticks = lrint (the_beat.ticks + (ticks_one_subdivisions_worth - rem));

			DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved forward to %1\n", the_beat.ticks));

			if (the_beat.ticks > BBT_Time::ticks_per_bar_division) {
				assert (i != _map->end());
				++i;
				assert (i != _map->end());
				the_beat.ticks -= BBT_Time::ticks_per_bar_division;
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("fold beat to %1\n", the_beat));
			} 

		} else if (rem > 0) {
			
			/* closer to previous subdivision, so shift backward */

			if (rem > the_beat.ticks) {
				if (i == _map->begin()) {
					/* can't go backwards past zero, so ... */
					return 0;
				}
				/* step back to previous beat */
				--i;
				the_beat.ticks = lrint (BBT_Time::ticks_per_bar_division - rem);
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("step back beat to %1\n", the_beat));
			} else {
				the_beat.ticks = lrint (the_beat.ticks - rem);
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved backward to %1\n", the_beat.ticks));
			}
		} else {
			/* on the subdivision, do nothing */
		}
	}

	return (*i).frame + (the_beat.ticks/BBT_Time::ticks_per_bar_division) * 
		(*i).meter->frames_per_division (*((*i).tempo), _frame_rate);
}

framepos_t
TempoMap::round_to_type (framepos_t frame, int dir, BBTPointType type)
{
	Glib::RWLock::ReaderLock lm (map_lock);
	BBTPointList::const_iterator fi;

	if (dir > 0) {
		fi = bbt_after_or_at (frame);
	} else {
		fi = bbt_before_or_at (frame);
	}

	assert (fi != _map->end());

	DEBUG_TRACE(DEBUG::SnapBBT, string_compose ("round from %1 (%3|%4 @ %5) to bars in direction %2\n", frame, dir, (*fi).bar, (*fi).beat, (*fi).frame));
		
	switch (type) {
	case Bar:
		if (dir < 0) {
			/* find bar previous to 'frame' */

			if ((*fi).is_bar() && (*fi).frame == frame) {
				--fi;
			}

			while (!(*fi).is_bar()) {
				if (fi == _map->begin()) {
					break;
				}
				fi--;
			}
			DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("rounded to bar: map iter at %1|%2 %3, return\n", 
								     (*fi).bar, (*fi).beat, (*fi).frame));
			return (*fi).frame;

		} else if (dir > 0) {

			/* find bar following 'frame' */

			if ((*fi).is_bar() && (*fi).frame == frame) {
				++fi;
			}

			while (!(*fi).is_bar()) {
				fi++;
				if (fi == _map->end()) {
					--fi;
					break;
				}
			}

			DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("rounded to bar: map iter at %1|%2 %3, return\n", 
								     (*fi).bar, (*fi).beat, (*fi).frame));
			return (*fi).frame;

		} else {
			
			/* true rounding: find nearest bar */

			BBTPointList::const_iterator prev = fi;
			BBTPointList::const_iterator next = fi;

			if ((*fi).frame == frame) {
				return frame;
			}

			while ((*prev).beat != 1) {
				if (prev == _map->begin()) {
					break;
				}
				prev--;
			}

			while ((*next).beat != 1) {
				next++;
				if (next == _map->end()) {
					--next;
					break;
				}
			}

			if ((frame - (*prev).frame) < ((*next).frame - frame)) {
				return (*prev).frame;
			} else {
				return (*next).frame;
			}
			
		}

		break;

	case Beat:
		if (dir < 0) {
			if ((*fi).frame >= frame) {
				DEBUG_TRACE (DEBUG::SnapBBT, "requested frame is on beat, step back\n");
				--fi;
			}
			DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("rounded to beat: map iter at %1|%2 %3, return\n", 
								     (*fi).bar, (*fi).beat, (*fi).frame));
			return (*fi).frame;
		} else if (dir > 0) {
			if ((*fi).frame <= frame) {
				DEBUG_TRACE (DEBUG::SnapBBT, "requested frame is on beat, step forward\n");
				++fi;
			}
			DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("rounded to beat: map iter at %1|%2 %3, return\n", 
								     (*fi).bar, (*fi).beat, (*fi).frame));
			return (*fi).frame;
		} else {
			/* find beat nearest to frame */
			if ((*fi).frame == frame) {
				return frame;
			}

			BBTPointList::const_iterator prev = fi;
			BBTPointList::const_iterator next = fi;
			--prev;
			++next;
			
			if ((frame - (*prev).frame) < ((*next).frame - frame)) {
				return (*prev).frame;
			} else {
				return (*next).frame;
			}
		}
		break;
	}

	/* NOTREACHED */
	assert (false);
	return 0;
}

void
TempoMap::map (TempoMap::BBTPointList::const_iterator& begin, 
	       TempoMap::BBTPointList::const_iterator& end, 
	       framepos_t lower, framepos_t upper) 
{
	require_map_to (upper);
	begin = lower_bound (_map->begin(), _map->end(), lower);
	end = upper_bound (_map->begin(), _map->end(), upper);
}

const TempoSection&
TempoMap::tempo_section_at (framepos_t frame) const
{
	Glib::RWLock::ReaderLock lm (metrics_lock);
	Metrics::const_iterator i;
	TempoSection* prev = 0;

	for (i = metrics->begin(); i != metrics->end(); ++i) {
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
	}

	return *prev;
}

const Tempo&
TempoMap::tempo_at (framepos_t frame) const
{
	TempoMetric m (metric_at (frame));
	return m.tempo();
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
		Glib::RWLock::ReaderLock lm (metrics_lock);
		for (i = metrics->begin(); i != metrics->end(); ++i) {
			root->add_child_nocopy ((*i)->get_state());
		}
	}

	return *root;
}

int
TempoMap::set_state (const XMLNode& node, int /*version*/)
{
	{
		Glib::RWLock::WriterLock lm (metrics_lock);

		XMLNodeList nlist;
		XMLNodeConstIterator niter;
		Metrics old_metrics (*metrics);
		MeterSection* last_meter = 0;

		metrics->clear();

		nlist = node.children();
		
		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			XMLNode* child = *niter;

			if (child->name() == TempoSection::xml_state_node_name) {

				try {
					TempoSection* ts = new TempoSection (*child);
					metrics->push_back (ts);

					if (ts->bar_offset() < 0.0) {
						if (last_meter) {
							ts->update_bar_offset_from_bbt (*last_meter);
						} 
					}
				}

				catch (failed_constructor& err){
					error << _("Tempo map: could not set new state, restoring old one.") << endmsg;
					*metrics = old_metrics;
					break;
				}

			} else if (child->name() == MeterSection::xml_state_node_name) {

				try {
					MeterSection* ms = new MeterSection (*child);
					metrics->push_back (ms);
					last_meter = ms;
				}

				catch (failed_constructor& err) {
					error << _("Tempo map: could not set new state, restoring old one.") << endmsg;
					*metrics = old_metrics;
					break;
				}
			}
		}

		if (niter == nlist.end()) {
			MetricSectionSorter cmp;
			metrics->sort (cmp);
		}
	}

	recompute_map (true);
	PropertyChanged (PropertyChange ());

	return 0;
}

void
TempoMap::dump (std::ostream& o) const
{
	Glib::RWLock::ReaderLock lm (metrics_lock);
	const MeterSection* m;
	const TempoSection* t;

	for (Metrics::const_iterator i = metrics->begin(); i != metrics->end(); ++i) {

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
	Glib::RWLock::ReaderLock lm (metrics_lock);
	int cnt = 0;

	for (Metrics::const_iterator i = metrics->begin(); i != metrics->end(); ++i) {
		if (dynamic_cast<const TempoSection*>(*i) != 0) {
			cnt++;
		}
	}

	return cnt;
}

int
TempoMap::n_meters() const
{
	Glib::RWLock::ReaderLock lm (metrics_lock);
	int cnt = 0;

	for (Metrics::const_iterator i = metrics->begin(); i != metrics->end(); ++i) {
		if (dynamic_cast<const MeterSection*>(*i) != 0) {
			cnt++;
		}
	}

	return cnt;
}

void
TempoMap::insert_time (framepos_t where, framecnt_t amount)
{
	Glib::RWLock::WriterLock lm (metrics_lock);
	for (Metrics::iterator i = metrics->begin(); i != metrics->end(); ++i) {
		if ((*i)->frame() >= where && (*i)->movable ()) {
			(*i)->set_frame ((*i)->frame() + amount);
		}
	}

	timestamp_metrics_from_audio_time ();

	PropertyChanged (PropertyChange ());
}

/** Add some (fractional) beats to a session frame position, and return the result in frames.
 *  pos can be -ve, if required.
 */
framepos_t
TempoMap::framepos_plus_beats (framepos_t pos, Evoral::MusicalTime beats)
{
	return framepos_plus_bbt (pos, BBT_Time (beats));
}

/** Subtract some (fractional) beats to a frame position, and return the result in frames */
framepos_t
TempoMap::framepos_minus_beats (framepos_t pos, Evoral::MusicalTime beats)
{
	return framepos_minus_bbt (pos, BBT_Time (beats));
}

framepos_t
TempoMap::framepos_minus_bbt (framepos_t pos, BBT_Time op)
{
	Glib::RWLock::ReaderLock lm (map_lock);
	BBTPointList::const_iterator i;
	framecnt_t extra_frames = 0;
	bool had_bars = (op.bars != 0);

	/* start from the bar|beat right before (or at) pos */

	i = bbt_before_or_at (pos);
	
	/* we know that (*i).frame is less than or equal to pos */
	extra_frames = pos - (*i).frame;
	
	/* walk backwards */

	while (i != _map->begin() && (op.bars || op.beats)) {
		--i;

		if (had_bars) {
			if ((*i).is_bar()) {
				if (op.bars) {
					op.bars--;
				}
			}
		}

		if ((had_bars && op.bars == 0) || !had_bars) {
			/* finished counting bars, or none to count, 
			   so decrement beat count
			*/
			if (op.beats) {
				op.beats--;
			}
		}
	}
	
	/* handle ticks (assumed to be less than
	 * BBT_Time::ticks_per_bar_division, as always.
	 */

	if (op.ticks) {
		frameoffset_t tick_frames = llrint ((*i).meter->frames_per_division (*(*i).tempo, _frame_rate) * (op.ticks/BBT_Time::ticks_per_bar_division));
		framepos_t pre_tick_frames = (*i).frame + extra_frames;
		if (tick_frames < pre_tick_frames) {
			return pre_tick_frames - tick_frames;
		} 
		return 0;
	} else {
		return (*i).frame + extra_frames;
	}
}

/** Add the BBT interval op to pos and return the result */
framepos_t
TempoMap::framepos_plus_bbt (framepos_t pos, BBT_Time op)
{
	Glib::RWLock::ReaderLock lm (map_lock);
	BBT_Time op_copy (op);
	int additional_minutes = 1;
	BBTPointList::const_iterator i;
	framecnt_t backup_frames = 0;
	bool had_bars = (op.bars != 0);
		
	while (true) {

		i = bbt_before_or_at (pos);

		op = op_copy;

		/* we know that (*i).frame is before or equal to pos */
		backup_frames = pos - (*i).frame;

		while (i != _map->end() && (op.bars || op.beats)) {

			++i;

			if (had_bars) {
				if ((*i).is_bar()) {
					if (op.bars) {
						op.bars--;
					}
				}
			}
			
			if ((had_bars && op.bars == 0) || !had_bars) {
				/* finished counting bars, or none to count, 
				   so decrement beat count
				*/

				if (op.beats) {
					op.beats--;
				}
			}
		}
		
		if (i != _map->end()) {
			break;
		}

		/* we hit the end of the map before finish the bbt walk.
		 */

		require_map_to (pos + (_frame_rate * 60 * additional_minutes));
		additional_minutes *= 2;

		/* go back and try again */
		warning << "reached end of map with op now at " << op << " end = " 
			<< _map->back().frame << ' ' << _map->back().bar << '|' << _map->back().beat << ", trying to walk " 
			<< op_copy << " ... retry" 
			<< endmsg;
	}
	
	if (op.ticks) {
		return (*i).frame - backup_frames + 
			llrint ((*i).meter->frames_per_division (*(*i).tempo, _frame_rate) * (op.ticks/BBT_Time::ticks_per_bar_division));
	} else {
		return (*i).frame - backup_frames;
	}
}

/** Count the number of beats that are equivalent to distance when going forward,
    starting at pos.
*/
Evoral::MusicalTime
TempoMap::framewalk_to_beats (framepos_t pos, framecnt_t distance)
{
	Glib::RWLock::ReaderLock lm (map_lock);
	BBTPointList::const_iterator i = bbt_after_or_at (pos);
	Evoral::MusicalTime beats = 0;
	framepos_t end = pos + distance;

	require_map_to (end);

	/* if our starting BBTPoint is after pos, add a fractional beat
	   to represent that distance.
	*/

	if ((*i).frame != pos) {
		beats += ((*i).frame - pos) / (*i).meter->frames_per_division (*(*i).tempo, _frame_rate);
	}

	while (i != _map->end() && (*i).frame < end) {
		++i;
		beats++;
	}
	assert (i != _map->end());
	
	/* if our ending BBTPoint is after the end, subtract a fractional beat
	   to represent that distance.
	*/

	if ((*i).frame > end) {
		beats -= ((*i).frame - end) / (*i).meter->frames_per_division (*(*i).tempo, _frame_rate);
	}

	return beats;
}

TempoMap::BBTPointList::const_iterator
TempoMap::bbt_before_or_at (framepos_t pos)
{
	BBTPointList::const_iterator i;

	require_map_to (pos);
	{
		Glib::RWLock::ReaderLock lm (map_lock);
		i = lower_bound (_map->begin(), _map->end(), pos);
		assert (i != _map->end());
		if ((*i).frame > pos) {
			assert (i != _map->begin());
			--i;
		}
	}
	return i;
}

TempoMap::BBTPointList::const_iterator
TempoMap::bbt_after_or_at (framepos_t pos)
{
	BBTPointList::const_iterator i;

	require_map_to (pos);
	{
		Glib::RWLock::ReaderLock lm (map_lock);
		i = upper_bound (_map->begin(), _map->end(), pos);
		assert (i != _map->end());
	}
	return i;
}

struct bbtcmp {
    bool operator() (const BBT_Time& a, const BBT_Time& b) {
	    return a < b;
    }
};

TempoMap::BBTPointList::const_iterator
TempoMap::bbt_point_for (const BBT_Time& bbt)
{
	BBTPointList::const_iterator i;
	bbtcmp cmp;
	int additional_minutes = 1;

	while (1) {
		{
			Glib::RWLock::ReaderLock lm (map_lock);
			if (!_map->empty() && _map->back().bar >= (bbt.bars + 1)) {
				break;
			}
		}
		/* add some more distance, using bigger steps each time */
		require_map_to (_map->back().frame + (_frame_rate * 60 * additional_minutes));
		additional_minutes *= 2;
	}

	{
		Glib::RWLock::ReaderLock lm (map_lock);
		i = lower_bound (_map->begin(), _map->end(), bbt, cmp);
		assert (i != _map->end());
	}

	return i;
}


/** Compare the time of this with that of another MetricSection.
 *  @param with_bbt True to compare using start(), false to use frame().
 *  @return -1 for less than, 0 for equal, 1 for greater than.
 */

int
MetricSection::compare (const MetricSection& other) const
{
	if (start() == other.start()) {
		return 0;
	} else if (start() < other.start()) {
		return -1;
	} else {
		return 1;
	}

	/* NOTREACHED */
	return 0;
}

bool
MetricSection::operator== (const MetricSection& other) const
{
	return compare (other) == 0;
}

bool
MetricSection::operator!= (const MetricSection& other) const
{
	return compare (other) != 0;
}

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
		o << *((Tempo*) ts);
	} else if ((ms = dynamic_cast<const MeterSection*> (&section)) != 0) {
		o << *((Meter*) ms);
	}

	return o;
}
