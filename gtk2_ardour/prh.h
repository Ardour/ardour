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

#include "canvas/rectangle.h"

#include "prh_base.h"

namespace ArdourCanvas {

class PianoRollHeader : public ArdourCanvas::Rectangle, public PianoRollHeaderBase {
  public:
	PianoRollHeader (ArdourCanvas::Item* parent, MidiViewBackground&);

	void size_request (double& w, double& h) const;
	void resize ();
	void redraw ();
	void redraw (double x, double y, double w, double h);
	double height() const;
	double width() const;
	double event_y_to_y (double evy) const;
	void draw_transform (double& x, double& y) const;
	void event_transform (double& x, double& y) const;
	void _queue_resize () { queue_resize(); }
	void do_grab() { ArdourCanvas::Rectangle::grab(); }
	void do_ungrab() { ArdourCanvas::Rectangle::ungrab(); }
	Glib::RefPtr<Gdk::Window> cursor_window();
	std::shared_ptr<ARDOUR::MidiTrack> midi_track();

	void render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context>) const;


 private:
	PBD::ScopedConnection height_connection;
	bool event_handler (GdkEvent*);
};

}

