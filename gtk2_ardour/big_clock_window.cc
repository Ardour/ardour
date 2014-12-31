/*
    Copyright (C) 20002-2013 Paul Davis

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

#include <algorithm>
#include <string>
#include <vector>

#include "ardour_ui.h"
#include "audio_clock.h"
#include "big_clock_window.h"
#include "public_editor.h"
#include "utils.h"

#include "i18n.h"

using std::min;
using std::string;
using namespace ARDOUR_UI_UTILS;

BigClockWindow::BigClockWindow (AudioClock& c) 
	: ArdourWindow (_("Big Clock"))
	, clock (c)
	, original_height (0)
	, original_width (0)
{
	ARDOUR_UI::Clock.connect (sigc::mem_fun (clock, &AudioClock::set));

	clock.set_corner_radius (0.0);

	set_keep_above (true);
	set_border_width (0);
	add (clock);
	clock.show_all ();
}

void
BigClockWindow::on_unmap ()
{
	ArdourWindow::on_unmap ();

	PublicEditor::instance().reset_focus ();
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
	get_window()->set_decorations (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH);

	int x, y, d;
	get_window()->get_geometry (x, y, original_width, original_height, d);
}

void
BigClockWindow::on_size_allocate (Gtk::Allocation& alloc)
{
	ArdourWindow::on_size_allocate (alloc);

	if (original_width) {
		clock.set_scale ((double) alloc.get_width() / original_width,
				 (double)  alloc.get_height() / original_height);



		std::cerr << "Rescale to "
			  << (double) alloc.get_width() / original_width
			  << " x " 
			  << (double)  alloc.get_height() / original_height
			  << " using " << alloc.get_width() << " vs. " << original_width
			  << " and " << alloc.get_height() << " vs. " << original_height
			  << std::endl;

	}
}


