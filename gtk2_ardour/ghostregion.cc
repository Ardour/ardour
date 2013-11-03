/*
    Copyright (C) 2000-2007 Paul Davis

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

#include "evoral/Note.hpp"
#include "canvas/group.h"
#include "canvas/rectangle.h"
#include "canvas/wave_view.h"
#include "canvas/debug.h"

#include "ardour_ui.h"
#include "automation_time_axis.h"
#include "ghostregion.h"
#include "midi_streamview.h"
#include "midi_time_axis.h"
#include "rgb_macros.h"
#include "note.h"

using namespace std;
using namespace Editing;
using namespace ArdourCanvas;
using namespace ARDOUR;

PBD::Signal1<void,GhostRegion*> GhostRegion::CatchDeletion;

GhostRegion::GhostRegion (ArdourCanvas::Group* parent, TimeAxisView& tv, TimeAxisView& source_tv, double initial_pos)
	: trackview (tv)
	, source_trackview (source_tv)
{
	group = new ArdourCanvas::Group (parent);
	CANVAS_DEBUG_NAME (group, "ghost region");
	group->set_position (ArdourCanvas::Duple (initial_pos, 0));

	base_rect = new ArdourCanvas::Rectangle (group);
	CANVAS_DEBUG_NAME (base_rect, "ghost region rect");
	base_rect->set_x0 (0);
	base_rect->set_y0 (0);
	base_rect->set_y1 (trackview.current_height());
	base_rect->set_outline_what (0);

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
	CatchDeletion (this);
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
		base_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_GhostTrackBase());
		base_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_GhostTrackBase());
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

AudioGhostRegion::AudioGhostRegion(TimeAxisView& tv, TimeAxisView& source_tv, double initial_unit_pos)
	: GhostRegion(tv.ghost_group(), tv, source_tv, initial_unit_pos)
{

}

void
AudioGhostRegion::set_samples_per_pixel (double fpp)
{
	for (vector<WaveView*>::iterator i = waves.begin(); i != waves.end(); ++i) {
		(*i)->set_samples_per_pixel (fpp);
	}
}

void
AudioGhostRegion::set_height ()
{
	vector<WaveView*>::iterator i;
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
		fill_color = ARDOUR_UI::config()->get_canvasvar_GhostTrackWaveFill();
	}
	else {
		fill_color = source_track_color(200);
	}

	for (uint32_t n=0; n < waves.size(); ++n) {
		waves[n]->set_outline_color (ARDOUR_UI::config()->get_canvasvar_GhostTrackWave());
		waves[n]->set_fill_color (fill_color);
		waves[n]->set_clip_color (ARDOUR_UI::config()->get_canvasvar_GhostTrackWaveClip());
		waves[n]->set_zero_color (ARDOUR_UI::config()->get_canvasvar_GhostTrackZeroLine());
	}
}

/** The general constructor; called when the destination timeaxisview doesn't have
 *  a midistreamview.
 *
 *  @param tv TimeAxisView that this ghost region is on.
 *  @param source_tv TimeAxisView that we are the ghost for.
 */
MidiGhostRegion::MidiGhostRegion(TimeAxisView& tv, TimeAxisView& source_tv, double initial_unit_pos)
	: GhostRegion(tv.ghost_group(), tv, source_tv, initial_unit_pos)
	, _optimization_iterator (events.end ())
{
	base_rect->lower_to_bottom();
	update_range ();

	midi_view()->NoteRangeChanged.connect (sigc::mem_fun (*this, &MidiGhostRegion::update_range));
}

/**
 *  @param msv MidiStreamView that this ghost region is on.
 *  @param source_tv TimeAxisView that we are the ghost for.
 */
MidiGhostRegion::MidiGhostRegion(MidiStreamView& msv, TimeAxisView& source_tv, double initial_unit_pos)
	: GhostRegion(msv.midi_underlay_group, msv.trackview(), source_tv, initial_unit_pos)
	, _optimization_iterator (events.end ())
{
	base_rect->lower_to_bottom();
	update_range ();

	midi_view()->NoteRangeChanged.connect (sigc::mem_fun (*this, &MidiGhostRegion::update_range));
}

MidiGhostRegion::~MidiGhostRegion()
{
	clear_events ();
}

MidiGhostRegion::GhostEvent::GhostEvent (NoteBase* e, ArdourCanvas::Group* g)
	: event (e)
{
	rect = new ArdourCanvas::Rectangle (g, ArdourCanvas::Rect (e->x0(), e->y0(), e->x1(), e->y1()));
	CANVAS_DEBUG_NAME (rect, "ghost note rect");
}

MidiGhostRegion::GhostEvent::~GhostEvent ()
{
	/* event is not ours to delete */
	delete rect;
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
	update_range();
}

void
MidiGhostRegion::set_colors()
{
	guint fill = source_track_color(200);

	GhostRegion::set_colors();

	for (EventList::iterator it = events.begin(); it != events.end(); ++it) {
		(*it)->rect->set_fill_color (fill);
		(*it)->rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_GhostTrackMidiOutline());
	}
}

void
MidiGhostRegion::update_range ()
{
	MidiStreamView* mv = midi_view();

	if (!mv) {
		return;
	}

	double const h = trackview.current_height() / double (mv->contents_note_range ());

	for (EventList::iterator it = events.begin(); it != events.end(); ++it) {
		uint8_t const note_num = (*it)->event->note()->note();

		if (note_num < mv->lowest_note() || note_num > mv->highest_note()) {
			(*it)->rect->hide();
		} else {
			(*it)->rect->show();
			double const y = trackview.current_height() - (note_num + 1 - mv->lowest_note()) * h + 1;
			(*it)->rect->set_y0 (y);
			(*it)->rect->set_y1 (y + h);
		}
	}
}

void
MidiGhostRegion::add_note (NoteBase* n)
{
	GhostEvent* event = new GhostEvent (n, group);
	events.push_back (event);

	event->rect->set_fill_color (source_track_color(200));
	event->rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_GhostTrackMidiOutline());

	MidiStreamView* mv = midi_view();

	if (mv) {
		const uint8_t note_num = n->note()->note();

		if (note_num < mv->lowest_note() || note_num > mv->highest_note()) {
			event->rect->hide();
		} else {
			const double y = mv->note_to_y(note_num);
			event->rect->set_y0 (y);
			event->rect->set_y1 (y + mv->note_height());
		}
	}
}

void
MidiGhostRegion::clear_events()
{
	for (EventList::iterator it = events.begin(); it != events.end(); ++it) {
		delete *it;
	}

	events.clear();
	_optimization_iterator = events.end ();
}

/** Update the x positions of our representation of a parent's note.
 *  @param parent The CanvasNote from the parent MidiRegionView.
 */
void
MidiGhostRegion::update_note (NoteBase* parent)
{
	GhostEvent* ev = find_event (parent);
	if (!ev) {
		return;
	}

	double const x1 = parent->x0 ();
	double const x2 = parent->x1 ();
	ev->rect->set_x0 (x1);
	ev->rect->set_x1 (x2);
}

void
MidiGhostRegion::remove_note (NoteBase* note)
{
	GhostEvent* ev = find_event (note);
	if (!ev) {
		return;
	}

	events.remove (ev);
	delete ev;
	_optimization_iterator = events.end ();
}

/** Given a note in our parent region (ie the actual MidiRegionView), find our
 *  representation of it.
 *  @return Our Event, or 0 if not found.
 */

MidiGhostRegion::GhostEvent *
MidiGhostRegion::find_event (NoteBase* parent)
{
	/* we are using _optimization_iterator to speed up the common case where a caller
	   is going through our notes in order.
	*/

	if (_optimization_iterator != events.end()) {
		++_optimization_iterator;
	}

	if (_optimization_iterator != events.end() && (*_optimization_iterator)->event == parent) {
		return *_optimization_iterator;
	}

	for (_optimization_iterator = events.begin(); _optimization_iterator != events.end(); ++_optimization_iterator) {
		if ((*_optimization_iterator)->event == parent) {
			return *_optimization_iterator;
		}
	}

	return 0;
}
