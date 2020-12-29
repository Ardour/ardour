/*
 * Copyright (C) 2014-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#include <algorithm>

#include "canvas/canvas.h"
#include "canvas/tracking_text.h"

using namespace ArdourCanvas;

TrackingText::TrackingText (Canvas* c)
	: Text (c)
	, track_x (true)
	, track_y (true)
	, offset (Duple (10, 10))
{
	init ();
}

TrackingText::TrackingText (Item* p)
	: Text (p)
	, track_x (true)
	, track_y (true)
	, offset (Duple (10, 10))
{
	init ();
}

void
TrackingText::init ()
{
	_canvas->MouseMotion.connect (sigc::mem_fun (*this, &TrackingText::pointer_motion));
	set_ignore_events (true);
	set_outline (true);
	hide ();
}

void
TrackingText::pointer_motion (Duple const& winpos)
{
	if (!_visible) {
		return;
	}

	Duple pos (_parent->window_to_item (winpos));

	if (!track_x) {
		pos.x = position ().x;
	} else {
		pos.x += offset.x;
	}

	if (!track_y) {
		pos.y = position ().y;
	} else {
		pos.y += offset.y;
	}

	/* keep inside the window */

	Rect r (0, 0, _canvas->width (), _canvas->height ());

	/* border of 200 pixels on the right, and 50 on all other sides */

	const double border = 50.0;

	r.x0 += border;
	r.x1 = std::max (r.x0, (r.x1 - 200.0));
	r.y0 += border;
	r.y1 = std::max (r.y0, (r.y1 - border));

	/* clamp */

	if (pos.x < r.x0) {
		pos.x = r.x0;
	} else if (pos.x > r.x1) {
		pos.x = r.x1;
	}

	if (pos.y < r.y0) {
		pos.y = r.y0;
	} else if (pos.y > r.y1) {
		pos.y = r.y1;
	}

	/* move */

	set_position (pos);
}

void
TrackingText::show_and_track (bool tx, bool ty)
{
	track_x = tx;
	track_y = ty;

	bool was_visible = _visible;
	show ();

	if (!was_visible) {
		/* move to current pointer location. do this after show() so that
		 * _visible is true, and thus ::pointer_motion() will do
		 * something.
		 */
		Duple winpos;

		if (!_canvas->get_mouse_position (winpos)) {
			return;
		}

		pointer_motion (winpos);
	}
}

void
TrackingText::set_x_offset (double o)
{
	begin_change ();
	offset.x = o;
	end_change ();
}

void
TrackingText::set_y_offset (double o)
{
	begin_change ();
	offset.y = o;
	end_change ();
}

void
TrackingText::set_offset (Duple const& d)
{
	begin_change ();
	offset = d;
	end_change ();
}
