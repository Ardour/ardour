/*
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2008-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "evoral/Note.h"

#include "ardour/parameter_descriptor.h"

#include "canvas/container.h"
#include "canvas/polygon.h"
#include "canvas/rectangle.h"
#include "canvas/debug.h"

#include "waveview/wave_view.h"

#include "automation_time_axis.h"
#include "ghostregion.h"
#include "midi_streamview.h"
#include "midi_time_axis.h"
#include "region_view.h"
#include "midi_region_view.h"
#include "rgb_macros.h"
#include "note.h"
#include "hit.h"
#include "ui_config.h"

using namespace std;
using namespace Editing;
using namespace ARDOUR;
using ArdourCanvas::Duple;

GhostRegion::GhostRegion (RegionView& rv,
                          ArdourCanvas::Container* parent,
                          TimeAxisView& tv,
                          TimeAxisView& source_tv,
                          double initial_pos)
	: parent_rv(rv)
	, trackview(tv)
	, source_trackview(source_tv)
{
	group = new ArdourCanvas::Container (parent);
	CANVAS_DEBUG_NAME (group, "ghost region");
	group->set_position (ArdourCanvas::Duple (initial_pos, 0));

	base_rect = new ArdourCanvas::Rectangle (group);
	CANVAS_DEBUG_NAME (base_rect, "ghost region rect");
	base_rect->set_x0 (0);
	base_rect->set_y0 (1.0);
	base_rect->set_y1 (trackview.current_height());
	base_rect->set_outline (false);

	if (!is_automation_ghost()) {
		base_rect->hide();
	}

	GhostRegion::set_colors();

	/* the parent group of a ghostregion is a dedicated group for ghosts,
	   so the new ghost would want to get to the top of that group*/
	group->raise_to_top ();
}

GhostRegion::~GhostRegion ()
{
	parent_rv.remove_ghost(this);
	trackview.erase_ghost(this);
	delete base_rect;
	delete group;
}

void
GhostRegion::set_duration (double units)
{
	base_rect->set_x1 (units);
}

void
GhostRegion::set_height ()
{
	base_rect->set_y1 (trackview.current_height());
}

void
GhostRegion::set_colors ()
{
	if (is_automation_ghost()) {
		base_rect->set_fill_color (UIConfiguration::instance().color_mod ("ghost track base", "ghost track base"));
	}
}

guint
GhostRegion::source_track_color(unsigned char alpha)
{
	Gdk::Color color = source_trackview.color();
	return RGBA_TO_UINT (color.get_red() / 256, color.get_green() / 256, color.get_blue() / 256, alpha);
}

bool
GhostRegion::is_automation_ghost()
{
	return (dynamic_cast<AutomationTimeAxisView*>(&trackview)) != 0;
}

AudioGhostRegion::AudioGhostRegion (RegionView& rv,
                                    TimeAxisView& tv,
                                    TimeAxisView& source_tv,
                                    double initial_unit_pos)
	: GhostRegion(rv, tv.ghost_group(), tv, source_tv, initial_unit_pos)
{

}

void
AudioGhostRegion::set_samples_per_pixel (double fpp)
{
	for (vector<ArdourWaveView::WaveView*>::iterator i = waves.begin(); i != waves.end(); ++i) {
		(*i)->set_samples_per_pixel (fpp);
	}
}

void
AudioGhostRegion::set_height ()
{
	vector<ArdourWaveView::WaveView*>::iterator i;
	uint32_t n;

	GhostRegion::set_height();

	double const ht = ((trackview.current_height()) / (double) waves.size());

	for (n = 0, i = waves.begin(); i != waves.end(); ++i, ++n) {
		(*i)->set_height (ht);
		(*i)->set_y_position (n * ht);
	}
}

void
AudioGhostRegion::set_colors ()
{
	GhostRegion::set_colors();
	guint fill_color;

	if (is_automation_ghost()) {
		fill_color = UIConfiguration::instance().color ("ghost track wave fill");
	}
	else {
		fill_color = source_track_color(200);
	}

	for (uint32_t n=0; n < waves.size(); ++n) {
		waves[n]->set_outline_color (UIConfiguration::instance().color ("ghost track wave"));
		waves[n]->set_fill_color (fill_color);
		waves[n]->set_clip_color (UIConfiguration::instance().color ("ghost track wave clip"));
		waves[n]->set_zero_color (UIConfiguration::instance().color ("ghost track zero line"));
	}
}

/** The general constructor; called when the destination timeaxisview doesn't have
 *  a midistreamview.
 *
 *  @param rv The parent RegionView that is being ghosted.
 *  @param tv TimeAxisView that this ghost region is on.
 *  @param source_tv TimeAxisView that we are the ghost for.
 */
MidiGhostRegion::MidiGhostRegion(MidiRegionView& rv,
                                 TimeAxisView& tv,
                                 TimeAxisView& source_tv,
                                 double initial_unit_pos)
	: GhostRegion(rv, tv.ghost_group(), tv, source_tv, initial_unit_pos)
	, _note_group (new ArdourCanvas::Container (group))
	,  parent_mrv (rv)
	, _optimization_iterator(events.end())
{
	_outline = UIConfiguration::instance().color ("ghost track midi outline");

	base_rect->lower_to_bottom();
}

/**
 *  @param rv The parent RegionView being ghosted.
 *  @param msv MidiStreamView that this ghost region is on.
 *  @param source_tv TimeAxisView that we are the ghost for.
 */
MidiGhostRegion::MidiGhostRegion(MidiRegionView& rv,
                                 MidiStreamView& msv,
                                 TimeAxisView& source_tv,
                                 double initial_unit_pos)
	: GhostRegion (rv,
	               msv.midi_underlay_group,
	               msv.trackview(),
	               source_tv,
	               initial_unit_pos)
	, _note_group (new ArdourCanvas::Container (group))
	, parent_mrv (rv)
	, _optimization_iterator(events.end())
{
	_outline = UIConfiguration::instance().color ("ghost track midi outline");

	base_rect->lower_to_bottom();
}

MidiGhostRegion::~MidiGhostRegion()
{
	clear_events ();
	delete _note_group;
}

MidiGhostRegion::GhostEvent::GhostEvent (NoteBase* e, ArdourCanvas::Container* g)
	: event (e)
{

	if (dynamic_cast<Note*>(e)) {
		item = new ArdourCanvas::Rectangle(
			g, ArdourCanvas::Rect(e->x0(), e->y0(), e->x1(), e->y1()));
		is_hit = false;
	} else {
		Hit* hit = dynamic_cast<Hit*>(e);
		if (!hit) {
			return;
		}
		ArdourCanvas::Polygon* poly = new ArdourCanvas::Polygon(g);
		poly->set(Hit::points(e->y1() - e->y0()));
		poly->set_position(hit->position());
		item = poly;
		is_hit = true;
	}

	CANVAS_DEBUG_NAME (item, "ghost note item");
}

MidiGhostRegion::GhostEvent::~GhostEvent ()
{
	/* event is not ours to delete */
	delete item;
}

void
MidiGhostRegion::set_samples_per_pixel (double /*spu*/)
{
}

/** @return MidiStreamView that we are providing a ghost for */
MidiStreamView*
MidiGhostRegion::midi_view ()
{
	StreamView* sv = source_trackview.view ();
	assert (sv);
	MidiStreamView* msv = dynamic_cast<MidiStreamView*> (sv);
	assert (msv);

	return msv;
}

void
MidiGhostRegion::set_height ()
{
	GhostRegion::set_height();
	update_contents_height ();
}

void
MidiGhostRegion::set_colors()
{
	GhostRegion::set_colors();
	_outline = UIConfiguration::instance().color ("ghost track midi outline");

	for (EventList::iterator it = events.begin(); it != events.end(); ++it) {
		it->second->item->set_fill_color (UIConfiguration::instance().color_mod((*it).second->event->base_color(), "ghost track midi fill"));
		it->second->item->set_outline_color (_outline);
	}
}

static double
note_height(TimeAxisView& trackview, MidiStreamView* mv)
{
	const double tv_height  = trackview.current_height();
	const double note_range = mv->contents_note_range();

	return std::max(1.0, floor(tv_height / note_range - 1.0));
}

static double
note_y(TimeAxisView& trackview, MidiStreamView* mv, uint8_t note_num)
{
	const double tv_height  = trackview.current_height();
	const double note_range = mv->contents_note_range();
	const double s          = tv_height / note_range;

	return tv_height - (note_num + 1 - mv->lowest_note()) * s;
}

void
MidiGhostRegion::update_contents_height ()
{
	MidiStreamView* mv = midi_view();

	if (!mv) {
		return;
	}

	double const h = note_height(trackview, mv);

	for (EventList::iterator it = events.begin(); it != events.end(); ++it) {
		uint8_t const note_num = it->second->event->note()->note();

		double const y = note_y(trackview, mv, note_num);

		if (!it->second->is_hit) {
			_tmp_rect = static_cast<ArdourCanvas::Rectangle*>(it->second->item);
			_tmp_rect->set (ArdourCanvas::Rect (_tmp_rect->x0(), y, _tmp_rect->x1(), y + h));
		} else {
			_tmp_poly = static_cast<ArdourCanvas::Polygon*>(it->second->item);
			ArdourCanvas::Duple position = _tmp_poly->position();
			position.y = y;
			_tmp_poly->set_position(position);
			_tmp_poly->set(Hit::points(h));
		}
	}
}

void
MidiGhostRegion::add_note (NoteBase* n)
{
	GhostEvent* event = new GhostEvent (n, _note_group);
	events.insert (make_pair (n->note(), event));

	event->item->set_fill_color (UIConfiguration::instance().color_mod(n->base_color(), "ghost track midi fill"));
	event->item->set_outline_color (_outline);

	MidiStreamView* mv = midi_view();

	if (mv) {

		if (!n->item()->visible()) {
			event->item->hide();
		} else {
			uint8_t const note_num = n->note()->note();
			double const  h        = note_height(trackview, mv);
			double const  y        = note_y(trackview, mv, note_num);
			if (!event->is_hit) {
				_tmp_rect = static_cast<ArdourCanvas::Rectangle*>(event->item);
				_tmp_rect->set (ArdourCanvas::Rect (_tmp_rect->x0(), y, _tmp_rect->x1(), y + h));
			} else {
				_tmp_poly = static_cast<ArdourCanvas::Polygon*>(event->item);
				Duple position = _tmp_poly->position();
				position.y = y;
				_tmp_poly->set_position(position);
				_tmp_poly->set(Hit::points(h));
			}
		}
	}
}

void
MidiGhostRegion::clear_events()
{
	_note_group->clear (true);
	events.clear ();
	_optimization_iterator = events.end();
}

/** Update the  positions of our representation of a note.
 *  @param ev The GhostEvent from the parent MidiRegionView.
 */
void
MidiGhostRegion::update_note (GhostEvent* ev)
{
	MidiStreamView* mv = midi_view();

	if (!mv) {
		return;
	}

	_tmp_rect = static_cast<ArdourCanvas::Rectangle*>(ev->item);

	uint8_t const note_num = ev->event->note()->note();
	double const y = note_y(trackview, mv, note_num);
	double const h = note_height(trackview, mv);

	_tmp_rect->set (ArdourCanvas::Rect (ev->event->x0(), y, ev->event->x1(), y + h));
}

/** Update the positions of our representation of a parent's hit.
 *  @param ev The GhostEvent from the parent MidiRegionView.
 */
void
MidiGhostRegion::update_hit (GhostEvent* ev)
{
	MidiStreamView* mv = midi_view();

	if (!mv) {
		return;
	}

	_tmp_poly = static_cast<ArdourCanvas::Polygon*>(ev->item);

	uint8_t const note_num = ev->event->note()->note();
	double const h = note_height(trackview, mv);
	double const y = note_y(trackview, mv, note_num);

	ArdourCanvas::Duple ppos = ev->item->position();
	ArdourCanvas::Duple gpos = _tmp_poly->position();
	gpos.x = ppos.x;
	gpos.y = y;

	_tmp_poly->set_position(gpos);
	_tmp_poly->set(Hit::points(h));
}

void
MidiGhostRegion::remove_note (NoteBase* note)
{
	EventList::iterator f = events.find (note->note());
	if (f == events.end()) {
		return;
	}

	delete f->second;
	events.erase (f);

	_optimization_iterator = events.end ();
}
void
MidiGhostRegion::redisplay_model ()
{
	/* we rely on the parent MRV having removed notes not in the model */
	for (EventList::iterator i = events.begin(); i != events.end(); ) {

		boost::shared_ptr<NoteType> note = i->first;
		GhostEvent* cne = i->second;
		const bool visible = (note->note() >= parent_mrv._current_range_min) &&
			(note->note() <= parent_mrv._current_range_max);

		if (visible) {
			if (cne->is_hit) {
				update_hit (cne);
			} else {
				update_note (cne);
			}
			cne->item->show ();
		} else {
			cne->item->hide ();
		}

		++i;
	}
}

/** Given a note in our parent region (ie the actual MidiRegionView), find our
 *  representation of it.
 *  @return Our Event, or 0 if not found.
 */
MidiGhostRegion::GhostEvent *
MidiGhostRegion::find_event (boost::shared_ptr<NoteType> parent)
{
	/* we are using _optimization_iterator to speed up the common case where a caller
	   is going through our notes in order.
	*/

	if (_optimization_iterator != events.end()) {
		++_optimization_iterator;
		if (_optimization_iterator != events.end() && _optimization_iterator->first == parent) {
			return _optimization_iterator->second;
		}
	}

	_optimization_iterator = events.find (parent);
	if (_optimization_iterator != events.end()) {
		return _optimization_iterator->second;
	}

	return 0;
}
