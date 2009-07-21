/*
    Copyright (C) 2002 Paul Davis 

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

#include "editor.h"

#include "ardour/session.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

void
Editor::hscrollbar_allocate (Gtk::Allocation &)
{
}

bool
Editor::hscrollbar_button_press (GdkEventButton *)
{
	_dragging_hscrollbar = true;
	return false;
}

bool
Editor::hscrollbar_button_release (GdkEventButton *)
{
	_dragging_hscrollbar = false;

	if (session) {

	}

	return false;
}

void
Editor::reset_hscrollbar_stepping ()
{
	horizontal_adjustment.set_step_increment ((current_page_frames() / 5)/frames_per_unit); /* 5 clicks to scroll the entire page */
	horizontal_adjustment.set_page_increment (current_page_frames()/frames_per_unit);
}
