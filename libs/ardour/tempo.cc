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
Meter::frames_per_bar (const Tempo& tempo, framecnt_t sr) const
{
	return (60.0 * sr * _divisions_per_bar) / tempo.beats_per_minute();
}

double 
Meter::frames_per_division (const Tempo& tempo, framecnt_t sr) const
{
	return (60.0 * sr) / (tempo.beats_per_minute() * _note_type/tempo.note_type());
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
	snprintf (buf, sizeof (buf), "%s", movable()?"yes":"no");
	root->add_property ("movable", buf);

	return *root;
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
}

int
TempoMap::move_metric_section (MetricSection& section, const BBT_Time& when)
{
	if (when == section.start() || !section.movable()) {
		return -1;
	}

	Glib::RWLock::WriterLock  lm (lock);
	MetricSectionSorter cmp;

	if (when.beats != 1) {

		/* position by audio frame, then recompute BBT timestamps from the audio ones */

		framepos_t frame = frame_time (when);
		// cerr << "nominal frame time = " << frame << endl;

		framepos_t prev_frame = round_to_type (frame, -1, Beat);
		framepos_t next_frame = round_to_type (frame, 1, Beat);

		// cerr << "previous beat at " << prev_frame << " next at " << next_frame << endl;

		/* use the closest beat */

		if ((frame - prev_frame) < (next_frame - frame)) {
			frame = prev_frame;
		} else {
			frame = next_frame;
		}

		// cerr << "actual frame time = " << frame << endl;
		section.set_frame (frame);
		// cerr << "frame time = " << section.frame() << endl;
		timestamp_metrics (false);
		// cerr << "new BBT time = " << section.start() << endl;
		metrics->sort (cmp);

	} else {

		/* positioned at bar start already, so just put it there */

		section.set_start (when);
		metrics->sort (cmp);
		timestamp_metrics (true);
	}


	return 0;
}

void
TempoMap::move_tempo (TempoSection& tempo, const BBT_Time& when)
{
	if (move_metric_section (tempo, when) == 0) {
		PropertyChanged (PropertyChange ());
	}
}

void
TempoMap::move_meter (MeterSection& meter, const BBT_Time& when)
{
	if (move_metric_section (meter, when) == 0) {
		PropertyChanged (PropertyChange ());
	}
}

void
TempoMap::remove_tempo (const TempoSection& tempo)
{
	bool removed = false;

	{
		Glib::RWLock::WriterLock lm (lock);
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

	if (removed) {
		PropertyChanged (PropertyChange ());
	}
}

void
TempoMap::remove_meter (const MeterSection& tempo)
{
	bool removed = false;

	{
		Glib::RWLock::WriterLock lm (lock);
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

	if (removed) {
		timestamp_metrics (true);
		PropertyChanged (PropertyChange ());
	}
}

void
TempoMap::do_insert (MetricSection* section, bool with_bbt)
{
	assert (section->start().ticks == 0);

	/* we only allow new meters to be inserted on beat 1 of an existing
	 * measure. 
	 */

	if (dynamic_cast<MeterSection*>(section) && 
	    (section->start().beats != 1 || section->start().ticks != 0)) {

		BBT_Time corrected = section->start();
		corrected.beats = 1;
		corrected.ticks = 0;
		
		warning << string_compose (_("Meter changes can only be positioned on the first beat of a bar. Moving from %1 to %2"),
					   section->start(), corrected) << endmsg;

		section->set_start (corrected);
	}

	Metrics::iterator i;

	/* Look for any existing MetricSection that is of the same type and
	   at the same time as the new one, and remove it before adding
	   the new one.
	*/

	Metrics::iterator to_remove = metrics->end ();

	for (i = metrics->begin(); i != metrics->end(); ++i) {

		int const c = (*i)->compare (section, with_bbt);

		if (c < 0) {
			/* this section is before the one to be added; go back round */
			continue;
		} else if (c > 0) {
			/* this section is after the one to be added; there can't be any at the same time */
			break;
		}

		/* hacky comparison of type */
		bool const a = dynamic_cast<TempoSection*> (*i) != 0;
		bool const b = dynamic_cast<TempoSection*> (section) != 0;

		if (a == b) {
			to_remove = i;
			break;
		}
	}

	if (to_remove != metrics->end()) {
		/* remove the MetricSection at the same time as the one we are about to add */
		metrics->erase (to_remove);
	}

	/* Add the given MetricSection */

	for (i = metrics->begin(); i != metrics->end(); ++i) {

		if ((*i)->compare (section, with_bbt) < 0) {
			continue;
		}

		metrics->insert (i, section);
		break;
	}

	if (i == metrics->end()) {
		metrics->insert (metrics->end(), section);
	}

	timestamp_metrics (with_bbt);
}

void
TempoMap::add_tempo (const Tempo& tempo, BBT_Time where)
{
	{
		Glib::RWLock::WriterLock lm (lock);

		/* new tempos always start on a beat */
		where.ticks = 0;

		do_insert (new TempoSection (where, tempo.beats_per_minute(), tempo.note_type()), true);
	}

	PropertyChanged (PropertyChange ());
}

void
TempoMap::add_tempo (const Tempo& tempo, framepos_t where)
{
	{
		Glib::RWLock::WriterLock lm (lock);
		do_insert (new TempoSection (where, tempo.beats_per_minute(), tempo.note_type()), false);
	}

	PropertyChanged (PropertyChange ());
}

void
TempoMap::replace_tempo (TempoSection& existing, const Tempo& replacement)
{
	bool replaced = false;

	{
		Glib::RWLock::WriterLock lm (lock);
		Metrics::iterator i;

		for (i = metrics->begin(); i != metrics->end(); ++i) {
			TempoSection *ts;

			if ((ts = dynamic_cast<TempoSection*>(*i)) != 0 && ts == &existing) {

				 *((Tempo *) ts) = replacement;

				replaced = true;
				timestamp_metrics (true);

				break;
			}
		}
	}

	if (replaced) {
		PropertyChanged (PropertyChange ());
	}
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

		do_insert (new MeterSection (where, meter.divisions_per_bar(), meter.note_divisor()), true);
	}

	PropertyChanged (PropertyChange ());
}

void
TempoMap::add_meter (const Meter& meter, framepos_t where)
{
	{
		Glib::RWLock::WriterLock lm (lock);
		do_insert (new MeterSection (where, meter.divisions_per_bar(), meter.note_divisor()), false);
	}

	PropertyChanged (PropertyChange ());
}

void
TempoMap::replace_meter (MeterSection& existing, const Meter& replacement)
{
	bool replaced = false;

	{
		Glib::RWLock::WriterLock lm (lock);
		Metrics::iterator i;

		for (i = metrics->begin(); i != metrics->end(); ++i) {
			MeterSection *ms;
			if ((ms = dynamic_cast<MeterSection*>(*i)) != 0 && ms == &existing) {

				*((Meter*) ms) = replacement;

				replaced = true;
				timestamp_metrics (true);
				break;
			}
		}
	}

	if (replaced) {
		PropertyChanged (PropertyChange ());
	}
}

void
TempoMap::change_initial_tempo (double beats_per_minute, double note_type)
{
	Tempo newtempo (beats_per_minute, note_type);
	TempoSection* t;

	for (Metrics::iterator i = metrics->begin(); i != metrics->end(); ++i) {
		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
			*((Tempo*) t) = newtempo;
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

	*((Tempo*)prev) = newtempo;
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
TempoMap::timestamp_metrics (bool use_bbt)
{
	Metrics::iterator i;
	const Meter* meter;
	const Tempo* tempo;
	Meter *m;
	Tempo *t;

	meter = &first_meter ();
	tempo = &first_tempo ();

	if (use_bbt) {
		
		// cerr << "\n\n\n ######################\nTIMESTAMP via BBT ##############\n" << endl;

		framepos_t current = 0;
		framepos_t section_frames;
		BBT_Time start;
		BBT_Time end;

		for (i = metrics->begin(); i != metrics->end(); ++i) {

			end = (*i)->start();

			section_frames = count_frames_between_metrics (*meter, *tempo, start, end);

			current += section_frames;

			start = end;

			(*i)->set_frame (current);

			if ((t = dynamic_cast<TempoSection*>(*i)) != 0) {
				tempo = t;
			} else if ((m = dynamic_cast<MeterSection*>(*i)) != 0) {
				meter = m;
			} else {
				fatal << _("programming error: unhandled MetricSection type") << endmsg;
				/*NOTREACHED*/
			}
		}

	} else {

		// cerr << "\n\n\n ######################\nTIMESTAMP via AUDIO ##############\n" << endl;

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

			bbt_time_with_metric ((*i)->frame(), bbt, metric);
			
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
	}

	// dump (cerr);
	// cerr << "###############################################\n\n\n" << endl;

}

TempoMetric
TempoMap::metric_at (framepos_t frame) const
{
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

	// cerr << "for framepos " << frame << " returning " << m.meter() << " @ " << m.tempo() << endl;
	return m;
}

TempoMetric
TempoMap::metric_at (BBT_Time bbt) const
{
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
TempoMap::bbt_time (framepos_t frame, BBT_Time& bbt) const
{
	{
		Glib::RWLock::ReaderLock lm (lock);
		bbt_time_unlocked (frame, bbt);
	}
}

void
TempoMap::bbt_time_unlocked (framepos_t frame, BBT_Time& bbt) const
{
	bbt_time_with_metric (frame, bbt, metric_at (frame));
}

void
TempoMap::bbt_time_with_metric (framepos_t frame, BBT_Time& bbt, const TempoMetric& metric) const
{
	framecnt_t frame_diff;

	const double divisions_per_bar = metric.meter().divisions_per_bar();
	const double ticks_per_frame = metric.meter().frames_per_division (metric.tempo(),_frame_rate) / BBT_Time::ticks_per_beat;

	// cerr << "*** Compute BBT time for " << frame 
	// <<  " from metric at " << metric.frame() << " tempo = " << metric.tempo().beats_per_minute () << " meter " 
	// << metric.meter().divisions_per_bar() << '/' << metric.meter().note_divisor() 
	// << endl; 

	/* now compute how far beyond that point we actually are. */

	frame_diff = frame - metric.frame();

	bbt.ticks = metric.start().ticks + (uint32_t)round((double)frame_diff / ticks_per_frame);
	uint32_t xtra_beats = bbt.ticks / (uint32_t)BBT_Time::ticks_per_beat;
	bbt.ticks %= (uint32_t)BBT_Time::ticks_per_beat;

	bbt.beats = metric.start().beats + xtra_beats - 1; // correction for 1-based counting, see below for matching operation.
	bbt.bars = metric.start().bars + (uint32_t)floor((double)bbt.beats / divisions_per_bar);
	bbt.beats = (uint32_t)fmod((double)bbt.beats, divisions_per_bar);

	/* if we have a fractional number of beats per bar, we see if
	   we're in the last beat (the fractional one).  if so, we
	   round ticks appropriately and bump to the next bar. 
	*/
	double beat_fraction = divisions_per_bar - floor(divisions_per_bar);

	/* XXX one problem here is that I'm not sure how to handle
	   fractional beats that don't evenly divide ticks_per_beat.
	   If they aren't handled consistently, I would guess we'll
	   continue to have strange discrepancies occuring.  Perhaps
	   this will also behave badly in the case of meters like
	   0.1/4, but I can't be bothered to test that.
	*/
	uint32_t ticks_on_last_beat = (uint32_t)floor(BBT_Time::ticks_per_beat * beat_fraction);

	if (bbt.beats > (uint32_t)floor(divisions_per_bar) && bbt.ticks >= ticks_on_last_beat) {
		bbt.ticks -= ticks_on_last_beat;
		bbt.beats = 0;
		bbt.bars++;
	}

	bbt.beats++; // correction for 1-based counting, see above for matching operation.

	// cerr << "-----\t RETURN " << bbt << endl;
}

framecnt_t
TempoMap::count_frames_between (const BBT_Time& start, const BBT_Time& end) const
{
	/* for this to work with fractional measure types, start and end have to be
	   "legal" BBT types, that means that the beats and ticks should be inside
	   a bar
	*/

	framecnt_t frames = 0;
	framepos_t start_frame = 0;
	framepos_t end_frame = 0;

	TempoMetric m = metric_at (start);

	uint32_t bar_offset = start.bars - m.start().bars;

	double  beat_offset = bar_offset*m.meter().divisions_per_bar() - (m.start().beats-1) + (start.beats -1)
		+ start.ticks/BBT_Time::ticks_per_beat;

	start_frame = m.frame() + (framepos_t) rint(beat_offset * m.meter().frames_per_division(m.tempo(),_frame_rate));

	// cerr << "from start " << start << " compute frame = " << start_frame 
	// <<  " from metric at " << m.frame() << " tempo = " << m.tempo().beats_per_minute () << " meter " 
	// << m.meter().divisions_per_bar() << '/' << m.meter().note_divisor() 
	// << endl;

	m =  metric_at(end);

	bar_offset = end.bars - m.start().bars;

	beat_offset = bar_offset * m.meter().divisions_per_bar() - (m.start().beats -1) + (end.beats - 1)
		+ end.ticks/BBT_Time::ticks_per_beat;

	end_frame = m.frame() + (framepos_t) rint(beat_offset * m.meter().frames_per_division(m.tempo(),_frame_rate));

	// cerr << "from end " << end << " compute frame = " << end_frame 
	// <<  " from metric at " << m.frame() << " tempo = " << m.tempo().beats_per_minute () << " meter " 
	// << m.meter().divisions_per_bar() << '/' << m.meter().note_divisor() 
	// << endl;

	frames = end_frame - start_frame;

	return frames;

}

framecnt_t
TempoMap::count_frames_between_metrics (const Meter& meter, const Tempo& tempo, const BBT_Time& start, const BBT_Time& end) const
{
	/* this is used in timestamping the metrics by actually counting the beats */

	framecnt_t frames = 0;
	uint32_t bar = start.bars;
	double beat = (double) start.beats;
	double divisions_counted = 0;
	double divisions_per_bar = 0;
	double division_frames = 0;

	divisions_per_bar = meter.divisions_per_bar();
	division_frames = meter.frames_per_division (tempo, _frame_rate);

	frames = 0;

	while (bar < end.bars || (bar == end.bars && beat < end.beats)) {

		if (beat >= divisions_per_bar) {
			beat = 1;
			++bar;
			++divisions_counted;

			if (beat > divisions_per_bar) {

				/* this is a fractional beat at the end of a fractional bar
				   so it should only count for the fraction
				*/

				divisions_counted -= (ceil(divisions_per_bar) - divisions_per_bar);
			}

		} else {
			++beat;
			++divisions_counted;
		}
	}

	// cerr << "Counted " << beats_counted << " from " << start << " to " << end
	// << " bpb were " << divisions_per_bar
	// << " fpb was " << beat_frames
	// << endl;

	frames = (framecnt_t) llrint (floor (divisions_counted * division_frames));

	return frames;

}

framepos_t
TempoMap::frame_time (const BBT_Time& bbt) const
{
	BBT_Time start ; /* 1|1|0 */

	return count_frames_between (start, bbt);
}

framecnt_t
TempoMap::bbt_duration_at (framepos_t pos, const BBT_Time& bbt, int dir) const
{
	framecnt_t frames = 0;

	BBT_Time when;
	bbt_time(pos, when);

	{
		Glib::RWLock::ReaderLock lm (lock);
		frames = bbt_duration_at_unlocked (when, bbt,dir);
	}

	return frames;
}

framecnt_t
TempoMap::bbt_duration_at_unlocked (const BBT_Time& when, const BBT_Time& bbt, int dir) const
{
	framecnt_t frames = 0;

	double divisions_per_bar;
	BBT_Time result;

	result.bars = max(1U, when.bars + dir * bbt.bars) ;
	result.beats = 1;
	result.ticks = 0;

	TempoMetric	metric = metric_at(result);
	divisions_per_bar = metric.meter().divisions_per_bar();

	/* Reduce things to legal bbt values we have to handle possible
	  fractional=shorter beats at the end of measures and things like 0|11|9000
	  as a duration in a 4.5/4 measure the musical decision is that the
	  fractional beat is also a beat , although a shorter one
	*/

	if (dir >= 0) {
		result.beats = when.beats +  bbt.beats;
		result.ticks = when.ticks +  bbt.ticks;

		while (result.beats >= (divisions_per_bar + 1)) {
			result.bars++;
			result.beats -=  (uint32_t) ceil(divisions_per_bar);
			metric = metric_at(result); // maybe there is a meter change
			divisions_per_bar = metric.meter().divisions_per_bar();

		}

		/* We now counted the beats and landed in the target measure, now deal
		  with ticks this seems complicated, but we want to deal with the
		  corner case of a sequence of time signatures like 0.2/4-0.7/4 and
		  with request like bbt = 3|2|9000 ,so we repeat the same loop but add
		  ticks
		*/

		/* of course gtk_ardour only allows bar with at least 1.0 beats .....
		 */

		uint32_t ticks_at_beat = (uint32_t) (result.beats == ceil(divisions_per_bar) ?
					(1 - (ceil(divisions_per_bar) - divisions_per_bar))* BBT_Time::ticks_per_beat
					   : BBT_Time::ticks_per_beat );

		while (result.ticks >= ticks_at_beat) {
			result.beats++;
			result.ticks -= ticks_at_beat;
			if  (result.beats >= (divisions_per_bar + 1)) {
				result.bars++;
				result.beats = 1;
				metric = metric_at(result); // maybe there is a meter change
				divisions_per_bar = metric.meter().divisions_per_bar();
			}
			ticks_at_beat= (uint32_t) (result.beats == ceil(divisions_per_bar) ?
				       (1 - (ceil(divisions_per_bar) - divisions_per_bar) ) * BBT_Time::ticks_per_beat
				       : BBT_Time::ticks_per_beat);
		}


	} else {
		uint32_t b = bbt.beats;

		/* count beats */
		while (b > when.beats) {
			--result.bars;
			result.bars = max(1U, result.bars);
			metric = metric_at(result); // maybe there is a meter change
			divisions_per_bar = metric.meter().divisions_per_bar();
			if (b >= ceil(divisions_per_bar)) {
				b -= (uint32_t) ceil(divisions_per_bar);
			} else {
				b = (uint32_t) ceil(divisions_per_bar) - b + when.beats ;
			}
		}
		result.beats = when.beats - b;

		/* count ticks */

		if (bbt.ticks <= when.ticks) {
			result.ticks = when.ticks - bbt.ticks;
		} else {

			uint32_t ticks_at_beat= (uint32_t) BBT_Time::ticks_per_beat;
			uint32_t t = bbt.ticks - when.ticks;

			do {

				if (result.beats == 1) {
					--result.bars;
					result.bars = max(1U, result.bars) ;
					metric = metric_at(result); // maybe there is a meter change
					divisions_per_bar = metric.meter().divisions_per_bar();
					result.beats = (uint32_t) ceil(divisions_per_bar);
					ticks_at_beat = (uint32_t) ((1 - (ceil(divisions_per_bar) - divisions_per_bar)) * BBT_Time::ticks_per_beat) ;
				} else {
					--result.beats;
					ticks_at_beat = (uint32_t) BBT_Time::ticks_per_beat;
				}

				if (t <= ticks_at_beat) {
					result.ticks = ticks_at_beat - t;
				} else {
					t-= ticks_at_beat;
				}
			} while (t > ticks_at_beat);

		}


	}

	if (dir < 0) {
		frames = count_frames_between(result, when);
	} else {
		frames = count_frames_between(when,result);
	}

	return frames;
}



framepos_t
TempoMap::round_to_bar (framepos_t fr, int dir)
{
	{
		Glib::RWLock::ReaderLock lm (lock);
		return round_to_type (fr, dir, Bar);
	}
}


framepos_t
TempoMap::round_to_beat (framepos_t fr, int dir)
{
	{
		Glib::RWLock::ReaderLock lm (lock);
		return round_to_type (fr, dir, Beat);
	}
}

framepos_t
TempoMap::round_to_beat_subdivision (framepos_t fr, int sub_num, int dir)
{
	BBT_Time the_beat;
	uint32_t ticks_one_half_subdivisions_worth;
	uint32_t ticks_one_subdivisions_worth;
	uint32_t difference;

	bbt_time(fr, the_beat);

	ticks_one_subdivisions_worth = (uint32_t)BBT_Time::ticks_per_beat / sub_num;
	ticks_one_half_subdivisions_worth = ticks_one_subdivisions_worth / 2;

	if (dir > 0) {

		/* round to next */

		uint32_t mod = the_beat.ticks % ticks_one_subdivisions_worth;

		if (mod == 0) {
			/* right on the subdivision, so the difference is just the subdivision ticks */
			difference = ticks_one_subdivisions_worth;

		} else {
			/* not on subdivision, compute distance to next subdivision */

			difference = ticks_one_subdivisions_worth - mod;
		}

		the_beat = bbt_add (the_beat, BBT_Time (0, 0, difference));

	} else if (dir < 0) {

		/* round to previous */

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

		try {
			the_beat = bbt_subtract (the_beat, BBT_Time (0, 0, difference));
		} catch (...) {
			/* can't go backwards from wherever pos is, so just return it */
			return fr;
		}

	} else {
		/* round to nearest */

		if (the_beat.ticks % ticks_one_subdivisions_worth > ticks_one_half_subdivisions_worth) {
			difference = ticks_one_subdivisions_worth - (the_beat.ticks % ticks_one_subdivisions_worth);
			the_beat = bbt_add (the_beat, BBT_Time (0, 0, difference));
		} else {
			// difference = ticks_one_subdivisions_worth - (the_beat.ticks % ticks_one_subdivisions_worth);
			the_beat.ticks -= the_beat.ticks % ticks_one_subdivisions_worth;
		}
	}

	return frame_time (the_beat);
}

framepos_t
TempoMap::round_to_type (framepos_t frame, int dir, BBTPointType type)
{
	TempoMetric metric = metric_at (frame);
	BBT_Time bbt;
	BBT_Time start;
	BBT_Time one_bar (1,0,0);
	BBT_Time one_beat (0,1,0);

	bbt_time_with_metric (frame, bbt, metric);

	switch (type) {
	case Bar:
		DEBUG_TRACE(DEBUG::SnapBBT, string_compose ("round from %1 (%3) to bars in direction %2\n", frame, dir, bbt));

		if (dir < 0) {

			/* find bar position preceding frame */

			try {
				bbt = bbt_subtract (bbt, one_bar);
			}

			catch (...) {
				return frame;
			}


		} else if (dir > 0) {

			/* find bar position following frame */

			try {
				bbt = bbt_add (bbt, one_bar, metric);
			}
			catch (...) {
				return frame;
			}

		} else {

			/* "true" rounding */

			float midbar_beats;
			float midbar_ticks;

			midbar_beats = metric.meter().divisions_per_bar() / 2 + 1;
			midbar_ticks = BBT_Time::ticks_per_beat * fmod (midbar_beats, 1.0f);
			midbar_beats = floor (midbar_beats);

			BBT_Time midbar (bbt.bars, lrintf (midbar_beats), lrintf (midbar_ticks));

			if (bbt < midbar) {
				/* round down */
				bbt.beats = 1;
				bbt.ticks = 0;
			} else {
				/* round up */
				bbt.bars++;
				bbt.beats = 1;
				bbt.ticks = 0;
			}
		}
		/* force beats & ticks to their values at the start of a bar */
		bbt.beats = 1;
		bbt.ticks = 0;
		break;

	case Beat:
		DEBUG_TRACE(DEBUG::SnapBBT, string_compose ("round from %1 (%3) to beat in direction %2\n", frame, (dir < 0 ? "back" : "forward"), bbt));

		if (dir < 0) {

			/* find beat position preceding frame */

			try {
				bbt = bbt_subtract (bbt, one_beat);
			}

			catch (...) {
				return frame;
			}


		} else if (dir > 0) {

			/* find beat position following frame */

			try {
				bbt = bbt_add (bbt, one_beat, metric);
			}
			catch (...) {
				return frame;
			}

		} else {

			/* "true" rounding */

			/* round to nearest beat */
			if (bbt.ticks >= (BBT_Time::ticks_per_beat/2)) {

				try {
					bbt = bbt_add (bbt, one_beat, metric);
				}
				catch (...) {
					return frame;
				}
			}
		}
		/* force ticks to the value at the start of a beat */
		bbt.ticks = 0;
		break;

	}

	DEBUG_TRACE(DEBUG::SnapBBT, string_compose ("\tat %1 count frames from %2 to %3 = %4\n", metric.frame(), metric.start(), bbt, count_frames_between (metric.start(), bbt)));
	return metric.frame() + count_frames_between (metric.start(), bbt);
}

TempoMap::BBTPointList *
TempoMap::get_points (framepos_t lower, framepos_t upper) const
{

	Metrics::const_iterator i;
	BBTPointList *points;
	double current;
	const MeterSection* meter;
	const MeterSection* m;
	const TempoSection* tempo;
	const TempoSection* t;
	uint32_t bar;
	uint32_t beat;
	double divisions_per_bar;
	double beat_frame;
	double beat_frames;
	double frames_per_bar;
	double delta_bars;
	double delta_beats;
	double dummy;
	framepos_t limit;

	meter = &first_meter ();
	tempo = &first_tempo ();

	/* find the starting point */

	for (i = metrics->begin(); i != metrics->end(); ++i) {

		if ((*i)->frame() > lower) {
			break;
		}

		if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
			tempo = t;
		} else if ((m = dynamic_cast<const MeterSection*>(*i)) != 0) {
			meter = m;
		}
	}

	/* We now have:

	   meter -> the Meter for "lower"
	   tempo -> the Tempo for "lower"
	   i     -> for first new metric after "lower", possibly metrics->end()

	   Now start generating points.
	*/

	divisions_per_bar = meter->divisions_per_bar ();
	frames_per_bar = meter->frames_per_bar (*tempo, _frame_rate);
	beat_frames = meter->frames_per_division (*tempo,_frame_rate);

	if (meter->frame() > tempo->frame()) {
		bar = meter->start().bars;
		beat = meter->start().beats;
		current = meter->frame();
	} else {
		bar = tempo->start().bars;
		beat = tempo->start().beats;
		current = tempo->frame();
	}

	/* initialize current to point to the bar/beat just prior to the
	   lower frame bound passed in.  assumes that current is initialized
	   above to be on a beat.
	*/

	delta_bars = (lower-current) / frames_per_bar;
	delta_beats = modf(delta_bars, &dummy) * divisions_per_bar;
	current += (floor(delta_bars) * frames_per_bar) +  (floor(delta_beats) * beat_frames);

	// adjust bars and beats too
	bar += (uint32_t) (floor(delta_bars));
	beat += (uint32_t) (floor(delta_beats));

	points = new BBTPointList;

	do {

		if (i == metrics->end()) {
			limit = upper;
			// cerr << "== limit set to end of request @ " << limit << endl;
		} else {
			// cerr << "== limit set to next metric @ " << (*i)->frame() << endl;
			limit = (*i)->frame();
		}

		limit = min (limit, upper);

		while (current < limit) {

			/* if we're at the start of a bar, add bar point */

			if (beat == 1) {
				if (current >= lower) {
					// cerr << "Add Bar at " << bar << "|1" << " @ " << current << endl;
					points->push_back (BBTPoint (*meter, *tempo,(framepos_t)rint(current), Bar, bar, 1));

				}
			}

			/* add some beats if we can */

			beat_frame = current;

			while (beat <= ceil(divisions_per_bar) && beat_frame < limit) {
				if (beat_frame >= lower) {
					// cerr << "Add Beat at " << bar << '|' << beat << " @ " << beat_frame << endl;
					points->push_back (BBTPoint (*meter, *tempo, (framepos_t) rint(beat_frame), Beat, bar, beat));
				}
				beat_frame += beat_frames;
				current+= beat_frames;

				beat++;
			}

			// cerr << "out of beats, @ end ? " << (i == metrics->end()) << " out of bpb ? "
			// << (beat > ceil(divisions_per_bar))
			// << " beat frame @ " << beat_frame << " vs. " << limit
			// << endl;

			if (beat > ceil(divisions_per_bar) || i != metrics->end()) {

				/* we walked an entire bar. its
				   important to move `current' forward
				   by the actual frames_per_bar, not move it to
				   an integral beat_frame, so that metrics with
				   non-integral beats-per-bar have
				   their bar positions set
				   correctly. consider a metric with
				   9-1/2 beats-per-bar. the bar we
				   just filled had  10 beat marks,
				   but the bar end is 1/2 beat before
				   the last beat mark.
				   And it is also possible that a tempo
				   change occured in the middle of a bar,
				   so we subtract the possible extra fraction from the current
				*/

				if (beat > ceil (divisions_per_bar)) {
					/* next bar goes where the numbers suggest */
					current -=  beat_frames * (ceil(divisions_per_bar)-divisions_per_bar);
					// cerr << "++ next bar from numbers\n";
				} else {
					/* next bar goes where the next metric is */
					current = limit;
					// cerr << "++ next bar at next metric\n";
				}
				bar++;
				beat = 1;
			}

		}

		/* if we're done, then we're done */

		if (current >= upper) {
			break;
		}

		/* i is an iterator that refers to the next metric (or none).
		   if there is a next metric, move to it, and continue.
		*/

		if (i != metrics->end()) {

			if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
				tempo = t;
			} else if ((m = dynamic_cast<const MeterSection*>(*i)) != 0) {
				meter = m;
				/* new MeterSection, beat always returns to 1 */
				beat = 1;
			}
			
			current = (*i)->frame ();
			// cerr << "loop around with current @ " << current << endl;

			divisions_per_bar = meter->divisions_per_bar ();
			frames_per_bar = meter->frames_per_bar (*tempo, _frame_rate);
			beat_frames = meter->frames_per_division (*tempo, _frame_rate);

			++i;
		}

	} while (1);

	return points;
}

const TempoSection&
TempoMap::tempo_section_at (framepos_t frame) const
{
	Glib::RWLock::ReaderLock lm (lock);
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
		Glib::RWLock::ReaderLock lm (lock);
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
		Glib::RWLock::WriterLock lm (lock);

		XMLNodeList nlist;
		XMLNodeConstIterator niter;
		Metrics old_metrics (*metrics);

		metrics->clear();

		nlist = node.children();

		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			XMLNode* child = *niter;

			if (child->name() == TempoSection::xml_state_node_name) {

				try {
					metrics->push_back (new TempoSection (*child));
				}

				catch (failed_constructor& err){
					error << _("Tempo map: could not set new state, restoring old one.") << endmsg;
					*metrics = old_metrics;
					break;
				}

			} else if (child->name() == MeterSection::xml_state_node_name) {

				try {
					metrics->push_back (new MeterSection (*child));
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
			timestamp_metrics (true);
		}
	}

	PropertyChanged (PropertyChange ());

	return 0;
}

void
TempoMap::dump (std::ostream& o) const
{
	const MeterSection* m;
	const TempoSection* t;

	for (Metrics::const_iterator i = metrics->begin(); i != metrics->end(); ++i) {

		if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
			o << "Tempo @ " << *i << ' ' << t->beats_per_minute() << " BPM (denom = " << t->note_type() << ") at " << t->start() << " frame= " << t->frame() << " (move? "
			  << t->movable() << ')' << endl;
		} else if ((m = dynamic_cast<const MeterSection*>(*i)) != 0) {
			o << "Meter @ " << *i << ' ' << m->divisions_per_bar() << '/' << m->note_divisor() << " at " << m->start() << " frame= " << m->frame()
			  << " (move? " << m->movable() << ')' << endl;
		}
	}
}

int
TempoMap::n_tempos() const
{
	Glib::RWLock::ReaderLock lm (lock);
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
	Glib::RWLock::ReaderLock lm (lock);
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
	for (Metrics::iterator i = metrics->begin(); i != metrics->end(); ++i) {
		if ((*i)->frame() >= where && (*i)->movable ()) {
			(*i)->set_frame ((*i)->frame() + amount);
		}
	}

	timestamp_metrics (false);

	PropertyChanged (PropertyChange ());
}

BBT_Time
TempoMap::bbt_add (const BBT_Time& start, const BBT_Time& other) const
{
	TempoMetric metric =  metric_at (start);
	return bbt_add (start, other, metric);
}

/**
 * add the BBT interval @param increment to  @param start and return the result
 */
BBT_Time
TempoMap::bbt_add (const BBT_Time& start, const BBT_Time& increment, const TempoMetric& /*metric*/) const
{
	BBT_Time result = start;
	BBT_Time op = increment; /* argument is const, but we need to modify it */
	uint32_t ticks = result.ticks + op.ticks;

	if (ticks >= BBT_Time::ticks_per_beat) {
		op.beats++;
		result.ticks = ticks % (uint32_t) BBT_Time::ticks_per_beat;
	} else {
		result.ticks += op.ticks;
	}

	/* now comes the complicated part. we have to add one beat a time,
	   checking for a new metric on every beat.
	*/

	/* grab all meter sections */

	list<const MeterSection*> meter_sections;

	for (Metrics::const_iterator x = metrics->begin(); x != metrics->end(); ++x) {
		const MeterSection* ms;
		if ((ms = dynamic_cast<const MeterSection*>(*x)) != 0) {
			meter_sections.push_back (ms);
		}
	}

	assert (!meter_sections.empty());

	list<const MeterSection*>::const_iterator next_meter;
	const Meter* meter = 0;

	/* go forwards through the meter sections till we get to the one
	   covering the current value of result. this positions i to point to
	   the next meter section too, or the end.
	*/

	for (next_meter = meter_sections.begin(); next_meter != meter_sections.end(); ++next_meter) {

		if (result < (*next_meter)->start()) {
			/* this metric is past the result time. stop looking, we have what we need */
			break;
		}

		if (result == (*next_meter)->start()) {
			/* this meter section starts at result, push i beyond it so that it points
			   to the NEXT section, opwise we will get stuck later, and use this meter section.
			*/
			meter = *next_meter;
			++next_meter;
			break;
		}

		meter = *next_meter;
	}

	assert (meter != 0);

	/* OK, now have the meter for the bar start we are on, and i is an iterator
	   that points to the metric after the one we are currently dealing with
	   (or to metrics->end(), of course)
	*/

	while (op.beats) {

		/* given the current meter, have we gone past the end of the bar ? */

		if (result.beats >= meter->divisions_per_bar()) {
			/* move to next bar, first beat */
			result.bars++;
			result.beats = 1;
		} else {
			result.beats++;
		}

		/* one down ... */

		op.beats--;

		/* check if we need to use a new meter section: has adding beats to result taken us
		   to or after the start of the next meter section? in which case, use it.
		*/

		if (next_meter != meter_sections.end() && (((*next_meter)->start () < result) || (result == (*next_meter)->start()))) {
			meter = *next_meter;
			++next_meter;
		}
	}

	/* finally, add bars */

	result.bars += op.bars++;

	return result;
}

/**
 * subtract the BBT interval @param decrement from @param start and return the result
 */
BBT_Time
TempoMap::bbt_subtract (const BBT_Time& start, const BBT_Time& decrement) const
{
	BBT_Time result = start;
	BBT_Time op = decrement; /* argument is const, but we need to modify it */

	if (op.ticks > result.ticks) {
		/* subtract an extra beat later; meanwhile set ticks to the right "carry" value */
		op.beats++;
		result.ticks = BBT_Time::ticks_per_beat - (op.ticks - result.ticks);
	} else {
		result.ticks -= op.ticks;
	}

	/* now comes the complicated part. we have to subtract one beat a time,
	   checking for a new metric on every beat.
	*/

	/* grab all meter sections */

	list<const MeterSection*> meter_sections;

	for (Metrics::const_iterator x = metrics->begin(); x != metrics->end(); ++x) {
		const MeterSection* ms;
		if ((ms = dynamic_cast<const MeterSection*>(*x)) != 0) {
			meter_sections.push_back (ms);
		}
		}

	assert (!meter_sections.empty());

	/* go backwards through the meter sections till we get to the one
	   covering the current value of result. this positions i to point to
	   the next (previous) meter section too, or the end.
	*/

	const MeterSection* meter = 0;
	list<const MeterSection*>::reverse_iterator next_meter; // older versions of GCC don't
	                                                        // support const_reverse_iterator::operator!=()

	for (next_meter = meter_sections.rbegin(); next_meter != meter_sections.rend(); ++next_meter) {

		/* when we find the first meter section that is before or at result, use it,
		   and set next_meter to the previous one
		*/

		if ((*next_meter)->start() < result || (*next_meter)->start() == result) {
			meter = *next_meter;
			++next_meter;
			break;
		}
	}

	assert (meter != 0);

	/* OK, now have the meter for the bar start we are on, and i is an iterator
	   that points to the metric after the one we are currently dealing with
	   (or to metrics->end(), of course)
	*/

	while (op.beats) {

		/* have we reached the start of the bar? if so, move to the last beat of the previous
		   bar. opwise, just step back 1 beat.
		*/

		if (result.beats == 1) {

			/* move to previous bar, last beat */

			if (result.bars <= 1) {
				/* i'm sorry dave, i can't do that */
				throw std::out_of_range ("illegal BBT subtraction");
			}

			result.bars--;
			result.beats = meter->divisions_per_bar();
		} else {

			/* back one beat */

			result.beats--;
		}

		/* one down ... */
		op.beats--;

		/* check if we need to use a new meter section: has subtracting beats to result taken us
		   to before the start of the current meter section? in which case, use the prior one.
		*/

		if (result < meter->start() && next_meter != meter_sections.rend()) {
			meter = *next_meter;
			++next_meter;
		}
	}

	/* finally, subtract bars */

	if (op.bars >= result.bars) {
		/* i'm sorry dave, i can't do that */
		throw std::out_of_range ("illegal BBT subtraction");
	}

	result.bars -= op.bars;
	return result;
}

/** Add some (fractional) beats to a session frame position, and return the result in frames.
 *  pos can be -ve, if required.
 */
framepos_t
TempoMap::framepos_plus_beats (framepos_t pos, Evoral::MusicalTime beats) const
{
	Metrics::const_iterator i;
	const TempoSection* tempo;

	/* Find the starting tempo */

	for (i = metrics->begin(); i != metrics->end(); ++i) {

		/* This is a bit of a hack, but pos could be -ve, and if it is,
		   we consider the initial metric changes (at time 0) to actually
		   be in effect at pos.
		*/
		framepos_t f = (*i)->frame ();
		if (pos < 0 && f == 0) {
			f = pos;
		}

		if (f > pos) {
			break;
		}

		const TempoSection* t;

		if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
			tempo = t;
		}
	}

	/* We now have:

	   tempo -> the Tempo for "pos"
	   i     -> for first new metric after "pos", possibly metrics->end()
	*/

	while (beats) {

		/* Distance to the end of this section in frames */
		framecnt_t distance_frames = i == metrics->end() ? max_framepos : ((*i)->frame() - pos);

		/* Distance to the end in beats */
		Evoral::MusicalTime distance_beats = distance_frames / tempo->frames_per_beat (_frame_rate);

		/* Amount to subtract this time */
		double const sub = min (distance_beats, beats);

		/* Update */
		beats -= sub;
		pos += sub * tempo->frames_per_beat (_frame_rate);

		/* Move on if there's anything to move to */
		if (i != metrics->end ()) {
			const TempoSection* t;
			
			if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
				tempo = t;
			}

			++i;
		}
	}

	return pos;
}

/** Subtract some (fractional) beats to a frame position, and return the result in frames */
framepos_t
TempoMap::framepos_minus_beats (framepos_t pos, Evoral::MusicalTime beats) const
{
	Metrics::const_iterator i;
	const TempoSection* tempo = 0;
	const TempoSection* t;
	
	/* Find the starting tempo */

	for (i = metrics->begin(); i != metrics->end(); ++i) {

		/* This is a bit of a hack, but pos could be -ve, and if it is,
		   we consider the initial metric changes (at time 0) to actually
		   be in effect at pos.
		*/
		framepos_t f = (*i)->frame ();
		if (pos < 0 && f == 0) {
			f = pos;
		}

		if ((*i)->frame() > pos) {
			break;
		}

		if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
			tempo = t;
		}
	}

	bool no_more_tempos = false;

	/* Move i back to the tempo before "pos" */
	if (i != metrics->begin ()) {
		while (i != metrics->begin ()) {
			--i;
			t = dynamic_cast<TempoSection*> (*i);
			if (t) {
				break;
			}
		}
	} else {
		no_more_tempos = true;
	}

	/* We now have:

	   tempo -> the Tempo for "pos"
	   i     -> the first metric before "pos", unless no_more_tempos is true
	*/

	while (beats) {

		/* Distance to the end of this section in frames */
		framecnt_t distance_frames = no_more_tempos ? max_framepos : (pos - (*i)->frame());

		/* Distance to the end in beats */
		Evoral::MusicalTime distance_beats = distance_frames / tempo->frames_per_beat (_frame_rate);

		/* Amount to subtract this time */
		double const sub = min (distance_beats, beats);

		/* Update */
		beats -= sub;
		pos -= sub * tempo->frames_per_beat (_frame_rate);

		/* Move i and tempo back, if there's anything to move to */
		if (i != metrics->begin ()) {
			while (i != metrics->begin ()) {
				--i;
				if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
					tempo = t;
					break;
				}
			}
		} else {
			no_more_tempos = true;
		}
	}

	return pos;
}

/** Add the BBT interval op to pos and return the result */
framepos_t
TempoMap::framepos_plus_bbt (framepos_t pos, BBT_Time op) const
{
	Metrics::const_iterator i;
	const MeterSection* meter;
	const MeterSection* m;
	const TempoSection* tempo;
	const TempoSection* t;
	double frames_per_beat;

	meter = &first_meter ();
	tempo = &first_tempo ();

	assert (meter);
	assert (tempo);

	/* find the starting metrics for tempo & meter */

	for (i = metrics->begin(); i != metrics->end(); ++i) {

		if ((*i)->frame() > pos) {
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
	   i     -> for first new metric after "pos", possibly metrics->end()
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

		if (i != metrics->end()) {
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

		if (i != metrics->end()) {
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
	Metrics::const_iterator i;
	const TempoSection* tempo;
	
	/* Find the starting tempo */

	for (i = metrics->begin(); i != metrics->end(); ++i) {

		if ((*i)->frame() > pos) {
			break;
		}

		const TempoSection* t;

		if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
			tempo = t;
		}
	}

	/* We now have:

	   tempo -> the Tempo for "pos"
	   i     -> the first metric after "pos", possibly metrics->end()
	*/

	Evoral::MusicalTime beats = 0;

	while (distance) {

		/* End of this section */
		framepos_t const end = i == metrics->end() ? max_framepos : (*i)->frame ();

		/* Distance to the end in frames */
		framecnt_t const distance_to_end = end - pos;

		/* Amount to subtract this time */
		double const sub = min (distance, distance_to_end);

		/* Update */
		pos += sub;
		distance -= sub;
		beats += sub / tempo->frames_per_beat (_frame_rate);

		/* Move on if there's anything to move to */
		if (i != metrics->end ()) {
			const TempoSection* t;
			
			if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
				tempo = t;
			}

			++i;
		}
	}

	return beats;
}

/** Compare the time of this with that of another MetricSection.
 *  @param with_bbt True to compare using start(), false to use frame().
 *  @return -1 for less than, 0 for equal, 1 for greater than.
 */

int
MetricSection::compare (MetricSection* other, bool with_bbt) const
{
	if (with_bbt) {
		if (start() == other->start()) {
			return 0;
		} else if (start() < other->start()) {
			return -1;
		} else {
			return 1;
		}
	} else {
		if (frame() == other->frame()) {
			return 0;
		} else if (frame() < other->frame()) {
			return -1;
		} else {
			return 1;
		}
	}

	/* NOTREACHED */
	return 0;
}

std::ostream& 
operator<< (std::ostream& o, const Meter& m) {
	return o << m.divisions_per_bar() << '/' << m.note_divisor();
}
std::ostream& 
operator<< (std::ostream& o, const Tempo& t) {
	return o << t.beats_per_minute() << " (1/" << t.note_type() << " per minute)" << endl;
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
