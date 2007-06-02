/*
    Copyright (C) 2001-2006 Paul Davis 

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

#include <cmath>
#include <cassert>
#include <algorithm>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include <ardour/playlist.h>
#include <ardour/midi_region.h>
#include <ardour/midi_source.h>
#include <ardour/midi_diskstream.h>
#include <ardour/midi_events.h>

#include "streamview.h"
#include "midi_region_view.h"
#include "midi_time_axis.h"
#include "simplerect.h"
#include "simpleline.h"
#include "public_editor.h"
#include "ghostregion.h"
#include "midi_time_axis.h"
#include "utils.h"
#include "rgb_macros.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;

MidiRegionView::MidiRegionView (ArdourCanvas::Group *parent, RouteTimeAxisView &tv, boost::shared_ptr<MidiRegion> r, double spu,
				  Gdk::Color& basic_color)
	: RegionView (parent, tv, r, spu, basic_color)
	, _active_notes(0)
{
}

MidiRegionView::MidiRegionView (ArdourCanvas::Group *parent, RouteTimeAxisView &tv, boost::shared_ptr<MidiRegion> r, double spu, 
				  Gdk::Color& basic_color, TimeAxisViewItem::Visibility visibility)
	: RegionView (parent, tv, r, spu, basic_color, visibility)
	, _active_notes(0)
{
}

void
MidiRegionView::init (Gdk::Color& basic_color, bool wfd)
{
	// FIXME: Some redundancy here with RegionView::init.  Need to figure out
	// where order is important and where it isn't...
	
	// FIXME
	RegionView::init(basic_color, /*wfd*/false);

	compute_colors (basic_color);

	reset_width_dependent_items ((double) _region->length() / samples_per_unit);

	set_y_position_and_height (0, trackview.height);

	region_muted ();
	region_resized (BoundsChanged);
	region_locked ();

	_region->StateChanged.connect (mem_fun(*this, &MidiRegionView::region_changed));

	set_colors ();
}

MidiRegionView::~MidiRegionView ()
{
	in_destructor = true;
	end_write();

	RegionViewGoingAway (this); /* EMIT_SIGNAL */
}

boost::shared_ptr<ARDOUR::MidiRegion>
MidiRegionView::midi_region() const
{
	// "Guaranteed" to succeed...
	return boost::dynamic_pointer_cast<MidiRegion>(_region);
}

void
MidiRegionView::show_region_editor ()
{
	cerr << "No MIDI region editor." << endl;
}

GhostRegion*
MidiRegionView::add_ghost (AutomationTimeAxisView& atv)
{
	throw; // FIXME
	return NULL;
}


/** Begin tracking note state for successive calls to add_event
 */
void
MidiRegionView::begin_write()
{
	_active_notes = new ArdourCanvas::SimpleRect*[128];
	for (unsigned i=0; i < 128; ++i)
		_active_notes[i] = NULL;
}


/** Destroy note state for add_event
 */
void
MidiRegionView::end_write()
{
	delete[] _active_notes;
	_active_notes = NULL;
}


void
MidiRegionView::add_event (const MidiEvent& ev)
{
	/*printf("Event, time = %u, size = %zu, data = ",
	  ev.time, ev.size);
	  for (size_t i=0; i < ev.size; ++i) {
	  printf("%X ", ev.buffer[i]);
	  }
	  printf("\n");*/
	double y1 = trackview.height / 2.0;
	if ((ev.buffer[0] & 0xF0) == MIDI_CMD_NOTE_ON) {
		const Byte& note = ev.buffer[1];
		y1 = trackview.height - ((trackview.height / 127.0) * note);

		ArdourCanvas::SimpleRect * ev_rect = new Gnome::Canvas::SimpleRect(
				*(ArdourCanvas::Group*)get_canvas_group());
		ev_rect->property_x1() = trackview.editor.frame_to_pixel (
				ev.time);
		ev_rect->property_y1() = y1;
		ev_rect->property_x2() = trackview.editor.frame_to_pixel (
				_region->length());
		ev_rect->property_y2() = y1 + (trackview.height / 127.0);
		ev_rect->property_outline_color_rgba() = 0xFFFFFFAA;
		/* outline all but right edge */
		ev_rect->property_outline_what() = (guint32) (0x1 & 0x4 & 0x8);
		ev_rect->property_fill_color_rgba() = 0xFFFFFF66;

		_events.push_back(ev_rect);
		if (_active_notes)
			_active_notes[note] = ev_rect;

	} else if ((ev.buffer[0] & 0xF0) == MIDI_CMD_NOTE_OFF) {
		const Byte& note = ev.buffer[1];
		if (_active_notes && _active_notes[note]) {
			_active_notes[note]->property_x2() = trackview.editor.frame_to_pixel(ev.time);
			_active_notes[note]->property_outline_what() = (guint32) 0xF; // all edges
			_active_notes[note] = NULL;
		}
	}

}


/** Extend active notes to rightmost edge of region (if length is changed)
 */
void
MidiRegionView::extend_active_notes()
{
	if (!_active_notes)
		return;

	for (unsigned i=0; i < 128; ++i)
		if (_active_notes[i])
			_active_notes[i]->property_x2() = trackview.editor.frame_to_pixel(_region->length());
}



