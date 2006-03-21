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

    $Id$
*/

#include <algorithm>
#include <unistd.h>

#include <cmath>

#include <sigc++/bind.h>

#include <pbd/lockmonitor.h>
#include <pbd/xml++.h>
#include <ardour/tempo.h>
#include <ardour/utils.h>

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;

/* _default tempo is 4/4 qtr=120 */

Meter    TempoMap::_default_meter (4.0, 4.0);
Tempo    TempoMap::_default_tempo (120.0);

const double Meter::ticks_per_beat = 1920.0;

/***********************************************************************/

double
Meter::frames_per_bar (const Tempo& tempo, jack_nframes_t sr) const
{
	return ((60.0 * sr * _beats_per_bar) / tempo.beats_per_minute());
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

	if ((prop = node.property ("movable")) == 0) {
		error << _("TempoSection XML node has no \"movable\" property") << endmsg;
		throw failed_constructor();
	}

	set_movable (prop->value() == "yes");
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

	if ((prop = node.property ("beats-per-bar")) == 0) {
		error << _("MeterSection XML node has no \"beats-per-bar\" property") << endmsg;
		throw failed_constructor();
	}

	if (sscanf (prop->value().c_str(), "%lf", &_beats_per_bar) != 1 || _beats_per_bar < 0.0) {
		error << _("MeterSection XML node has an illegal \"beats-per-bar\" value") << endmsg;
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

	set_movable (prop->value() == "yes");
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
	snprintf (buf, sizeof (buf), "%f", _beats_per_bar);
	root->add_property ("beats-per-bar", buf);
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

TempoMap::TempoMap (jack_nframes_t fr)
{
	metrics = new Metrics;
	_frame_rate = fr;
	last_bbt_valid = false;
	BBT_Time start;
	in_set_state = false;
	
	start.bars = 1;
	start.beats = 1;
	start.ticks = 0;

	TempoSection *t = new TempoSection (start, _default_tempo.beats_per_minute());
	MeterSection *m = new MeterSection (start, _default_meter.beats_per_bar(), _default_meter.note_divisor());

	t->set_movable (false);
	m->set_movable (false);

	/* note: frame time is correct (zero) for both of these */
	
	metrics->push_back (t);
	metrics->push_back (m);
	
	save_state (_("initial"));
}

TempoMap::~TempoMap ()
{
}

int
TempoMap::move_metric_section (MetricSection& section, const BBT_Time& when)
{
	if (when == section.start()) {
		return -1;
	}

	if (!section.movable()) {
		return 1;
	}

	LockMonitor lm (lock, __LINE__, __FILE__);
	MetricSectionSorter cmp;
	BBT_Time corrected (when);
	
	if (dynamic_cast<MeterSection*>(&section) != 0) {
		if (corrected.beats > 1) {
			corrected.beats = 1;
			corrected.bars++;
		}
	}
	corrected.ticks = 0;

	section.set_start (corrected);
	metrics->sort (cmp);
	timestamp_metrics ();
	save_state (_("move metric"));

	return 0;
}

void
TempoMap::move_tempo (TempoSection& tempo, const BBT_Time& when)
{
	if (move_metric_section (tempo, when) == 0) {
		send_state_changed (Change (0));
	}
}

void
TempoMap::move_meter (MeterSection& meter, const BBT_Time& when)
{
	if (move_metric_section (meter, when) == 0) {
		send_state_changed (Change (0));
	}
}
		

void
TempoMap::remove_tempo (const TempoSection& tempo)
{
	bool removed = false;

	{
		LockMonitor lm (lock, __LINE__, __FILE__);
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
		send_state_changed (Change (0));
	}
}

void
TempoMap::remove_meter (const MeterSection& tempo)
{
	bool removed = false;

	{
		LockMonitor lm (lock, __LINE__, __FILE__);
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

		if (removed) {
			save_state (_("metric removed"));
		}
	}

	if (removed) {
		send_state_changed (Change (0));
	}
}

void
TempoMap::do_insert (MetricSection* section)
{
	Metrics::iterator i;

	for (i = metrics->begin(); i != metrics->end(); ++i) {
		
		if ((*i)->start() < section->start()) {
			continue;
		}
		
		metrics->insert (i, section);
		break;
	}
	
	if (i == metrics->end()) {
		metrics->insert (metrics->end(), section);
	}
	
	timestamp_metrics ();
}	

void
TempoMap::add_tempo (const Tempo& tempo, BBT_Time where)
{
	{
		LockMonitor lm (lock, __LINE__, __FILE__);

		/* new tempos always start on a beat */
	
		where.ticks = 0;
		
		do_insert (new TempoSection (where, tempo.beats_per_minute()));

		save_state (_("add tempo"));
	}

	send_state_changed (Change (0));
}

void
TempoMap::replace_tempo (TempoSection& existing, const Tempo& replacement)
{
	bool replaced = false;

	{ 
		LockMonitor lm (lock, __LINE__, __FILE__);
		Metrics::iterator i;
		
		for (i = metrics->begin(); i != metrics->end(); ++i) {
			TempoSection *ts;

			if ((ts = dynamic_cast<TempoSection*>(*i)) != 0 && ts == &existing) {
				
				*((Tempo *) ts) = replacement;

				replaced = true;
				timestamp_metrics ();
				break;
			}
		}

		if (replaced) {
			save_state (_("replace tempo"));
		}
	}
	
	if (replaced) {
		send_state_changed (Change (0));
	}
}

void
TempoMap::add_meter (const Meter& meter, BBT_Time where)
{
	{
		LockMonitor lm (lock, __LINE__, __FILE__);

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

		do_insert (new MeterSection (where, meter.beats_per_bar(), meter.note_divisor()));

		save_state (_("add meter"));
	}

	send_state_changed (Change (0));
}

void
TempoMap::replace_meter (MeterSection& existing, const Meter& replacement)
{
	bool replaced = false;

	{ 
		LockMonitor lm (lock, __LINE__, __FILE__);
		Metrics::iterator i;
		
		for (i = metrics->begin(); i != metrics->end(); ++i) {
			MeterSection *ms;
			if ((ms = dynamic_cast<MeterSection*>(*i)) != 0 && ms == &existing) {
				
				*((Meter*) ms) = replacement;

				replaced = true;
				timestamp_metrics ();
				break;
			}
		}

		if (replaced) {
			save_state (_("replaced meter"));
		}
	}
	
	if (replaced) {
		send_state_changed (Change (0));
	}
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
TempoMap::timestamp_metrics ()
{
	Metrics::iterator i;
	const Meter* meter;
	const Tempo* tempo;
	Meter *m;
	Tempo *t;
	jack_nframes_t current;
	jack_nframes_t section_frames;
	BBT_Time start;
	BBT_Time end;

	meter = &first_meter ();
	tempo = &first_tempo ();
	current = 0;

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
}

TempoMap::Metric
TempoMap::metric_at (jack_nframes_t frame) const
{
	Metric m (first_meter(), first_tempo());
	const Meter* meter;
	const Tempo* tempo;

	/* at this point, we are *guaranteed* to have m.meter and m.tempo pointing
	   at something, because we insert the default tempo and meter during
	   TempoMap construction.

	   now see if we can find better candidates.
	*/

	for (Metrics::const_iterator i = metrics->begin(); i != metrics->end(); ++i) {

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

TempoMap::Metric
TempoMap::metric_at (BBT_Time bbt) const
{
	Metric m (first_meter(), first_tempo());
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
TempoMap::bbt_time (jack_nframes_t frame, BBT_Time& bbt) const
{
	LockMonitor lm (lock, __LINE__, __FILE__);
	bbt_time_unlocked (frame, bbt);
}

void
TempoMap::bbt_time_unlocked (jack_nframes_t frame, BBT_Time& bbt) const
{
	bbt_time_with_metric (frame, bbt, metric_at (frame));
}

void
TempoMap::bbt_time_with_metric (jack_nframes_t frame, BBT_Time& bbt, const Metric& metric) const
{
	jack_nframes_t frame_diff;

	uint32_t xtra_bars = 0;
	double xtra_beats = 0;
	double beats = 0;

	const double beats_per_bar = metric.meter().beats_per_bar();
	const double frames_per_bar = metric.meter().frames_per_bar (metric.tempo(), _frame_rate);
	const double beat_frames = metric.tempo().frames_per_beat (_frame_rate);

	/* now compute how far beyond that point we actually are. */

	frame_diff = frame - metric.frame();

	xtra_bars = (uint32_t) floor (frame_diff / frames_per_bar);
	frame_diff -= (uint32_t) floor (xtra_bars * frames_per_bar);
	xtra_beats = (double) frame_diff / beat_frames;


	/* and set the returned value */

	/* and correct beat/bar shifts to match the meter.
	  remember: beat and bar counting is 1-based, 
	  not zero-based 
	  also the meter may contain a fraction
	*/
	
	bbt.bars = metric.start().bars + xtra_bars; 

	beats = (double) metric.start().beats + xtra_beats;

	bbt.bars += (uint32_t) floor(beats/ (beats_per_bar+1) );

	beats = fmod(beats - 1, beats_per_bar )+ 1.0;
	bbt.ticks = (uint32_t)( round((beats - floor(beats)) *(double) Meter::ticks_per_beat));
	bbt.beats = (uint32_t) floor(beats);

}


jack_nframes_t 
TempoMap::count_frames_between ( const BBT_Time& start, const BBT_Time& end) const
{

        /* for this to work with fractional measure types, start and end have to "legal" BBT types, 
        that means that  the  beats and ticks should be  inside a bar
	*/


	jack_nframes_t frames = 0;
	jack_nframes_t start_frame = 0;
	jack_nframes_t end_frame = 0;

	Metric m = metric_at(start);

	uint32_t bar_offset = start.bars - m.start().bars;

	double  beat_offset = bar_offset*m.meter().beats_per_bar() - (m.start().beats-1) + (start.beats -1) 
		+ start.ticks/Meter::ticks_per_beat;


	start_frame = m.frame() + (jack_nframes_t) rint( beat_offset * m.tempo().frames_per_beat(_frame_rate));

    	m =  metric_at(end);

	bar_offset = end.bars - m.start().bars;

	beat_offset = bar_offset * m.meter().beats_per_bar() - (m.start().beats -1) + (end.beats - 1) 
		+ end.ticks/Meter::ticks_per_beat;

	end_frame = m.frame() + (jack_nframes_t) rint(beat_offset * m.tempo().frames_per_beat(_frame_rate));

	frames = end_frame - start_frame;

	return frames;
	
}	

jack_nframes_t 
TempoMap::count_frames_between_metrics (const Meter& meter, const Tempo& tempo, const BBT_Time& start, const BBT_Time& end) const
{
        /*this is used in timestamping the metrics by actually counting the beats */ 

	jack_nframes_t frames = 0;
	uint32_t bar = start.bars;
	double beat = (double) start.beats;
	double beats_counted = 0;
	double beats_per_bar = 0;
	double beat_frames = 0;

	beats_per_bar = meter.beats_per_bar();
	beat_frames = tempo.frames_per_beat (_frame_rate);

	frames = 0;

	while (bar < end.bars || (bar == end.bars && beat < end.beats)) {
		
		if (beat >= beats_per_bar) {
			beat = 1;
			++bar;
			++beats_counted;
		} else {
			++beat;
			++beats_counted;
			if (beat > beats_per_bar) {
				/* this is a fractional beat at the end of a fractional bar
				   so it should only count for the fraction */
				beats_counted -= (ceil(beats_per_bar) - beats_per_bar);
			}
		}
	}
	
	frames = (jack_nframes_t) floor (beats_counted * beat_frames);

	return frames;
	
}	

jack_nframes_t 
TempoMap::frame_time (const BBT_Time& bbt) const
{
	BBT_Time start ; /* 1|1|0 */

	return  count_frames_between ( start, bbt);
}

jack_nframes_t 
TempoMap::bbt_duration_at (jack_nframes_t pos, const BBT_Time& bbt, int dir) const
{
	jack_nframes_t frames = 0;

	BBT_Time when;
	bbt_time(pos,when);

	{
		LockMonitor lm (lock, __LINE__, __FILE__);
		frames = bbt_duration_at_unlocked (when, bbt,dir);
	}

	return frames;
}

jack_nframes_t 
TempoMap::bbt_duration_at_unlocked (const BBT_Time& when, const BBT_Time& bbt, int dir) const
{

	jack_nframes_t frames = 0;

	double beats_per_bar;
	BBT_Time result;
	
	result.bars = max(1U,when.bars + dir * bbt.bars) ;
	result.beats = 1;
	result.ticks = 0;

	Metric	metric = metric_at(result);
	beats_per_bar = metric.meter().beats_per_bar();



        /*reduce things to legal bbt  values 
	  we have to handle possible fractional=shorter beats at the end of measures
          and things like 0|11|9000  as a duration in a 4.5/4 measure
	  the musical decision is that the fractional beat is also a beat , although a shorter one 
	*/

    
	if (dir >= 0) {
		result.beats = when.beats +  bbt.beats;
		result.ticks = when.ticks +  bbt.ticks;

		while (result.beats >= (beats_per_bar+1)) {
			result.bars++;
			result.beats -=  (uint32_t) ceil(beats_per_bar);
			metric = metric_at(result); // maybe there is a meter change
			beats_per_bar = metric.meter().beats_per_bar();
			
		}
		/*we now counted the beats and landed in the target measure, now deal with ticks 
		  this seems complicated, but we want to deal with the corner case of a sequence of time signatures like 0.2/4-0.7/4
		  and with request like bbt = 3|2|9000 ,so we repeat the same loop but add ticks
		*/

		/* of course gtk_ardour only allows bar with at least 1.0 beats .....
		 */

		uint32_t ticks_at_beat = (uint32_t) ( result.beats == ceil(beats_per_bar) ?
					(1 - (ceil(beats_per_bar) - beats_per_bar))* Meter::ticks_per_beat 
					   : Meter::ticks_per_beat );

		while (result.ticks >= ticks_at_beat) {
			result.beats++;
			result.ticks -= ticks_at_beat;
			if  (result.beats >= (beats_per_bar+1)) {
				result.bars++;
				result.beats = 1;
				metric = metric_at(result); // maybe there is a meter change
				beats_per_bar = metric.meter().beats_per_bar();
			}
			ticks_at_beat= (uint32_t) ( result.beats == ceil(beats_per_bar) ?
				       (1 - (ceil(beats_per_bar) - beats_per_bar) )* Meter::ticks_per_beat 
				       : Meter::ticks_per_beat);

		}

	  
	} else {
		uint32_t b = bbt.beats;

                /* count beats */
		while( b > when.beats ) {
			
			result.bars = max(1U,result.bars-- ) ;
			metric = metric_at(result); // maybe there is a meter change
			beats_per_bar = metric.meter().beats_per_bar();
			if (b >= ceil(beats_per_bar)) {
				
				b -= (uint32_t) ceil(beats_per_bar);
			} else {
				b = (uint32_t) ceil(beats_per_bar)- b + when.beats ;
			}
		}
		result.beats = when.beats - b;
                
                /*count ticks */

		if (bbt.ticks <= when.ticks) {
			result.ticks = when.ticks - bbt.ticks;
		} else {

			uint32_t ticks_at_beat= (uint32_t) Meter::ticks_per_beat;
			uint32_t t = bbt.ticks - when.ticks;

			do {

				if (result.beats == 1) {
					result.bars = max(1U,result.bars-- ) ;
					metric = metric_at(result); // maybe there is a meter change
					beats_per_bar = metric.meter().beats_per_bar();
					result.beats = (uint32_t) ceil(beats_per_bar);
					ticks_at_beat = (uint32_t) ((1 - (ceil(beats_per_bar) - beats_per_bar))* Meter::ticks_per_beat) ;
				} else {
					result.beats --;
					ticks_at_beat = (uint32_t) Meter::ticks_per_beat;
				}
						
				if (t <= ticks_at_beat) {
					result.ticks = ticks_at_beat - t; 
				} else {
					t-= ticks_at_beat;
				}
			} while (t > ticks_at_beat);

		}


	}

	if (dir < 0 ) {
		frames = count_frames_between( result,when);
	} else {
		frames = count_frames_between(when,result);
	}

	return frames;
}



jack_nframes_t
TempoMap::round_to_bar (jack_nframes_t fr, int dir)
{
	LockMonitor lm (lock, __LINE__, __FILE__);
	return round_to_type (fr, dir, Bar);
}


jack_nframes_t
TempoMap::round_to_beat (jack_nframes_t fr, int dir)
{
	LockMonitor lm (lock, __LINE__, __FILE__);
	return round_to_type (fr, dir, Beat);
}

jack_nframes_t

TempoMap::round_to_beat_subdivision (jack_nframes_t fr, int sub_num)
{
        LockMonitor lm (lock, __LINE__, __FILE__);
        TempoMap::BBTPointList::iterator i;
        TempoMap::BBTPointList *more_zoomed_bbt_points;
        jack_nframes_t frame_one_beats_worth;
        jack_nframes_t pos = 0;
	jack_nframes_t next_pos = 0 ;
        double tempo = 1;
        double frames_one_subdivisions_worth;
        bool fr_has_changed = false;

        int n;

	frame_one_beats_worth = (jack_nframes_t) ::floor ((double)  _frame_rate *  60 / 20 ); //one beat @ 20 bpm
	more_zoomed_bbt_points = get_points((fr >= frame_one_beats_worth) ? 
					    fr - frame_one_beats_worth : 0, fr+frame_one_beats_worth );

	if (more_zoomed_bbt_points == 0 || more_zoomed_bbt_points->empty()) {
		return fr;
	}

	for (i = more_zoomed_bbt_points->begin(); i != more_zoomed_bbt_points->end(); i++) {
		if  ((*i).frame <= fr) {
			pos = (*i).frame;
			tempo = (*i).tempo->beats_per_minute();
			
		} else {
			i++;
			next_pos = (*i).frame;
			break;
		}
	}
	frames_one_subdivisions_worth = ((double) _frame_rate *  60 / (sub_num * tempo));

	for (n = sub_num; n > 0; n--) {
		if (fr >= (pos + ((n - 0.5) * frames_one_subdivisions_worth))) {
			fr = (jack_nframes_t) round(pos + (n  * frames_one_subdivisions_worth));
		 	if (fr > next_pos) {
 				fr = next_pos;  //take care of fractional beats that don't match the subdivision asked
 			}
			fr_has_changed = true;
			break;
		}
	}

	if (!fr_has_changed) {
		fr = pos;
	}

        delete more_zoomed_bbt_points;
        return fr ;
}

jack_nframes_t

TempoMap::round_to_type (jack_nframes_t frame, int dir, BBTPointType type)
{
	Metric metric = metric_at (frame);
	BBT_Time bbt;
	BBT_Time start;
	bbt_time_with_metric (frame, bbt, metric);

	switch (type) {
	case Bar:
		if (dir < 0) {
			/* relax */

		} else if (dir > 0) {
			if (bbt.beats > 0) {
				bbt.bars++;
			}
		} else {
			if (bbt.beats > metric.meter().beats_per_bar()/2) {
				bbt.bars++;
			}

		}
		bbt.beats = 1;
		bbt.ticks = 0;
		break;
	
	case Beat:
		if (dir < 0) {
			/* relax */
		} else if (dir > 0) {
			if (bbt.ticks > 0) {
				bbt.beats++;
			}
		} else {
			if (bbt.ticks >= (Meter::ticks_per_beat/2)) {
				bbt.beats++;
			}
		}
		if (bbt.beats > ceil(metric.meter().beats_per_bar()) ) {
			bbt.beats = 1;
			bbt.bars++;
		}
		bbt.ticks = 0;
		break;
	
	}

	return metric.frame() + count_frames_between (metric.start(), bbt);
}

TempoMap::BBTPointList *
TempoMap::get_points (jack_nframes_t lower, jack_nframes_t upper) const
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

	if (meter->frame() > tempo->frame()) {
		bar = meter->start().bars;
		beat = meter->start().beats;
		current = meter->frame();
	} else {
		bar = tempo->start().bars;
		beat = tempo->start().beats;
		current = tempo->frame();
	}

	points = new BBTPointList;

	do {
		double beats_per_bar;
		double beat_frame;
		double beat_frames;
		double frames_per_bar;
		jack_nframes_t limit;
		
		beats_per_bar = meter->beats_per_bar ();
		frames_per_bar = meter->frames_per_bar (*tempo, _frame_rate);
		beat_frames = tempo->frames_per_beat (_frame_rate);

		if (i == metrics->end()) {
			limit = upper;
		} else {
			limit = (*i)->frame();
		}

		limit = min (limit, upper);

		while (current < limit) {
			
			/* if we're at the start of a bar, add bar point */

			if (beat == 1) {
				if (current >= lower) {
					points->push_back (BBTPoint (*meter, *tempo,(jack_nframes_t)rint(current), Bar, bar, 1));

				}
			}

			/* add some beats if we can */

			beat_frame = current;

			while (beat <= ceil( beats_per_bar) && beat_frame < limit) {
				if (beat_frame >= lower) {
					points->push_back (BBTPoint (*meter, *tempo, (jack_nframes_t) rint(beat_frame), Beat, bar, beat));
				}
				beat_frame += beat_frames;
				current+= beat_frames;
			       
				beat++;
			}

			if (beat > ceil(beats_per_bar) ) {

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

				current -=  beat_frames * (ceil(beats_per_bar)-beats_per_bar);
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

			++i;
		}

	} while (1);

	return points;
}	

const Tempo&
TempoMap::tempo_at (jack_nframes_t frame)
{
	Metric m (metric_at (frame));
	return m.tempo();
}


const Meter&
TempoMap::meter_at (jack_nframes_t frame)
{
	Metric m (metric_at (frame));
	return m.meter();
}

XMLNode&
TempoMap::get_state ()
{
	LockMonitor lm (lock, __LINE__, __FILE__);
	Metrics::const_iterator i;
	XMLNode *root = new XMLNode ("TempoMap");

	for (i = metrics->begin(); i != metrics->end(); ++i) {
		root->add_child_nocopy ((*i)->get_state());
	}

	return *root;
}

int
TempoMap::set_state (const XMLNode& node)
{
	{
		LockMonitor lm (lock, __LINE__, __FILE__);

		XMLNodeList nlist;
		XMLNodeConstIterator niter;
		Metrics old_metrics (*metrics);
		
		in_set_state = true;
		
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
			timestamp_metrics ();
		}

		in_set_state = false;
	}
	
	/* This state needs to be saved. This string will never be a part of the 
	   object's history though, because the allow_save flag is false during 
	   session load. This state will eventually be tagged "initial state", 
	   by a call to StateManager::allow_save from Session::set_state.

	   If this state is not saved, there is no way to reach it through undo actions.
	*/
	save_state(_("load XML data"));
	
	send_state_changed (Change (0));

	return 0;
}

void
TempoMap::dump (std::ostream& o) const
{
	const MeterSection* m;
	const TempoSection* t;
	
	for (Metrics::const_iterator i = metrics->begin(); i != metrics->end(); ++i) {

		if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
			o << "Tempo @ " << *i << ' ' << t->beats_per_minute() << " BPM at " << t->start() << " frame= " << t->frame() << " (move? "
			  << t->movable() << ')' << endl;
		} else if ((m = dynamic_cast<const MeterSection*>(*i)) != 0) {
			o << "Meter @ " << *i << ' ' << m->beats_per_bar() << '/' << m->note_divisor() << " at " << m->start() << " frame= " << m->frame() 
			  << " (move? " << m->movable() << ')' << endl;
		}
	}
}

UndoAction
TempoMap::get_memento () const
{
	return sigc::bind (mem_fun (*(const_cast<TempoMap *> (this)), &StateManager::use_state), _current_state_id);
}

Change
TempoMap::restore_state (StateManager::State& state)
{
	LockMonitor lm (lock, __LINE__, __FILE__);

	TempoMapState* tmstate = dynamic_cast<TempoMapState*> (&state);

	/* We can't just set the metrics pointer to the address of the metrics list 
	   stored in the state, cause this would ruin this state for restoring in
	   the future. If they have the same address, they are the same list.
	   Thus we need to copy all the elements from the state metrics list to the 
	   current metrics list.
	*/
	metrics->clear();
	for (Metrics::iterator i = tmstate->metrics->begin(); i != tmstate->metrics->end(); ++i) {
		TempoSection *ts;
		MeterSection *ms;
		
		if ((ts = dynamic_cast<TempoSection*>(*i)) != 0) {
			metrics->push_back (new TempoSection (*ts));
		} else if ((ms = dynamic_cast<MeterSection*>(*i)) != 0) {
			metrics->push_back (new MeterSection (*ms));
		}
	}
	
	last_bbt_valid = false;

	return Change (0);
}

StateManager::State* 
TempoMap::state_factory (std::string why) const
{
	TempoMapState* state = new TempoMapState (why);

	for (Metrics::iterator i = metrics->begin(); i != metrics->end(); ++i) {
		TempoSection *ts;
		MeterSection *ms;
		
		if ((ts = dynamic_cast<TempoSection*>(*i)) != 0) {
			state->metrics->push_back (new TempoSection (*ts));
		} else if ((ms = dynamic_cast<MeterSection*>(*i)) != 0) {
			state->metrics->push_back (new MeterSection (*ms));
		}
	}
		
	return state;
}

void
TempoMap::save_state (std::string why)
{
	if (!in_set_state) {
		StateManager::save_state (why);
	}
}
