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
	_bar_offset = ((start().beats - 1) * BBT_Time::ticks_per_beat + start().ticks) / 
		(m.divisions_per_bar() * BBT_Time::ticks_per_beat);

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
	_frame_rate = fr;
	BBT_Time start;

	start.bars = 1;
	start.beats = 1;
	start.ticks = 0;

	TempoSection *t = new TempoSection (start, _default_tempo.beats_per_minute(), _default_tempo.note_type());
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
		Glib::RWLock::WriterLock lm (lock);
		Metrics::iterator i;

		for (i = metrics.begin(); i != metrics.end(); ++i) {
			if (dynamic_cast<TempoSection*> (*i) != 0) {
				if (tempo.frame() == (*i)->frame()) {
					if ((*i)->movable()) {
						metrics.erase (i);
						removed = true;
						break;
					}
				}
			}
		}

		if (removed && complete_operation) {
			recompute_map (false);
		}
	}

	if (removed && complete_operation) {
		PropertyChanged (PropertyChange ());
	}
}

void
TempoMap::remove_meter (const MeterSection& tempo, bool complete_operation)
{
	bool removed = false;

	{
		Glib::RWLock::WriterLock lm (lock);
		Metrics::iterator i;

		for (i = metrics.begin(); i != metrics.end(); ++i) {
			if (dynamic_cast<MeterSection*> (*i) != 0) {
				if (tempo.frame() == (*i)->frame()) {
					if ((*i)->movable()) {
						metrics.erase (i);
						removed = true;
						break;
					}
				}
			}
		}

		if (removed && complete_operation) {
			recompute_map (true);
		}
	}

	if (removed && complete_operation) {
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
TempoMap::replace_tempo (const TempoSection& ts, const Tempo& tempo, const BBT_Time& where)
{
	const TempoSection& first (first_tempo());

	if (ts.start() != first.start()) {
		remove_tempo (ts, false);
		add_tempo (tempo, where);
	} else {
		{
			Glib::RWLock::WriterLock lm (lock);
			/* cannot move the first tempo section */
			*((Tempo*)&first) = tempo;
			recompute_map (false);
		}
	}

	PropertyChanged (PropertyChange ());
}

void
TempoMap::add_tempo (const Tempo& tempo, BBT_Time where)
{
	{
		Glib::RWLock::WriterLock lm (lock);

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

		recompute_map (false);
	}


	PropertyChanged (PropertyChange ());
}

void
TempoMap::replace_meter (const MeterSection& ms, const Meter& meter, const BBT_Time& where)
{
	const MeterSection& first (first_meter());

	if (ms.start() != first.start()) {
		remove_meter (ms, false);
		add_meter (meter, where);
	} else {
		{
			Glib::RWLock::WriterLock lm (lock);
			/* cannot move the first meter section */
			*((Meter*)&first) = meter;
			recompute_map (true);
		}
	}

	PropertyChanged (PropertyChange ());
}

void
TempoMap::add_meter (const Meter& meter, BBT_Time where)
{
	{
		Glib::RWLock::WriterLock lm (lock);

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
		recompute_map (true);
	}

	
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

	for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
			{ 
				Glib::RWLock::WriterLock lm (lock);
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
		Glib::RWLock::WriterLock lm (lock);
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
	/*NOTREACHED*/
	return *m;
}

const TempoSection&
TempoMap::first_tempo () const
{
	const TempoSection *t = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if ((t = dynamic_cast<const TempoSection *> (*i)) != 0) {
			return *t;
		}
	}

	fatal << _("programming error: no tempo section in tempo map!") << endmsg;
	/*NOTREACHED*/
	return *t;
}

void
TempoMap::require_map_to (framepos_t pos)
{
	Glib::RWLock::WriterLock lm (lock);

	if (_map.empty() || _map.back().frame < pos) {
		extend_map (pos);
	}
}

void
TempoMap::require_map_to (const BBT_Time& bbt)
{
	Glib::RWLock::WriterLock lm (lock);

	/* since we have no idea where BBT is if its off the map, see the last
	 * point in the map is past BBT, and if not add an arbitrary amount of
	 * time until it is.
	 */

	int additional_minutes = 1;
	
	while (1) {
		if (!_map.empty() && _map.back().bar >= (bbt.bars + 1)) {
			break;
		}
		/* add some more distance, using bigger steps each time */
		extend_map (_map.back().frame + (_frame_rate * 60 * additional_minutes));
		additional_minutes *= 2;
	}
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
		if (!_map.empty ()) {
			/* never allow the map to be shortened */
			end = max (end, _map.back().frame);
		}
	}

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("recomputing tempo map, zero to %1\n", end));

	for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {
		MeterSection* ms;

		if ((ms = dynamic_cast<MeterSection *> (*i)) != 0) {
			meter = ms;
			break;
		}
	}

	for (Metrics::iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* ts;

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
				/*NOTREACHED*/
			}
		}
	}

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("start with meter = %1 tempo = %2\n", *((Meter*)meter), *((Tempo*)tempo)));

	next_metric = metrics.begin();
	++next_metric; // skip meter (or tempo)
	++next_metric; // skip tempo (or meter)

	_map.clear ();

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("Add first bar at 1|1 @ %2\n", current.bars, current_frame));
	_map.push_back (BBTPoint (*meter, *tempo,(framepos_t) llrint(current_frame), 1, 1));

	if (end == 0) {
		/* silly call from Session::process() during startup
		 */
		return;
	}

	_extend_map (tempo, meter, next_metric, current, current_frame, end);
}

void
TempoMap::extend_map (framepos_t end)
{
	/* CALLER MUST HOLD WRITE LOCK */

	if (_map.empty()) {
		recompute_map (false, end);
		return;
	}

	BBTPointList::const_iterator i = _map.end();	
	Metrics::iterator next_metric;

	--i;

	BBT_Time last_metric_start;

	if ((*i).tempo->frame() > (*i).meter->frame()) {
		last_metric_start = (*i).tempo->start();
	} else {
		last_metric_start = (*i).meter->start();
	}

	/* find the metric immediately after the tempo + meter sections for the
	 * last point in the map 
	 */

	for (next_metric = metrics.begin(); next_metric != metrics.end(); ++next_metric) {
		if ((*next_metric)->start() > last_metric_start) {
			break;
		}
	}

	/* we cast away const here because this is the one place where we need
	 * to actually modify the frame time of each metric section. 
	 */

	_extend_map (const_cast<TempoSection*> ((*i).tempo), 
		     const_cast<MeterSection*> ((*i).meter),
		     next_metric, BBT_Time ((*i).bar, (*i).beat, 0), (*i).frame, end);
}

void
TempoMap::_extend_map (TempoSection* tempo, MeterSection* meter, 
		       Metrics::iterator next_metric,
		       BBT_Time current, framepos_t current_frame, framepos_t end)
{
	/* CALLER MUST HOLD WRITE LOCK */

	TempoSection* ts;
	MeterSection* ms;
	double beat_frames;
	framepos_t bar_start_frame;

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("Extend map to %1 from %2 = %3\n", end, current, current_frame));

	if (current.beats == 1) {
		bar_start_frame = current_frame;
	} else {
		bar_start_frame = 0;
	}

	beat_frames = meter->frames_per_grid (*tempo,_frame_rate);

	while (current_frame < end) {

		current.beats++;
		current_frame += beat_frames;

		if (current.beats > meter->divisions_per_bar()) {
			current.bars++;
			current.beats = 1;
		}

		if (next_metric != metrics.end()) {

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
						
						double next_beat_frames = tempo->frames_per_beat (_frame_rate);					
						
						DEBUG_TRACE (DEBUG::TempoMath, string_compose ("bumped into non-beat-aligned tempo metric at %1 = %2, adjust next beat using %3\n",
											       tempo->start(), current_frame, tempo->bar_offset()));
						
						/* back up to previous beat */
						current_frame -= beat_frames;

						/* set tempo section location
						 * based on offset from last
						 * bar start 
						 */
						tempo->set_frame (bar_start_frame + 
								  llrint ((ts->bar_offset() * meter->divisions_per_bar() * beat_frames)));
						
						/* advance to the location of
						 * the new (adjusted) beat. do
						 * this by figuring out the
						 * offset within the beat that
						 * would have been there
						 * without the tempo
						 * change. then stretch the
						 * beat accordingly.
						 */

						double offset_within_old_beat = (tempo->frame() - current_frame) / beat_frames;

						current_frame += (offset_within_old_beat * beat_frames) + ((1.0 - offset_within_old_beat) * next_beat_frames);

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
				
				beat_frames = meter->frames_per_grid (*tempo, _frame_rate);
				
				DEBUG_TRACE (DEBUG::TempoMath, string_compose ("New metric with beat frames = %1 dpb %2 meter %3 tempo %4\n", 
									       beat_frames, meter->divisions_per_bar(), *((Meter*)meter), *((Tempo*)tempo)));
			
				++next_metric;

				if (next_metric != metrics.end() && ((*next_metric)->start() == current)) {
					/* same position so go back and set this one up before advancing
					*/
					goto set_metrics;
				}
			}
		} 

		if (current.beats == 1) {
			DEBUG_TRACE (DEBUG::TempoMath, string_compose ("Add Bar at %1|1 @ %2\n", current.bars, current_frame));
			_map.push_back (BBTPoint (*meter, *tempo,(framepos_t) llrint(current_frame), current.bars, 1));
			bar_start_frame = current_frame;
		} else {
			DEBUG_TRACE (DEBUG::TempoMath, string_compose ("Add Beat at %1|%2 @ %3\n", current.bars, current.beats, current_frame));
			_map.push_back (BBTPoint (*meter, *tempo, (framepos_t) llrint(current_frame), current.bars, current.beats));
		}

		if (next_metric == metrics.end()) {
			/* no more metrics - we've timestamped them all, stop here */
			if (end == max_framepos) {
				DEBUG_TRACE (DEBUG::TempoMath, string_compose ("stop extending map now that we've reach the end @ %1|%2 = %3\n",
									       current.bars, current.beats, current_frame));
				break;
			}
		}
	}
}

TempoMetric
TempoMap::metric_at (framepos_t frame) const
{
	Glib::RWLock::ReaderLock lm (lock);
	TempoMetric m (first_meter(), first_tempo());
	const Meter* meter;
	const Tempo* tempo;

	/* at this point, we are *guaranteed* to have m.meter and m.tempo pointing
	   at something, because we insert the default tempo and meter during
	   TempoMap construction.

	   now see if we can find better candidates.
	*/

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {

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
	
	return m;
}

TempoMetric
TempoMap::metric_at (BBT_Time bbt) const
{
	Glib::RWLock::ReaderLock lm (lock);
	TempoMetric m (first_meter(), first_tempo());
	const Meter* meter;
	const Tempo* tempo;

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
	require_map_to (frame);

	Glib::RWLock::ReaderLock lm (lock);

	if (frame < 0) {
		bbt.bars = 1;
		bbt.beats = 1;
		bbt.ticks = 0;
		warning << string_compose (_("tempo map asked for BBT time at frame %1\n"), frame) << endmsg;
		return;
	}

	return bbt_time (frame, bbt, bbt_before_or_at (frame));
}

void
TempoMap::bbt_time_rt (framepos_t frame, BBT_Time& bbt)
{
	Glib::RWLock::ReaderLock lm (lock, Glib::TRY_LOCK);

	if (!lm.locked()) {
		throw std::logic_error ("TempoMap::bbt_time_rt() could not lock tempo map");
	}
	
	if (_map.empty() || _map.back().frame < frame) {
		throw std::logic_error (string_compose ("map not long enough to reach %1", frame));
	}

	return bbt_time (frame, bbt, bbt_before_or_at (frame));
}

void
TempoMap::bbt_time (framepos_t frame, BBT_Time& bbt, const BBTPointList::const_iterator& i)
{
	/* CALLER MUST HOLD READ LOCK */

	bbt.bars = (*i).bar;
	bbt.beats = (*i).beat;

	if ((*i).frame == frame) {
		bbt.ticks = 0;
	} else {
		bbt.ticks = llrint (((frame - (*i).frame) / (*i).tempo->frames_per_beat(_frame_rate)) *
				    BBT_Time::ticks_per_beat);
	}
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

	require_map_to (bbt);

	Glib::RWLock::ReaderLock lm (lock);

	BBTPointList::const_iterator s = bbt_before_or_at (BBT_Time (1, 1, 0));
	BBTPointList::const_iterator e = bbt_before_or_at (BBT_Time (bbt.bars, bbt.beats, 0));

	if (bbt.ticks != 0) {
		return ((*e).frame - (*s).frame) + 
			llrint ((*e).tempo->frames_per_beat (_frame_rate) * (bbt.ticks/BBT_Time::ticks_per_beat));
	} else {
		return ((*e).frame - (*s).frame);
	}
}

framecnt_t
TempoMap::bbt_duration_at (framepos_t pos, const BBT_Time& bbt, int dir)
{
	BBT_Time when;
	bbt_time (pos, when);
	
	Glib::RWLock::ReaderLock lm (lock);
	return bbt_duration_at_unlocked (when, bbt, dir);
}

framecnt_t
TempoMap::bbt_duration_at_unlocked (const BBT_Time& when, const BBT_Time& bbt, int /*dir*/) 
{
	if (bbt.bars == 0 && bbt.beats == 0 && bbt.ticks == 0) {
		return 0;
	}

	/* round back to the previous precise beat */
	BBTPointList::const_iterator wi = bbt_before_or_at (BBT_Time (when.bars, when.beats, 0));
	BBTPointList::const_iterator start (wi);
	double tick_frames = 0;

	assert (wi != _map.end());

	/* compute how much rounding we did because of non-zero ticks */

	if (when.ticks != 0) {
		tick_frames = (*wi).tempo->frames_per_beat (_frame_rate) * (when.ticks/BBT_Time::ticks_per_beat);
	}
	
	uint32_t bars = 0;
	uint32_t beats = 0;

	while (wi != _map.end() && bars < bbt.bars) {
		++wi;
		if ((*wi).is_bar()) {
			++bars;
		}
	}
	assert (wi != _map.end());

	while (wi != _map.end() && beats < bbt.beats) {
		++wi;
		++beats;
	}
	assert (wi != _map.end());

	/* add any additional frames related to ticks in the added value */

	if (bbt.ticks != 0) {
		tick_frames += (*wi).tempo->frames_per_beat (_frame_rate) * (bbt.ticks/BBT_Time::ticks_per_beat);
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
	require_map_to (fr);

	Glib::RWLock::ReaderLock lm (lock);
	BBTPointList::const_iterator i = bbt_before_or_at (fr);
	BBT_Time the_beat;
	uint32_t ticks_one_subdivisions_worth;
	uint32_t difference;

	bbt_time (fr, the_beat, i);

	DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("round %1 to nearest 1/%2 beat, before-or-at = %3 @ %4|%5 precise = %6\n",
						     fr, sub_num, (*i).frame, (*i).bar, (*i).beat, the_beat));

	ticks_one_subdivisions_worth = (uint32_t)BBT_Time::ticks_per_beat / sub_num;

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

		if (the_beat.ticks > BBT_Time::ticks_per_beat) {
			assert (i != _map.end());
			++i;
			assert (i != _map.end());
			the_beat.ticks -= BBT_Time::ticks_per_beat;
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
			if (i == _map.begin()) {
				/* can't go backwards from wherever pos is, so just return it */
				return fr;
			}
			--i;
			the_beat.ticks = BBT_Time::ticks_per_beat - the_beat.ticks;
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

			if (the_beat.ticks > BBT_Time::ticks_per_beat) {
				assert (i != _map.end());
				++i;
				assert (i != _map.end());
				the_beat.ticks -= BBT_Time::ticks_per_beat;
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("fold beat to %1\n", the_beat));
			} 

		} else if (rem > 0) {
			
			/* closer to previous subdivision, so shift backward */

			if (rem > the_beat.ticks) {
				if (i == _map.begin()) {
					/* can't go backwards past zero, so ... */
					return 0;
				}
				/* step back to previous beat */
				--i;
				the_beat.ticks = lrint (BBT_Time::ticks_per_beat - rem);
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("step back beat to %1\n", the_beat));
			} else {
				the_beat.ticks = lrint (the_beat.ticks - rem);
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved backward to %1\n", the_beat.ticks));
			}
		} else {
			/* on the subdivision, do nothing */
		}
	}

	return (*i).frame + (the_beat.ticks/BBT_Time::ticks_per_beat) * 
		(*i).tempo->frames_per_beat (_frame_rate);
}

framepos_t
TempoMap::round_to_type (framepos_t frame, int dir, BBTPointType type)
{
	require_map_to (frame);

	Glib::RWLock::ReaderLock lm (lock);
	BBTPointList::const_iterator fi;

	if (dir > 0) {
		fi = bbt_after_or_at (frame);
	} else {
		fi = bbt_before_or_at (frame);
	}

	assert (fi != _map.end());

	DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("round from %1 (%3|%4 @ %5) to %6 in direction %2\n", frame, dir, (*fi).bar, (*fi).beat, (*fi).frame,
						     (type == Bar ? "bar" : "beat")));
		
	switch (type) {
	case Bar:
		if (dir < 0) {
			/* find bar previous to 'frame' */

			if (fi == _map.begin()) {
				return 0;
			}

			if ((*fi).is_bar() && (*fi).frame == frame) {
				--fi;
			}

			while (!(*fi).is_bar()) {
				if (fi == _map.begin()) {
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
				if (fi == _map.end()) {
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
				if (prev == _map.begin()) {
					break;
				}
				prev--;
			}

			while ((next != _map.end()) && (*next).beat != 1) {
				next++;
			}

			if ((next == _map.end()) || (frame - (*prev).frame) < ((*next).frame - frame)) {
				return (*prev).frame;
			} else {
				return (*next).frame;
			}
			
		}

		break;

	case Beat:
		if (dir < 0) {

			if (fi == _map.begin()) {
				return 0;
			}

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

			/* fi is already the beat before_or_at frame, and
			   we've just established that its not at frame, so its
			   the beat before frame.
			*/
			++next;
			
			if ((next == _map.end()) || (frame - (*prev).frame) < ((*next).frame - frame)) {
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
TempoMap::get_grid (TempoMap::BBTPointList::const_iterator& begin, 
		    TempoMap::BBTPointList::const_iterator& end, 
		    framepos_t lower, framepos_t upper) 
{
	{ 
		Glib::RWLock::WriterLock lm (lock);
		if (_map.empty() || (_map.back().frame < upper)) {
			recompute_map (false, upper);
		}
	}

	begin = lower_bound (_map.begin(), _map.end(), lower);
	end = upper_bound (_map.begin(), _map.end(), upper);
}

const TempoSection&
TempoMap::tempo_section_at (framepos_t frame) const
{
	Glib::RWLock::ReaderLock lm (lock);
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
		Glib::RWLock::ReaderLock lm (lock);
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
		Glib::RWLock::WriterLock lm (lock);

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

		recompute_map (true, -1);
	}

	PropertyChanged (PropertyChange ());

	return 0;
}

void
TempoMap::dump (std::ostream& o) const
{
	Glib::RWLock::ReaderLock lm (lock, Glib::TRY_LOCK);
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
	Glib::RWLock::ReaderLock lm (lock);
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
	Glib::RWLock::ReaderLock lm (lock);
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
		Glib::RWLock::WriterLock lm (lock);
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
			
			BBTPointList::const_iterator bi = bbt_before_or_at ((*i)->frame());
			bbt_time ((*i)->frame(), bbt, bi);
			
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
				/*NOTREACHED*/
			}
			
			prev = (*i);
		}
		
		recompute_map (true);
	}


	PropertyChanged (PropertyChange ());
}

/** Add some (fractional) beats to a session frame position, and return the result in frames.
 *  pos can be -ve, if required.
 */
framepos_t
TempoMap::framepos_plus_beats (framepos_t pos, Evoral::MusicalTime beats) const
{
	Glib::RWLock::ReaderLock lm (lock);
	Metrics::const_iterator next_tempo;
	const TempoSection* tempo = 0;

	/* Find the starting tempo metric */

	for (next_tempo = metrics.begin(); next_tempo != metrics.end(); ++next_tempo) {

		const TempoSection* t;

		if ((t = dynamic_cast<const TempoSection*>(*next_tempo)) != 0) {

			/* This is a bit of a hack, but pos could be -ve, and if it is,
			   we consider the initial metric changes (at time 0) to actually
			   be in effect at pos.
			*/

			framepos_t f = (*next_tempo)->frame ();

			if (pos < 0 && f == 0) {
				f = pos;
			}
			
			if (f > pos) {
				break;
			}
			
			tempo = t;
		}
	}

	/* We now have:

	   tempo       -> the Tempo for "pos"
	   next_tempo  -> first tempo after "pos", possibly metrics.end()
	*/

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("frame %1 plus %2 beats, start with tempo = %3 @ %4\n",
						       pos, beats, *((Tempo*)tempo), tempo->frame()));

	while (beats) {

		/* Distance to the end of this section in frames */
		framecnt_t distance_frames = (next_tempo == metrics.end() ? max_framepos : ((*next_tempo)->frame() - pos));

		/* Distance to the end in beats */
		Evoral::MusicalTime distance_beats = distance_frames / tempo->frames_per_beat (_frame_rate);

		/* Amount to subtract this time */
		double const delta = min (distance_beats, beats);

		DEBUG_TRACE (DEBUG::TempoMath, string_compose ("\tdistance to %1 = %2 (%3 beats)\n",
							       (next_tempo == metrics.end() ? max_framepos : (*next_tempo)->frame()),
							       distance_frames, distance_beats));

		/* Update */
		beats -= delta;
		pos += delta * tempo->frames_per_beat (_frame_rate);

		DEBUG_TRACE (DEBUG::TempoMath, string_compose ("\tnow at %1, %2 beats left\n", pos, beats));

		/* step forwards to next tempo section */

		if (next_tempo != metrics.end()) {

			tempo = dynamic_cast<const TempoSection*>(*next_tempo);

			DEBUG_TRACE (DEBUG::TempoMath, string_compose ("\tnew tempo = %1 @ %2 fpb = %3\n",
								       *((Tempo*)tempo), tempo->frame(),
								       tempo->frames_per_beat (_frame_rate)));

			while (next_tempo != metrics.end ()) {

				++next_tempo;
				
				if (next_tempo != metrics.end() && dynamic_cast<const TempoSection*>(*next_tempo)) {
					break;
				}
			}
		}
	}

	return pos;
}

/** Subtract some (fractional) beats from a frame position, and return the result in frames */
framepos_t
TempoMap::framepos_minus_beats (framepos_t pos, Evoral::MusicalTime beats) const
{
	Glib::RWLock::ReaderLock lm (lock);
	Metrics::const_reverse_iterator prev_tempo;
	const TempoSection* tempo = 0;

	/* Find the starting tempo metric */

	for (prev_tempo = metrics.rbegin(); prev_tempo != metrics.rend(); ++prev_tempo) {

		const TempoSection* t;

		if ((t = dynamic_cast<const TempoSection*>(*prev_tempo)) != 0) {

			/* This is a bit of a hack, but pos could be -ve, and if it is,
			   we consider the initial metric changes (at time 0) to actually
			   be in effect at pos.
			*/

			framepos_t f = (*prev_tempo)->frame ();

			if (pos < 0 && f == 0) {
				f = pos;
			}

			/* this is slightly more complex than the forward case
			   because we reach the tempo in effect at pos after
			   passing through pos (rather before, as in the
			   forward case). having done that, we then need to
			   keep going to get the previous tempo (or
			   metrics.rend())
			*/
			
			if (f <= pos) {
				if (tempo == 0) {
					/* first tempo with position at or
					   before pos
					*/
					tempo = t;
				} else if (f < pos) {
					/* some other tempo section that
					   is even earlier than 'tempo'
					*/
					break;
				}
			}
		}
	}

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("frame %1 minus %2 beats, start with tempo = %3 @ %4 prev at beg? %5\n",
						       pos, beats, *((Tempo*)tempo), tempo->frame(),
						       prev_tempo == metrics.rend()));

	/* We now have:

	   tempo       -> the Tempo for "pos"
	   prev_tempo  -> the first metric before "pos", possibly metrics.rend()
	*/

	while (beats) {
		
		/* Distance to the start of this section in frames */
		framecnt_t distance_frames = (pos - tempo->frame());

		/* Distance to the start in beats */
		Evoral::MusicalTime distance_beats = distance_frames / tempo->frames_per_beat (_frame_rate);

		/* Amount to subtract this time */
		double const sub = min (distance_beats, beats);

		DEBUG_TRACE (DEBUG::TempoMath, string_compose ("\tdistance to %1 = %2 (%3 beats)\n",
							       tempo->frame(), distance_frames, distance_beats));
		/* Update */

		beats -= sub;
		pos -= sub * tempo->frames_per_beat (_frame_rate);

		DEBUG_TRACE (DEBUG::TempoMath, string_compose ("\tnow at %1, %2 beats left, prev at end ? %3\n", pos, beats,
							       (prev_tempo == metrics.rend())));

		/* step backwards to prior TempoSection */

		if (prev_tempo != metrics.rend()) {

			tempo = dynamic_cast<const TempoSection*>(*prev_tempo);

			DEBUG_TRACE (DEBUG::TempoMath, string_compose ("\tnew tempo = %1 @ %2 fpb = %3\n",
								       *((Tempo*)tempo), tempo->frame(),
								       tempo->frames_per_beat (_frame_rate)));

			while (prev_tempo != metrics.rend ()) {

				++prev_tempo;

				if (prev_tempo != metrics.rend() && dynamic_cast<const TempoSection*>(*prev_tempo) != 0) {
					break;
				}
			}
		} else {
			pos -= llrint (beats * tempo->frames_per_beat (_frame_rate));
			beats = 0;
		}
	}

	return pos;
}

/** Add the BBT interval op to pos and return the result */
framepos_t
TempoMap::framepos_plus_bbt (framepos_t pos, BBT_Time op) const
{
	Glib::RWLock::ReaderLock lm (lock);
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
Evoral::MusicalTime
TempoMap::framewalk_to_beats (framepos_t pos, framecnt_t distance) const
{
	Glib::RWLock::ReaderLock lm (lock);
	Metrics::const_iterator next_tempo;
	const TempoSection* tempo = 0;
	framepos_t effective_pos = max (pos, (framepos_t) 0);

	/* Find the relevant initial tempo metric  */

	for (next_tempo = metrics.begin(); next_tempo != metrics.end(); ++next_tempo) {

		const TempoSection* t;

		if ((t = dynamic_cast<const TempoSection*>(*next_tempo)) != 0) {

			if ((*next_tempo)->frame() > effective_pos) {
				break;
			}

			tempo = t;
		}
	}

	/* We now have:

	   tempo -> the Tempo for "pos"
	   next_tempo -> the next tempo after "pos", possibly metrics.end()
	*/

	assert (tempo);

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("frame %1 walk by %2 frames, start with tempo = %3 @ %4\n",
						       pos, distance, *((Tempo*)tempo), tempo->frame()));
	
	Evoral::MusicalTime beats = 0;

	while (distance) {

		/* End of this section */
		framepos_t end;
		/* Distance to `end' in frames */
		framepos_t distance_to_end;

		if (next_tempo == metrics.end ()) {
			/* We can't do (end - pos) if end is max_framepos, as it will overflow if pos is -ve */
			end = max_framepos;
			distance_to_end = max_framepos;
		} else {
			end = (*next_tempo)->frame ();
			distance_to_end = end - pos;
		}

		/* Amount to subtract this time */
		double const sub = min (distance, distance_to_end);

		DEBUG_TRACE (DEBUG::TempoMath, string_compose ("to reach end at %1 (end ? %2), distance= %3 sub=%4\n", end, (next_tempo == metrics.end()),
							       distance_to_end, sub));

		/* Update */
		pos += sub;
		distance -= sub;
		assert (tempo);
		beats += sub / tempo->frames_per_beat (_frame_rate);

		DEBUG_TRACE (DEBUG::TempoMath, string_compose ("now at %1, beats = %2 distance left %3\n",
							       pos, beats, distance));

		/* Move on if there's anything to move to */

		if (next_tempo != metrics.end()) {

			tempo = dynamic_cast<const TempoSection*>(*next_tempo);

			DEBUG_TRACE (DEBUG::TempoMath, string_compose ("\tnew tempo = %1 @ %2 fpb = %3\n",
								       *((Tempo*)tempo), tempo->frame(),
								       tempo->frames_per_beat (_frame_rate)));

			while (next_tempo != metrics.end ()) {

				++next_tempo;
				
				if (next_tempo != metrics.end() && dynamic_cast<const TempoSection*>(*next_tempo)) {
					break;
				}
			}

			if (next_tempo == metrics.end()) {
				DEBUG_TRACE (DEBUG::TempoMath, "no more tempo sections\n");
			} else {
				DEBUG_TRACE (DEBUG::TempoMath, string_compose ("next tempo section is %1 @ %2\n",
									       **next_tempo, (*next_tempo)->frame()));
			}

		}
		assert (tempo);
	}

	return beats;
}

TempoMap::BBTPointList::const_iterator
TempoMap::bbt_before_or_at (framepos_t pos)
{
	/* CALLER MUST HOLD READ LOCK */

	BBTPointList::const_iterator i;

	if (pos < 0) {
		/* not really correct, but we should catch pos < 0 at a higher
		   level 
		*/
		return _map.begin();
	}

	i = lower_bound (_map.begin(), _map.end(), pos);
	assert (i != _map.end());
	if ((*i).frame > pos) {
		assert (i != _map.begin());
		--i;
	}
	return i;
}

struct bbtcmp {
    bool operator() (const BBT_Time& a, const BBT_Time& b) {
	    return a < b;
    }
};

TempoMap::BBTPointList::const_iterator
TempoMap::bbt_before_or_at (const BBT_Time& bbt)
{
	BBTPointList::const_iterator i;
	bbtcmp cmp;

	i = lower_bound (_map.begin(), _map.end(), bbt, cmp);
	assert (i != _map.end());
	if ((*i).bar > bbt.bars || (*i).beat > bbt.beats) {
		assert (i != _map.begin());
		--i;
	}
	return i;
}

TempoMap::BBTPointList::const_iterator
TempoMap::bbt_after_or_at (framepos_t pos) 
{
	/* CALLER MUST HOLD READ LOCK */

	BBTPointList::const_iterator i;

	if (_map.back().frame == pos) {
		i = _map.end();
		assert (i != _map.begin());
		--i;
		return i;
	}

	i = upper_bound (_map.begin(), _map.end(), pos);
	assert (i != _map.end());
	return i;
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
