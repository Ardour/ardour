/*
    Copyright (C) 2000 Paul Davis

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

#include <cstdlib>
#include <cmath>
#include <string>

#include <gtkmm/treeview.h>

#include "pbd/error.h"

#include "ardour/session.h"

#include "ardour_ui.h"
#include "editor.h"
#include "region_view.h"
#include "selection.h"
#include "time_axis_view.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

void
Editor::keyboard_selection_finish (bool add)
{
	if (_session && have_pending_keyboard_selection) {

		framepos_t end;
		bool ignored;

		if (_session->transport_rolling()) {
			end = _session->audible_frame();
		} else {
			if (!mouse_frame (end, ignored)) {
				return;
			}
		}

		if (add) {
			selection->add (pending_keyboard_selection_start, end);
		} else {
			selection->set (pending_keyboard_selection_start, end);
		}

		have_pending_keyboard_selection = false;
	}
}

void
Editor::keyboard_selection_begin ()
{
	if (_session) {
		if (_session->transport_rolling()) {
			pending_keyboard_selection_start = _session->audible_frame();
			have_pending_keyboard_selection = true;
		} else {
			bool ignored;
			framepos_t where; // XXX fix me

			if (mouse_frame (where, ignored)) {
				pending_keyboard_selection_start = where;
				have_pending_keyboard_selection = true;
			}

		}
	}
}

void
Editor::keyboard_paste ()
{
	paste (1);
}
