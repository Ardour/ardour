/*
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2012 Paul Davis <paul@linuxaudiosystems.com>
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

#pragma once

#include <ytkmm/drawingarea.h>

#include "prh_base.h"

class PianoRollHeader : public Gtk::DrawingArea, public PianoRollHeaderBase {
  public:
	PianoRollHeader(MidiViewBackground&);

	bool on_expose_event (GdkEventExpose*);
	bool on_motion_notify_event (GdkEventMotion*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_scroll_event (GdkEventScroll*);
	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);

	void on_size_request(Gtk::Requisition*);
	void redraw ();
	void redraw (double x, double y, double w, double h);
	double height() const;
	double width() const;
	double event_y_to_y (double evy) const { return evy; }
	void draw_transform (double& x, double& y) const {}
	void event_transform (double& x, double& y) const {}
	void _queue_resize () { queue_resize(); }
	void do_grab() { add_modal_grab(); }
	void do_ungrab() { remove_modal_grab(); }
	Glib::RefPtr<Gdk::Window> cursor_window();
	std::shared_ptr<ARDOUR::MidiTrack> midi_track();

	void instrument_info_change ();

 private:
	MidiStreamView* stream_view;
};

