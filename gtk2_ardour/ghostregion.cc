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
#include "ardour_ui.h"
#include "automation_time_axis.h"
#include "canvas-hit.h"
#include "canvas-note.h"
#include "ghostregion.h"
#include "midi_streamview.h"
#include "midi_time_axis.h"
#include "rgb_macros.h"
#include "simplerect.h"
#include "waveview.h"

using namespace std;
using namespace Editing;
using namespace ArdourCanvas;
using namespace ARDOUR;

GhostRegion::GhostRegion (ArdourCanvas::Group* parent, TimeAxisViewPtr tv, TimeAxisViewPtr source_tv, double initial_pos)
	: trackview (tv)
	, source_trackview (source_tv)
{
	group = new ArdourCanvas::Group (*parent);
	group->property_x() = initial_pos;
	group->property_y() = 0.0;

	base_rect = new ArdourCanvas::SimpleRect (*group);
	base_rect->property_x1() = (double) 0.0;
	base_rect->property_y1() = (double) 0.0;
	base_rect->property_y2() = (double) trackview->current_height();
	base_rect->property_outline_what() = (guint32) 0;

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
	GoingAway (this);
	delete base_rect;
	delete group;
}

void
GhostRegion::set_duration (double units)
{
	base_rect->property_x2() = units;
}

void
GhostRegion::set_height ()
{
	base_rect->property_y2() = (double) trackview->current_height();
}

void
GhostRegion::set_colors ()
{
	if (is_automation_ghost()) {
		base_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_GhostTrackBase.get();
		base_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_GhostTrackBase.get();
	}
}

guint
GhostRegion::source_track_color(unsigned char alpha) {
	Gdk::Color color = source_trackview->color();
	unsigned char r,g,b ;
	r = color.get_red()/256;
	g = color.get_green()/256;
	b = color.get_blue()/256;
	return RGBA_TO_UINT(r, g, b, alpha);
}

bool
GhostRegion::is_automation_ghost() {
	return (boost::dynamic_pointer_cast<AutomationTimeAxisViewPtr> (trackview)) != 0;
}

AudioGhostRegion::AudioGhostRegion(TimeAxisViewPtr tv, TimeAxisViewPtr source_tv, double initial_unit_pos)
	: GhostRegion(tv->ghost_group(), tv, source_tv, initial_unit_pos) {
}

void
AudioGhostRegion::set_samples_per_unit (double spu)
{
	for (vector<WaveView*>::iterator i = waves.begin(); i != waves.end(); ++i) {
		(*i)->property_samples_per_unit() = spu;
	}		
}

void
AudioGhostRegion::set_height ()
{
	gdouble ht;
	vector<WaveView*>::iterator i;
	uint32_t n;

	GhostRegion::set_height();

	ht = ((trackview->current_height()) / (double) waves.size());
	
	for (n = 0, i = waves.begin(); i != waves.end(); ++i, ++n) {
		gdouble const yoff = n * ht;
		(*i)->property_height() = ht;
		(*i)->property_y() = yoff;
	}
}

void
AudioGhostRegion::set_colors ()
{
	GhostRegion::set_colors();
	guint fill_color;

	if (is_automation_ghost()) {
		fill_color = ARDOUR_UI::config()->canvasvar_GhostTrackWaveFill.get();
	}
	else {
		fill_color = source_track_color(200);
	}

	for (uint32_t n=0; n < waves.size(); ++n) {
		waves[n]->property_wave_color() = ARDOUR_UI::config()->canvasvar_GhostTrackWave.get();
		waves[n]->property_fill_color() = fill_color;
		waves[n]->property_clip_color() = ARDOUR_UI::config()->canvasvar_GhostTrackWaveClip.get();
		waves[n]->property_zero_color() = ARDOUR_UI::config()->canvasvar_GhostTrackZeroLine.get();
	}
}

/*
 * This is the general constructor, and is called when the destination timeaxisview doesn't have
 * a midistreamview. But what to do when positioning the midi ghost here? For example, there is
 * no range controller in these tracks. maybe show the whole range.
 */
MidiGhostRegion::MidiGhostRegion(TimeAxisViewPtr tv, TimeAxisViewPtr source_tv, double initial_unit_pos)
	: GhostRegion (tv->ghost_group(), tv, source_tv, initial_unit_pos)
{

	base_rect->lower_to_bottom();
}

MidiGhostRegion::MidiGhostRegion(MidiStreamView& msv, TimeAxisViewPtr source_tv, double initial_unit_pos)
	: GhostRegion(msv.midi_underlay_group, msv.trackview(), source_tv, initial_unit_pos)
{
	base_rect->lower_to_bottom();	
}

MidiGhostRegion::~MidiGhostRegion()
{
	//clear_events();
}

MidiGhostRegion::Event::Event(ArdourCanvas::CanvasNoteEvent* e)
	: event(e)
{
}

MidiGhostRegion::Note::Note(ArdourCanvas::CanvasNote* n, ArdourCanvas::Group* g)
	: Event(n)
{
	rect = new ArdourCanvas::SimpleRect(*g, n->x1(), n->y1(), n->x2(), n->y2());
}

MidiGhostRegion::Note::~Note()
{
	//delete rect;
}

void
MidiGhostRegion::Note::x_changed()
{
	rect->property_x1() = event->x1();
	rect->property_x2() = event->x2();
}

MidiGhostRegion::Hit::Hit(ArdourCanvas::CanvasHit* h, ArdourCanvas::Group*)
	: Event(h)
{
	cerr << "Hit ghost item does not work yet" << endl;
}

MidiGhostRegion::Hit::~Hit()
{
}

void
MidiGhostRegion::Hit::x_changed()
{
}

void
MidiGhostRegion::set_samples_per_unit (double spu)
{
}

MidiStreamView*
MidiGhostRegion::midi_view()
{
	MidiTimeAxisViewPtr mtv;

	if ((mtv = boost::dynamic_pointer_cast<MidiTimeAxisView> (trackview)) != 0) {
		return mtv->midi_view();
	}
	else {
		return 0;
	}
}

void
MidiGhostRegion::set_height()
{
	GhostRegion::set_height();
	update_range();
}

void
MidiGhostRegion::set_colors()
{
	MidiGhostRegion::Note* note;
	guint fill = source_track_color(200);

	GhostRegion::set_colors();

	for (EventList::iterator it = events.begin(); it != events.end(); ++it) {
		if ((note = dynamic_cast<MidiGhostRegion::Note*>(*it)) != 0) {
			note->rect->property_fill_color_rgba() = fill;
			note->rect->property_outline_color_rgba() =  ARDOUR_UI::config()->canvasvar_GhostTrackMidiOutline.get();
		}
	}
}

void
MidiGhostRegion::update_range()
{
	MidiStreamView* mv = midi_view();

	if (!mv) {
		return;
	}

	MidiGhostRegion::Note* note;
	uint8_t note_num;
	double y;
	
	for (EventList::iterator it = events.begin(); it != events.end(); ++it) {
		if ((note = dynamic_cast<MidiGhostRegion::Note*>(*it)) != 0) {
			note_num = note->event->note()->note();

			if (note_num < mv->lowest_note() || note_num > mv->highest_note()) {
				note->rect->hide();
			}
			else {
				note->rect->show();
				y = mv->note_to_y(note_num);
				note->rect->property_y1() = y;
				note->rect->property_y2() = y + mv->note_height();
			}
		}
	}
}

void
MidiGhostRegion::add_note(ArdourCanvas::CanvasNote* n)
{
	Note* note = new Note(n, group);
	events.push_back(note);

	note->rect->property_fill_color_rgba() = source_track_color(200);
	note->rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_GhostTrackMidiOutline.get();

	MidiStreamView* mv = midi_view();

	if (mv) {
		const uint8_t note_num = n->note()->note();

		if (note_num < mv->lowest_note() || note_num > mv->highest_note()) {
			note->rect->hide();
		} else {
			const double y = mv->note_to_y(note_num);
			note->rect->property_y1() = y;
			note->rect->property_y2() = y + mv->note_height();
		}
	}
}

void
MidiGhostRegion::add_hit(ArdourCanvas::CanvasHit* h)
{
	//events.push_back(new Hit(h, group));
}

void
MidiGhostRegion::clear_events()
{
	for (EventList::iterator it = events.begin(); it != events.end(); ++it) {
		delete *it;
	}

	events.clear();
}

