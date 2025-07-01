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

#include <iostream>

#include "evoral/midi_events.h"

#include "canvas/canvas.h"

#include "ardour/instrument_info.h"
#include "ardour/midi_track.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"

#include "midi++/midnam_patch.h"

#include "editing.h"
#include "gui_thread.h"
#include "midi_view.h"
#include "midi_view_background.h"
#include "mouse_cursors.h"
#include "prh.h"
#include "editing_context.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtkmm2ext;

namespace ArdourCanvas {

PianoRollHeader::PianoRollHeader (Item* parent, MidiViewBackground& bg)
	: Rectangle (parent)
	, PianoRollHeaderBase (bg)
{
	Event.connect (sigc::mem_fun (*this, &PianoRollHeader::event_handler));

	alloc_layouts (_canvas->get_pango_context());

	/* draw vertical lines on both sides of the rectangle */
	set_fill (false);
	set_outline_color (0x000000ff); /* XXX theme me */
	set_outline_what (Rectangle::What (Rectangle::LEFT|Rectangle::RIGHT));

	_midi_context.HeightChanged.connect (height_connection, MISSING_INVALIDATOR, std::bind (&PianoRollHeader::resize, this), gui_context());
	resize ();
}

void
PianoRollHeader::redraw ()
{
	ArdourCanvas::Rectangle::redraw ();
}

void
PianoRollHeader::redraw (double x, double y, double w, double h)
{
	ArdourCanvas::Duple origin (x, y);
	origin = item_to_window (origin);

	dynamic_cast<ArdourCanvas::GtkCanvas*>(_canvas)->queue_draw_area (origin.x, origin.y, w, h);
}

void
PianoRollHeader::resize ()
{
	double w, h;
	size_request (w, h);
	set (Rect (0., 0., w, h));
}

void
PianoRollHeader::size_request (double& w, double& h) const
{
	h = _midi_context.contents_height();

	if (show_scroomer()) {
		_scroomer_size = 60.f * UIConfiguration::instance().get_ui_scale();
	} else {
		_scroomer_size = 20.f * UIConfiguration::instance().get_ui_scale();
	}

	w = _scroomer_size +  20.;
}

bool
PianoRollHeader::event_handler (GdkEvent* ev)
{
	GdkEvent* copy = gdk_event_copy (ev);
	Duple evd;

	/* Remember that ev uses canvas coordinates, not item */

	switch (ev->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		evd =  (canvas_to_item (Duple (ev->button.x, ev->button.y)));
		copy->button.x = evd.x;
		copy->button.y = evd.y;
		return button_press_handler (&copy->button);

	case GDK_BUTTON_RELEASE:
		evd =  (canvas_to_item (Duple (ev->button.x, ev->button.y)));
		copy->button.x = evd.x;
		copy->button.y = evd.y;
		return button_release_handler (&copy->button);

	case GDK_ENTER_NOTIFY:
		evd =  (canvas_to_item (Duple (ev->crossing.x, ev->crossing.y)));
		copy->crossing.x = evd.x;
		copy->crossing.y = evd.y;
		return enter_handler (&copy->crossing);

	case GDK_LEAVE_NOTIFY:
		evd =  (canvas_to_item (Duple (ev->crossing.x, ev->crossing.y)));
		copy->crossing.x = evd.x;
		copy->crossing.y = evd.y;
		return leave_handler (&copy->crossing);

	case GDK_SCROLL:
		evd =  (canvas_to_item (Duple (ev->scroll.x, ev->scroll.y)));
		copy->scroll.x = evd.x;
		copy->scroll.y = evd.y;
		return scroll_handler (&copy->scroll);

	case GDK_MOTION_NOTIFY:
		evd =  (canvas_to_item (Duple (ev->motion.x, ev->motion.y)));
		copy->motion.x = evd.x;
		copy->motion.y = evd.y;
		return motion_handler (&copy->motion);

	default:
		break;
	}

	gdk_event_free (copy);

	return false;
}

double
PianoRollHeader::height() const
{
	return get().height();
}

double
PianoRollHeader::width() const
{
	return get().width();
}

void
PianoRollHeader::render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rect self (get());
	Rectangle::render (area, context);
	PianoRollHeaderBase::render (self, area, context);
}

double
PianoRollHeader::event_y_to_y (double evy) const
{
	Duple evd (canvas_to_item (Duple (0., evy)));
	return evd.y;
}

void
PianoRollHeader::draw_transform (double& x, double& y) const
{
	Duple d (x, y);
	d = item_to_window (d);
	x = d.x;
	y = d.y;
}

void
PianoRollHeader::event_transform (double& x, double& y) const
{
	Duple d (x, y);
	d = canvas_to_item (d);
	x = d.x;
	y = d.y;
}

Glib::RefPtr<Gdk::Window>
PianoRollHeader::cursor_window()
{
	ArdourCanvas::GtkCanvas* gc (_midi_context.editing_context().get_canvas());
	assert (gc);
	return gc->get_window ();
}

std::shared_ptr<ARDOUR::MidiTrack>
PianoRollHeader::midi_track()
{
	if (_view) {
		return _view->midi_track ();
	}

	return nullptr;
}


} // namespace
