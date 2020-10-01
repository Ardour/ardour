/*
 * Copyright (C) 2013-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <algorithm>
#include <string>
#include <vector>

#include "gtkmm2ext/utils.h"

#include "ardour_ui.h"
#include "audio_clock.h"
#include "big_clock_window.h"
#include "public_editor.h"
#include "utils.h"

#include "pbd/i18n.h"

using std::min;
using std::string;
using namespace ARDOUR_UI_UTILS;

BigClockWindow::BigClockWindow (AudioClock& c)
	: ArdourWindow (_("Big Clock"))
	, clock (c)
{
	ARDOUR_UI::Clock.connect (sigc::bind (sigc::mem_fun (clock, &AudioClock::set), false, Temporal::timecnt_t()));

	clock.set_corner_radius (0.0);

	set_keep_above (true);
	set_border_width (0);
	add (clock);
	clock.show_all ();

	clock.size_request (default_size);

	clock.signal_size_allocate().connect (sigc::mem_fun (*this, &BigClockWindow::clock_size_reallocated));
}

void
BigClockWindow::on_unmap ()
{
	ArdourWindow::on_unmap ();
	ARDOUR_UI::instance()->reset_focus (this);
}

bool
BigClockWindow::on_key_press_event (GdkEventKey* ev)
{
	return relay_key_press (ev, this);
}

void
BigClockWindow::on_realize ()
{
	ArdourWindow::on_realize ();
	/* (try to) ensure that resizing is possible and the window can be moved (and closed) */
	get_window()->set_decorations (Gdk::DECOR_BORDER | Gdk::DECOR_RESIZEH | Gdk::DECOR_TITLE | Gdk::DECOR_MENU);

	/* try to force a fixed aspect ratio so that we don't distort the font */
	float aspect = default_size.width/(float)default_size.height;
	Gdk::Geometry geom;

	geom.min_aspect = aspect;
	geom.max_aspect = aspect;
	geom.min_width = -1; /* use requisition */
	geom.min_height = -1; /* use requisition */

	get_window()->set_geometry_hints (geom, Gdk::WindowHints (Gdk::HINT_ASPECT|Gdk::HINT_MIN_SIZE));
}

void
BigClockWindow::clock_size_reallocated (Gtk::Allocation& alloc)
{
	clock.set_scale ((double) alloc.get_width() / default_size.width,
			 (double)  alloc.get_height() / default_size.height);
}


