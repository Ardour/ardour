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
#include <unistd.h>

#include <cmath>

#include <sigc++/bind.h>

#include <glibmm/thread.h>
#include "pbd/xml++.h"
#include "ardour/tempo.h"
#include "ardour/utils.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/* _default tempo is 4/4 qtr=120 */

Meter    TempoMap::_default_meter (4.0, 4.0);
Tempo    TempoMap::_default_tempo (120.0);

const double Meter::ticks_per_beat = 1920.0;

double Tempo::frames_per_beat (nframes_t sr, const Meter& meter) const
{
	return  ((60.0 * sr) / (_beats_per_minute * meter.note_divisor()/_note_type));
}

/***********************************************************************/

double
Meter::frames_per_bar (const Tempo& tempo, nframes_t sr) const
{
	return ((60.0 * sr * _beats_per_bar) / (tempo.beats_per_minute() * _note_type/tempo.note_type()));
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

TempoMap::TempoMap (nframes_t fr)
{
	metrics = new Metrics;
	_frame_rate = fr;
	last_bbt_valid = false;
	BBT_Time start;
	
	start.bars = 1;
	start.beats = 1;
	start.ticks = 0;

	TempoSection *t = new TempoSection (start, _default_tempo.beats_per_minute(), _default_tempo.note_type());
	MeterSection *m = new MeterSection (start, _default_meter.beats_per_bar(), _default_meter.note_divisor());

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

		nframes_t frame = frame_time (when);
		// cerr << "nominal frame time = " << frame << endl;

		nframes_t prev_frame = round_to_type (frame, -1, Beat);
		nframes_t next_frame = round_to_type (frame, 1, Beat);
		
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
		StateChanged (Change (0));
	}
}

void
TempoMap::move_meter (MeterSection& meter, const BBT_Time& when)
{
	if (move_metric_section (meter, when) == 0) {
		StateChanged (Change (0));
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
		StateChanged (Change (0));
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
		StateChanged (Change (0));
	}
}

void
TempoMap::do_insert (MetricSection* section, bool with_bbt)
{
	Metrics::iterator i;

	for (i = metrics->begin(); i != metrics->end(); ++i) {
		
		if (with_bbt) {
			if ((*i)->start() < section->start()) {
				continue;
			}
		} else {
			if ((*i)->frame() < section->frame()) {
				continue;
			}			
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

	StateChanged (Change (0));
}

void
TempoMap::add_tempo (const Tempo& tempo, nframes_t where)
{
	{
		Glib::RWLock::WriterLock lm (lock);
		do_insert (new TempoSection (where, tempo.beats_per_minute(), tempo.note_type()), false);
	}

	StateChanged (Change (0));
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
		StateChanged (Change (0));
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

		do_insert (new MeterSection (where, meter.beats_per_bar(), meter.note_divisor()), true);
	}

	StateChanged (Change (0));
}

void
TempoMap::add_meter (const Meter& meter, nframes_t where)
{
	{
		Glib::RWLock::WriterLock lm (lock);
		do_insert (new MeterSection (where, meter.beats_per_bar(), meter.note_divisor()), false);
	}

	StateChanged (Change (0));
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
		StateChanged (Change (0));
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
			StateChanged (Change (0));
			break;
		}
	}
}

void
TempoMap::change_existing_tempo_at (nframes_t where, double beats_per_minute, double note_type)
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
	StateChanged (Change (0));
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

		nframes_t current = 0;
		nframes_t section_frames;
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
			Metric metric (*meter, *tempo);
			
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
				
				if (bbt.ticks > Meter::ticks_per_beat/2) {
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
			
			//s cerr << bbt.bars << "|" << bbt.beats << "|" << bbt.ticks << endl;
			
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

TempoMap::Metric
TempoMap::metric_at (nframes_t frame) const
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
TempoMap::bbt_time (nframes_t frame, BBT_Time& bbt) const
{
	{
		Glib::RWLock::ReaderLock lm (lock);
		bbt_time_unlocked (frame, bbt);
	}
}

void
TempoMap::bbt_time_unlocked (nframes_t frame, BBT_Time& bbt) const
{
	bbt_time_with_metric (frame, bbt, metric_at (frame));
}

void
TempoMap::bbt_time_with_metric (nframes_t frame, BBT_Time& bbt, const Metric& metric) const
{
	nframes_t frame_diff;

	// cerr << "---- BBT time for " << frame << " using metric @ " << metric.frame() << " BBT " << metric.start() << endl;

	const double beats_per_bar = metric.meter().beats_per_bar();
	const double ticks_per_frame = metric.tempo().frames_per_beat (_frame_rate, metric.meter()) / Meter::ticks_per_beat;

	/* now compute how far beyond that point we actually are. */

	frame_diff = frame - metric.frame();

        bbt.ticks = metric.start().ticks + (uint32_t)round((double)frame_diff / ticks_per_frame);
        uint32_t xtra_beats = bbt.ticks / (uint32_t)Meter::ticks_per_beat;
        bbt.ticks %= (uint32_t)Meter::ticks_per_beat;

        bbt.beats = metric.start().beats + xtra_beats - 1; // correction for 1-based counting, see below for matching operation.
        bbt.bars = metric.start().bars + (uint32_t)floor((double)bbt.beats / beats_per_bar);
        bbt.beats = (uint32_t)fmod((double)bbt.beats, beats_per_bar);

        /* if we have a fractional number of beats per bar, we see if
           we're in the last beat (the fractional one).  if so, we
           round ticks appropriately and bump to the next bar. */
        double beat_fraction = beats_per_bar - floor(beats_per_bar);
        /* XXX one problem here is that I'm not sure how to handle
           fractional beats that don't evenly divide ticks_per_beat.
           If they aren't handled consistently, I would guess we'll
           continue to have strange discrepancies occuring.  Perhaps
           this will also behave badly in the case of meters like
           0.1/4, but I can't be bothered to test that.
        */
        uint32_t ticks_on_last_beat = (uint32_t)floor(Meter::ticks_per_beat * beat_fraction);
        if(bbt.beats > (uint32_t)floor(beats_per_bar) &&
           bbt.ticks >= ticks_on_last_beat) {
          bbt.ticks -= ticks_on_last_beat;
          bbt.beats = 0;
          bbt.bars++;
        }

        bbt.beats++; // correction for 1-based counting, see above for matching operation.

	// cerr << "-----\t RETURN " << bbt << endl;
}

nframes_t 
TempoMap::count_frames_between ( const BBT_Time& start, const BBT_Time& end) const
{
        /* for this to work with fractional measure types, start and end have to be "legal" BBT types, 
	   that means that the beats and ticks should be inside a bar
	*/

	nframes_t frames = 0;
	nframes_t start_frame = 0;
	nframes_t end_frame = 0;

	Metric m = metric_at (start);

	uint32_t bar_offset = start.bars - m.start().bars;

	double  beat_offset = bar_offset*m.meter().beats_per_bar() - (m.start().beats-1) + (start.beats -1) 
		+ start.ticks/Meter::ticks_per_beat;


	start_frame = m.frame() + (nframes_t) rint( beat_offset * m.tempo().frames_per_beat(_frame_rate, m.meter()));

    	m =  metric_at(end);

	bar_offset = end.bars - m.start().bars;

	beat_offset = bar_offset * m.meter().beats_per_bar() - (m.start().beats -1) + (end.beats - 1) 
		+ end.ticks/Meter::ticks_per_beat;

	end_frame = m.frame() + (nframes_t) rint(beat_offset * m.tempo().frames_per_beat(_frame_rate, m.meter()));

	frames = end_frame - start_frame;

	return frames;
	
}	

nframes_t 
TempoMap::count_frames_between_metrics (const Meter& meter, const Tempo& tempo, const BBT_Time& start, const BBT_Time& end) const
{
        /* this is used in timestamping the metrics by actually counting the beats */ 

	nframes_t frames = 0;
	uint32_t bar = start.bars;
	double beat = (double) start.beats;
	double beats_counted = 0;
	double beats_per_bar = 0;
	double beat_frames = 0;

	beats_per_bar = meter.beats_per_bar();
	beat_frames = tempo.frames_per_beat (_frame_rate,meter);

	frames = 0;

	while (bar < end.bars || (bar == end.bars && beat < end.beats)) {
		
		if (beat >= beats_per_bar) {
			beat = 1;
			++bar;
			++beats_counted;

			if (beat > beats_per_bar) {

				/* this is a fractional beat at the end of a fractional bar
				   so it should only count for the fraction 
				*/

				beats_counted -= (ceil(beats_per_bar) - beats_per_bar);
			}

		} else {
			++beat;
			++beats_counted;
		}
	}

	// cerr << "Counted " << beats_counted << " from " << start << " to " << end 
	// << " bpb were " << beats_per_bar 
	// << " fpb was " << beat_frames
	// << endl;
	
	frames = (nframes_t) floor (beats_counted * beat_frames);

	return frames;
	
}	

nframes_t 
TempoMap::frame_time (const BBT_Time& bbt) const
{
	BBT_Time start ; /* 1|1|0 */

	return  count_frames_between ( start, bbt);
}

nframes_t 
TempoMap::bbt_duration_at (nframes_t pos, const BBT_Time& bbt, int dir) const
{
	nframes_t frames = 0;

	BBT_Time when;
	bbt_time(pos, when);

	{
		Glib::RWLock::ReaderLock lm (lock);
		frames = bbt_duration_at_unlocked (when, bbt,dir);
	}

	return frames;
}

nframes_t 
TempoMap::bbt_duration_at_unlocked (const BBT_Time& when, const BBT_Time& bbt, int dir) const
{

	nframes_t frames = 0;

	double beats_per_bar;
	BBT_Time result;
	
	result.bars = max(1U, when.bars + dir * bbt.bars) ;
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

		while (result.beats >= (beats_per_bar + 1)) {
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
			if  (result.beats >= (beats_per_bar + 1)) {
				result.bars++;
				result.beats = 1;
				metric = metric_at(result); // maybe there is a meter change
				beats_per_bar = metric.meter().beats_per_bar();
			}
			ticks_at_beat= (uint32_t) ( result.beats == ceil(beats_per_bar) ?
				       (1 - (ceil(beats_per_bar) - beats_per_bar) ) * Meter::ticks_per_beat 
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
				b = (uint32_t) ceil(beats_per_bar) - b + when.beats ;
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
					result.bars = max(1U, result.bars-- ) ;
					metric = metric_at(result); // maybe there is a meter change
					beats_per_bar = metric.meter().beats_per_bar();
					result.beats = (uint32_t) ceil(beats_per_bar);
					ticks_at_beat = (uint32_t) ((1 - (ceil(beats_per_bar) - beats_per_bar)) * Meter::ticks_per_beat) ;
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



nframes_t
TempoMap::round_to_bar (nframes_t fr, int dir)
{
        {
	        Glib::RWLock::ReaderLock lm (lock);
		return round_to_type (fr, dir, Bar);
	}
}


nframes_t
TempoMap::round_to_beat (nframes_t fr, int dir)
{
        {
	        Glib::RWLock::ReaderLock lm (lock);
		return round_to_type (fr, dir, Beat);
	}
}

nframes_t

TempoMap::round_to_beat_subdivision (nframes_t fr, int sub_num)
{

	BBT_Time the_beat;
	uint32_t ticks_one_half_subdivisions_worth;
	uint32_t ticks_one_subdivisions_worth;

	bbt_time(fr, the_beat);

	ticks_one_subdivisions_worth = (uint32_t)Meter::ticks_per_beat / sub_num;
	ticks_one_half_subdivisions_worth = ticks_one_subdivisions_worth / 2;

	if (the_beat.ticks % ticks_one_subdivisions_worth > ticks_one_half_subdivisions_worth) {
	  uint32_t difference = ticks_one_subdivisions_worth - (the_beat.ticks % ticks_one_subdivisions_worth);
	  if (the_beat.ticks + difference >= (uint32_t)Meter::ticks_per_beat) {
	    the_beat.beats++;
	    the_beat.ticks += difference;
	    the_beat.ticks -= (uint32_t)Meter::ticks_per_beat;
	  } else {  
	    the_beat.ticks += difference;
	  }
	} else {
	  the_beat.ticks -= the_beat.ticks % ticks_one_subdivisions_worth;
	}

	return frame_time (the_beat);
}

nframes_t
TempoMap::round_to_type (nframes_t frame, int dir, BBTPointType type)
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
			} else if (metric.frame() < frame) {
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
			} else if (metric.frame() < frame) {
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
	
	/* 
	cerr << "for " << frame << " round to " << bbt << " using "
	     << metric.start()
	     << endl;
	*/
	return metric.frame() + count_frames_between (metric.start(), bbt);
}

TempoMap::BBTPointList *
TempoMap::get_points (nframes_t lower, nframes_t upper) const
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
	double beats_per_bar;
	double beat_frame;
	double beat_frames;
	double frames_per_bar;
	double delta_bars;
	double delta_beats;
	double dummy;
	nframes_t limit;

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

	beats_per_bar = meter->beats_per_bar ();
	frames_per_bar = meter->frames_per_bar (*tempo, _frame_rate);
	beat_frames = tempo->frames_per_beat (_frame_rate, *meter);
	
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
	delta_beats = modf(delta_bars, &dummy) * beats_per_bar;
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
					points->push_back (BBTPoint (*meter, *tempo,(nframes_t)rint(current), Bar, bar, 1));

				}
			}

			/* add some beats if we can */

			beat_frame = current;

			while (beat <= ceil( beats_per_bar) && beat_frame < limit) {
				if (beat_frame >= lower) {
					// cerr << "Add Beat at " << bar << '|' << beat << " @ " << beat_frame << endl;
					points->push_back (BBTPoint (*meter, *tempo, (nframes_t) rint(beat_frame), Beat, bar, beat));
				}
				beat_frame += beat_frames;
				current+= beat_frames;
			       
				beat++;
			}

			//  cerr << "out of beats, @ end ? " << (i == metrics->end()) << " out of bpb ? "
			// << (beat > ceil(beats_per_bar))
			// << endl;

			if (beat > ceil(beats_per_bar) || i != metrics->end()) {

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

				if (beat > ceil (beats_per_bar)) {
					/* next bar goes where the numbers suggest */
					current -=  beat_frames * (ceil(beats_per_bar)-beats_per_bar);
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

			beats_per_bar = meter->beats_per_bar ();
			frames_per_bar = meter->frames_per_bar (*tempo, _frame_rate);
			beat_frames = tempo->frames_per_beat (_frame_rate, *meter);
			
			++i;
		}

	} while (1);

	return points;
}	

const TempoSection&
TempoMap::tempo_section_at (nframes_t frame)
{
	Glib::RWLock::ReaderLock lm (lock);
	Metrics::iterator i;
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
TempoMap::tempo_at (nframes_t frame)
{
	Metric m (metric_at (frame));
	return m.tempo();
}


const Meter&
TempoMap::meter_at (nframes_t frame)
{
	Metric m (metric_at (frame));
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
TempoMap::set_state (const XMLNode& node)
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
	
	StateChanged (Change (0));

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
			o << "Meter @ " << *i << ' ' << m->beats_per_bar() << '/' << m->note_divisor() << " at " << m->start() << " frame= " << m->frame() 
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
