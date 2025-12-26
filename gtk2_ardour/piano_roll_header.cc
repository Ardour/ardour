/*
 * Copyright (C) 2008-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#include "ardour/midi_track.h"
#include "evoral/midi_events.h"
#include <iostream>

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/gtk_ui.h"

#include "editing.h"
#include "midi_streamview.h"
#include "midi_time_axis.h"
#include "piano_roll_header.h"
#include "public_editor.h"
#include "ui_config.h"
#include "midi++/midnam_patch.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtkmm2ext;

PianoRollHeader::PianoRollHeader (MidiViewBackground& bg)
	: PianoRollHeaderBase (bg)
 {
	 stream_view = dynamic_cast<MidiStreamView*> (&bg);
	 assert (stream_view);

	 alloc_layouts (get_pango_context ());

	_adj.set_lower(0);
	_adj.set_upper(127);

	/* set minimum view range to one octave */
	//set_min_page_size(12);

	//_adj = v->note_range_adjustment;

	Gtkmm2ext::UI::instance()->set_tip (*this, string_compose (_("Left-button to play a note, left-button-drag to play a series of notes\n"
	                                                             "%1-left-button to select or extend selection to all notes with this pitch\n"),
	                                                           Keyboard::tertiary_modifier_name()));
	add_events (Gdk::BUTTON_PRESS_MASK |
	            Gdk::BUTTON_RELEASE_MASK |
	            Gdk::POINTER_MOTION_MASK |
	            Gdk::ENTER_NOTIFY_MASK |
	            Gdk::LEAVE_NOTIFY_MASK |
	            Gdk::SCROLL_MASK);
}

bool
PianoRollHeader::on_scroll_event (GdkEventScroll* ev)
{
	return scroll_handler (ev);
}

void
PianoRollHeader::redraw ()
{
	queue_draw ();
}

void
PianoRollHeader::redraw (double x, double y, double w, double h)
{
	queue_draw_area (x, y, w, h);
}

bool
PianoRollHeader::on_expose_event (GdkEventExpose* ev)
{
	ArdourCanvas::Rect rect (ev->area.x, ev->area.y, ev->area.x + ev->area.width, ev->area.y + ev->area.height);
	ArdourCanvas::Rect self (0., 0., get_width(), get_height());

	render (self, rect, get_window()->create_cairo_context());

	return true;
}
bool
PianoRollHeader::on_motion_notify_event (GdkEventMotion* ev)
{
	return motion_handler (ev);
}

bool
PianoRollHeader::on_button_press_event (GdkEventButton* ev)
{
	return button_press_handler (ev);
}


bool
PianoRollHeader::on_button_release_event (GdkEventButton* ev)
{
	return button_release_handler (ev);
}

bool
PianoRollHeader::on_enter_notify_event (GdkEventCrossing* ev)
{
	return enter_handler (ev);
}

bool
PianoRollHeader::on_leave_notify_event (GdkEventCrossing* ev)
{
	return leave_handler (ev);
}

void
PianoRollHeader::on_size_request (Gtk::Requisition* r)
{
	if (show_scroomer()) {
		_scroomer_size = 60.f * UIConfiguration::instance().get_ui_scale();
	} else {
		_scroomer_size = 20.f * UIConfiguration::instance().get_ui_scale();
	}

	r->width = _scroomer_size + 20.f;
}

double
PianoRollHeader::height() const
{
	return get_height();
}

double
PianoRollHeader::width() const
{
	return get_width();
}

Glib::RefPtr<Gdk::Window>
PianoRollHeader::cursor_window()
{
	return get_window ();
}

void
PianoRollHeader::instrument_info_change ()
{
	PianoRollHeaderBase::instrument_info_change ();

	/* need this to get editor to potentially sync all
	   track header widths if our piano roll header changes
	   width.
	*/

	stream_view->trackview().stripable()->gui_changed ("visible_tracks", (void *) 0); /* EMIT SIGNAL */
}

std::shared_ptr<ARDOUR::MidiTrack>
PianoRollHeader::midi_track()
{
	std::shared_ptr<ARDOUR::MidiTrack> mt = std::dynamic_pointer_cast<ARDOUR::MidiTrack> (stream_view->trackview().stripable());

	if (mt) {
		return mt;
	}

	return nullptr;
}
